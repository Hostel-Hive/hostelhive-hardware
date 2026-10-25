"""Local HTTP -> HTTPS relay for the Wokwi simulator only.

The simulated ESP32 cannot finish a TLS handshake with the hosted backend
fast enough (the simulator runs below real time and the server drops slow
handshakes), so the simulator talks plain HTTP to this relay on your own
computer and the relay forwards to the backend over verified HTTPS.

Usage:
    python simulation/https_relay.py [--backend https://...] [--port 8080]

Then in include/secrets.h (simulation build only):
    #define API_BASE_URL "http://host.wokwi.internal:8080"
    #define ALLOW_INSECURE_HTTP 1

Never use this for a physical gate.
"""
import argparse
import http.server
import ssl
import urllib.error
import urllib.request

DEFAULT_BACKEND = "https://hostelhive-backend-dev.onrender.com"
# Only the scanner endpoints are relayed; anything else gets 404.
ALLOWED_PREFIX = "/api/v1/device/"
FORWARD_HEADERS = ("Authorization", "Content-Type")


class NoRedirect(urllib.request.HTTPRedirectHandler):
    # Following a redirect could send the device credential to another host.
    def redirect_request(self, *args, **kwargs):
        return None


def make_handler(backend, opener):
    class RelayHandler(http.server.BaseHTTPRequestHandler):
        def _relay(self):
            if not self.path.startswith(ALLOWED_PREFIX):
                self.send_error(404, "only scanner endpoints are relayed")
                return
            length = int(self.headers.get("Content-Length") or 0)
            body = self.rfile.read(length) if length else None
            req = urllib.request.Request(backend + self.path, data=body, method=self.command)
            for name in FORWARD_HEADERS:
                if self.headers.get(name):
                    req.add_header(name, self.headers[name])
            try:
                resp = opener.open(req, timeout=90)  # Render cold start can take ~60 s
                status, payload, ctype = resp.status, resp.read(), resp.headers.get("Content-Type")
            except urllib.error.HTTPError as e:
                status, payload, ctype = e.code, e.read(), e.headers.get("Content-Type")
            except (urllib.error.URLError, TimeoutError) as e:
                self.log_message("backend unreachable: %s", e)
                self.send_error(502, "backend unreachable")
                return
            self.send_response(status)
            if ctype:
                self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(payload)

        do_GET = _relay
        do_POST = _relay

    return RelayHandler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--backend", default=DEFAULT_BACKEND)
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    if not args.backend.startswith("https://"):
        parser.error("--backend must be https://")

    opener = urllib.request.build_opener(
        urllib.request.HTTPSHandler(context=ssl.create_default_context()), NoRedirect())
    # Loopback only: the Wokwi gateway connects from this machine.
    server = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(args.backend.rstrip("/"), opener))
    print(f"Relaying http://127.0.0.1:{args.port}{ALLOWED_PREFIX}* -> {args.backend}  (Ctrl+C to stop)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
