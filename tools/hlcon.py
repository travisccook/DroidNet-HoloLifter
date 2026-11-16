#!/usr/bin/env python3
"""Send console lines to the HoloLifter over USB and capture the replies.

    hlcon.py PORT OUTFILE [--boot SECONDS] [--wait SECONDS] [--noreset] [-- CMD ...]

    PORT            the lift's USB serial port (115200 baud)
    OUTFILE         the capture: everything the lift printed, with a '>>> CMD'
                    line before each command; also printed at the end
    --boot SECONDS  listen this long before the first command (default 4)
    --wait SECONDS  listen this long after each command (default 1.5)
    --noreset       do not reset the lift (see below)
    -- CMD ...      console lines, sent in order, each ended with CR. A CMD of
                    the form sleep:N sends nothing and listens for N seconds.

By default the tool pulses the board's EN line through RTS when it opens the
port, which REBOOTS the lift: the homing state is lost (the next up, down or
goto homes first) and the relay goes back to its power-up state, so the
holofan's power changes. Use --noreset to talk to a lift that must keep
running; opening a serial port can still reset some boards by itself.

The capture is written while it runs and closed on any exit, so an error or
Ctrl-C keeps what was received. Exit 0 done, 1 serial error or interrupted,
2 usage.
"""
import sys
import time

BAUD = 115200


class UsageError(Exception):
    pass


def seconds(option, text):
    try:
        value = float(text)
    except ValueError:
        value = -1.0
    if not 0 <= value <= 3600:              # also refuses nan and inf
        raise UsageError(f"{option} needs a number of seconds from 0 to 3600, not {text!r}")
    return value


def parse_args(argv):
    """-> dict(port, out, boot, wait, reset, cmds); raises UsageError."""
    if len(argv) < 2 or argv[0].startswith("-") or argv[1].startswith("-"):
        raise UsageError("PORT and OUTFILE come first")
    cfg = {"port": argv[0], "out": argv[1], "boot": 4.0, "wait": 1.5, "reset": True, "cmds": []}
    i = 2
    while i < len(argv):
        a = argv[i]
        if a == "--":
            cfg["cmds"] = list(argv[i + 1:])
            break
        if a in ("--boot", "--wait"):
            if i + 1 >= len(argv):
                raise UsageError(f"{a} needs a number of seconds")
            cfg[a[2:]] = seconds(a, argv[i + 1])
            i += 2
        elif a == "--noreset":
            cfg["reset"] = False
            i += 1
        elif a.startswith("-"):
            raise UsageError(f"unknown option {a}")
        else:
            raise UsageError(f"{a!r}: commands come after '--'")
    for c in cfg["cmds"]:
        if c.startswith("sleep:"):
            seconds("sleep:N", c[6:])
        elif "\r" in c or "\n" in c:
            raise UsageError("a command is one line: no CR or LF inside it")
    return cfg


def run(cfg, serial_module, out, sleep=time.sleep, clock=time.time):
    """Open the port, optionally reset, send the commands; every received or
    sent line goes to out.write(). The port is closed on any exit."""
    s = serial_module.Serial()
    s.port, s.baudrate, s.timeout = cfg["port"], BAUD, 0.1
    s.dtr = False
    s.rts = False                           # do not hold EN/IO0 on open
    s.open()
    try:
        if cfg["reset"]:
            s.dtr = False
            s.rts = True                    # EN low: the lift reboots
            sleep(0.15)
            s.rts = False

        def drain(sec):
            end = clock() + sec
            while clock() < end:
                d = s.read(4096)
                if d:
                    out.write(d.decode("utf-8", "replace"))

        drain(cfg["boot"])
        for c in cfg["cmds"]:
            if c.startswith("sleep:"):
                out.write(f"\n>>> (waiting {c[6:]} s)\n")
                drain(float(c[6:]))
                continue
            out.write(f"\n>>> {c}\n")
            s.write((c + "\r").encode())
            s.flush()
            drain(cfg["wait"])
    finally:
        s.close()


class Capture:
    """Collects the capture and writes it to the file as it arrives."""

    def __init__(self, f):
        self.f, self.parts = f, []

    def write(self, text):
        self.parts.append(text)
        self.f.write(text)
        self.f.flush()


def main(argv, serial_module=None):
    try:
        cfg = parse_args(argv)
    except UsageError as e:
        print(f"error: {e}\n\n{__doc__}", file=sys.stderr)
        return 2
    if serial_module is None:
        try:
            import serial as serial_module  # pyserial
        except ImportError:
            print("error: pyserial is not installed (pip install pyserial)", file=sys.stderr)
            return 1
    status = 0
    try:
        f = open(cfg["out"], "w", encoding="utf-8")
    except OSError as e:
        print(f"error: cannot write {cfg['out']}: {e.strerror}", file=sys.stderr)
        return 1
    capture = Capture(f)
    try:
        run(cfg, serial_module, capture)
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        status = 1
    except (OSError, getattr(serial_module, "SerialException", OSError)) as e:
        print(f"error: {e}", file=sys.stderr)
        status = 1
    finally:
        f.close()
        print("".join(capture.parts))
        if status:
            print(f"the capture so far is in {cfg['out']}", file=sys.stderr)
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
