"""Write a student's HostelHive pass to an RFID card through the NodeMCU gate.

    python tools/provision.py HH1:<64 lowercase hex> [COM5]
    python tools/provision.py wipe [COM5]

Sends the "provision" (or "wipe") serial command to the running firmware and
waits for the card to be tapped. The board is not reset or reflashed. Close any
other serial monitor first: only one program can hold the port.
"""
import re
import sys
import time

import serial
from serial.tools import list_ports

PASS_RE = re.compile(r"HH1:[0-9a-f]{64}")
USB_SERIAL_VIDS = {0x1A86, 0x10C4, 0x0403}  # CH340, CP210x, FTDI
WAIT_SECONDS = 25  # firmware gives up after 20 s

DONE = ("Pass written to card", "HostelHive data erased")
FAILED = ("No card tapped in time", "Usage:", "RC522 not responding")


def find_port():
    ports = [p for p in list_ports.comports() if p.vid in USB_SERIAL_VIDS]
    if len(ports) == 1:
        return ports[0].device
    if not ports:
        sys.exit("No NodeMCU found. Plug it in, or give the port: provision <pass> COM5")
    sys.exit("Several USB serial ports found (%s). Give the port: provision <pass> COM5"
             % ", ".join(p.device for p in ports))


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        sys.exit(__doc__)
    arg = sys.argv[1].strip()
    port = sys.argv[2] if len(sys.argv) > 2 else find_port()

    if arg.lower() == "wipe":
        command, label = "wipe", "erase HostelHive data from"
    elif PASS_RE.fullmatch(arg):
        command, label = "provision " + arg, "write %s... to" % arg[:8]
    else:
        sys.exit("Not a HostelHive pass. Expected HH1: followed by 64 lowercase hex characters\n"
                 "(the text inside the student's QR code).")

    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0.2
    # Keep DTR/RTS released so opening the port doesn't reset the NodeMCU.
    s.dtr = False
    s.rts = False
    try:
        s.open()
    except serial.SerialException as e:
        sys.exit("Could not open %s: %s\nClose the Serial Monitor (Arduino IDE or monitor.cmd) and try again."
                 % (port, e))

    with s:
        s.reset_input_buffer()
        s.write((command + "\n").encode())
        print("Sent to %s. Tap the card on the reader now to %s it (20 s)." % (port, label))

        buf = ""
        deadline = time.time() + WAIT_SECONDS
        while time.time() < deadline:
            buf += s.read(512).decode("utf-8", errors="replace")
            while "\n" in buf:
                line, buf = buf.split("\n", 1)
                line = line.strip()
                if "[RFID]" not in line and "[GATE]" not in line:
                    continue
                print("  " + line)
                if any(m in line for m in DONE):
                    print("Done.")
                    return 0
                if any(m in line for m in FAILED):
                    return 1
        print("No answer from the gate. Is the HostelHive firmware running on %s?" % port)
        return 1


if __name__ == "__main__":
    sys.exit(main())
