# Hardware-to-Backend API Contract
**Endpoint:** `POST /api/v1/device/scans`
**Source of truth:** `hostelhive-backend/docs/attendance-scans.md` and `docs/attendance-permission-checks.md`
**Reference:** HostelHive SRS (FR011, FR013) & SDS Section 6.3 (Hardware Security)

## Transport
HTTPS only. The device checks the server certificate against the root CA in `API_ROOT_CA`. Redirects are not followed, so the `Authorization` header is never sent to another host. Plain HTTP is available only in the simulation build with `ALLOW_INSECURE_HTTP 1`.

## Request Headers
| Header | Value | Description |
|:---|:---|:---|
| `Authorization` | `Bearer hhd1.<device-uuid>.<43-char-secret>` | Per-device credential issued once by `POST /api/v1/scanner-devices` (or `rotate-credential`). The backend stores only its SHA-256. |
| `Content-Type` | `application/json` | Required (415 otherwise) |

## Request Payload
```json
{
  "event_id": "3f2a8c4e-1b6d-4e9a-9c7f-2d5e8a1b4c6f",
  "qr": "HH1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```
* `event_id`: a random UUIDv4 generated on the device (hardware RNG) for each physical scan. A network retry reuses the same ID, and the backend returns the original result for it.
* `qr`: the scanned pass exactly as issued. The device rejects anything that isn't `HH1:` + 64 lowercase hex characters before sending.
* Unknown fields are rejected (400). Direction is not sent; the backend uses the direction an admin configured for this device.

## Responses and gate behaviour
| HTTP | Body | Gate |
|:---|:---|:---|
| 200 | Event with `outcome` = `recorded`, `cooldown`, `already_in` or `already_out` | **Opens** |
| 200 | `access_result: "unapproved"` (restricted hours, no approved late-access request) | Opens, display shows "LATE - REPORTED"; the backend creates an incident |
| 200 | Missing or unknown `outcome` / unparseable body | Locked |
| 422 | `{"error":"active_student_qr_required"}` (unknown, inactive or archived student) | Locked: "INVALID / INACTIVE PASS" |
| 409 | `{"error":"scanner_direction_required"}` | Locked: "GATE NOT CONFIGURED" |
| 409 | `{"error":"event_id_conflict"}` | Locked: "SCAN REJECTED" |
| 401 | `{"error":"unauthorized"}` (bad, rotated or deactivated credential) | Locked: "GATE NOT AUTHORIZED" |
| 400 / 413 / 415 | Malformed request | Locked: "SCAN REJECTED" |
| 5xx, timeout, no Wi-Fi | — | Locked: "OFFLINE - LOCKED, contact security" |

Example 200 body:
```json
{
  "event_id": "3f2a8c4e-1b6d-4e9a-9c7f-2d5e8a1b4c6f",
  "device_id": "9b1c...",
  "student_id": "5d7e...",
  "direction": "IN",
  "outcome": "recorded",
  "occurred_at": "2026-10-25T14:03:11Z",
  "access_result": "not_required"
}
```

## Startup check
`GET /api/v1/device/check` with the same `Authorization` header returns 200 `{"device_id","name","status":"authenticated"}`. The firmware calls it at boot and logs the result.

## Manual overrides
The backend has no override endpoint yet. Overrides are appended to `/override_log.jsonl` on the device's flash (with sequence number, node, time and uptime) and printed to serial. Send `overrides` over the USB console to dump the log.
