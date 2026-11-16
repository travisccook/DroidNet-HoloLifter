#!/usr/bin/env python3
"""Tests for hlcon.py with the serial module stubbed: no port is opened and
nothing is sent to hardware.
Run: python3 -B -m unittest discover -s tools -p 'test_hlcon.py' -v"""
import contextlib
import io
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hlcon  # noqa: E402


class FakeSerial:
    """Stands in for serial.Serial: records what the tool does to the port."""
    replies = [b"boot\r\n"]
    fail_write = None

    def __init__(self):
        self.events, self.is_open = [], False
        self._rts = self._dtr = None
        self.pending = list(self.replies)
        FakeSerial.last = self

    rts = property(lambda self: self._rts, lambda self, v: self._set("rts", v))
    dtr = property(lambda self: self._dtr, lambda self, v: self._set("dtr", v))

    def _set(self, name, value):
        setattr(self, "_" + name, value)
        self.events.append((name, value))

    def open(self):
        self.is_open = True
        self.events.append(("open", self.port, self.baudrate))

    def read(self, n):
        return self.pending.pop(0) if self.pending else b""

    def write(self, data):
        if self.fail_write is not None:
            raise self.fail_write
        self.events.append(("write", data))
        self.pending.append(b"ok " + data.strip() + b"\r\n")

    def flush(self):
        pass

    def close(self):
        self.is_open = False
        self.events.append(("close",))


class FakeSerialModule:
    Serial = FakeSerial

    class SerialException(Exception):
        pass


class ParseArgs(unittest.TestCase):
    def usage_error(self, *argv):
        with self.assertRaises(hlcon.UsageError) as c:
            hlcon.parse_args(list(argv))
        return str(c.exception)

    def test_defaults(self):
        self.assertEqual(hlcon.parse_args(["PORT", "out.txt", "--", "status", "sleep:2"]),
                         {"port": "PORT", "out": "out.txt", "boot": 4.0, "wait": 1.5, "reset": True,
                          "cmds": ["status", "sleep:2"]})

    def test_options(self):
        cfg = hlcon.parse_args(["PORT", "o", "--noreset", "--boot", "0", "--wait", "0.25", "--", "home"])
        self.assertEqual((cfg["boot"], cfg["wait"], cfg["reset"], cfg["cmds"]), (0.0, 0.25, False, ["home"]))

    def test_no_separator_means_no_commands_and_returns(self):
        # The old loop never ended here.
        self.assertEqual(hlcon.parse_args(["PORT", "o"])["cmds"], [])
        self.assertEqual(hlcon.parse_args(["PORT", "o", "--noreset"])["cmds"], [])
        self.assertEqual(hlcon.parse_args(["PORT", "o", "--"])["cmds"], [])

    def test_everything_after_the_separator_is_a_command(self):
        self.assertEqual(hlcon.parse_args(["PORT", "o", "--", "--noreset", "--"])["cmds"], ["--noreset", "--"])

    def test_unknown_option_is_an_error(self):
        # So did this one.
        self.assertIn("unknown option --bogus", self.usage_error("PORT", "o", "--bogus", "--", "status"))
        self.assertIn("unknown option -x", self.usage_error("PORT", "o", "-x"))

    def test_command_before_the_separator_is_an_error(self):
        self.assertIn("after '--'", self.usage_error("PORT", "o", "status"))

    def test_missing_port_or_outfile(self):
        for argv in ([], ["PORT"], ["--noreset", "o"], ["PORT", "--", "status"]):
            self.usage_error(*argv)

    def test_bad_numbers(self):
        self.assertIn("--boot needs", self.usage_error("PORT", "o", "--boot"))
        for bad in ("x", "-1", "nan", "inf", ""):
            self.usage_error("PORT", "o", "--wait", bad)
            self.usage_error("PORT", "o", "--", "sleep:" + bad)

    def test_command_with_a_line_end_is_an_error(self):
        self.usage_error("PORT", "o", "--", "up\rstop")
        self.usage_error("PORT", "o", "--", "up\n")


class Main(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.out = os.path.join(self.tmp.name, "capture.txt")
        FakeSerial.fail_write = None
        FakeSerial.last = None

    def tearDown(self):
        self.tmp.cleanup()

    def main(self, *argv):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = hlcon.main(list(argv), FakeSerialModule)
        return status, stdout.getvalue(), stderr.getvalue()

    def capture(self):
        with open(self.out, newline="") as f:
            return f.read()

    def test_usage_error_opens_nothing_and_documents_noreset(self):
        status, _, err = self.main("PORT", self.out, "--bogus")
        self.assertEqual(status, 2)
        self.assertIsNone(FakeSerial.last)
        self.assertFalse(os.path.exists(self.out))
        self.assertIn("--noreset", err)
        self.assertIn("REBOOTS", err)

    def test_default_pulses_en_once_and_sends_each_command_with_cr(self):
        status, out, _ = self.main("PORT", self.out, "--boot", "0.05", "--wait", "0.05", "--", "status", "home")
        self.assertEqual(status, 0)
        ev = FakeSerial.last.events
        self.assertEqual(ev[:3], [("dtr", False), ("rts", False), ("open", "PORT", 115200)])
        self.assertEqual([e for e in ev if e[0] == "rts"], [("rts", False), ("rts", True), ("rts", False)])
        self.assertEqual([e[1] for e in ev if e[0] == "write"], [b"status\r", b"home\r"])
        self.assertEqual(ev[-1], ("close",))
        text = self.capture()
        for part in ("boot", ">>> status", "ok status", ">>> home", "ok home"):
            self.assertIn(part, text)
        self.assertIn(text, out)

    def test_noreset_never_raises_rts(self):
        status, _, _ = self.main("PORT", self.out, "--noreset", "--boot", "0.02", "--wait", "0.02", "--", "status")
        self.assertEqual(status, 0)
        self.assertNotIn(("rts", True), FakeSerial.last.events)
        self.assertNotIn(("dtr", True), FakeSerial.last.events)

    def test_sleep_sends_nothing(self):
        self.main("PORT", self.out, "--boot", "0.02", "--", "sleep:0.02")
        self.assertEqual([e for e in FakeSerial.last.events if e[0] == "write"], [])
        self.assertIn("(waiting 0.02 s)", self.capture())

    def test_capture_survives_a_serial_error(self):
        FakeSerial.fail_write = FakeSerialModule.SerialException("device disconnected")
        status, out, err = self.main("PORT", self.out, "--boot", "0.05", "--wait", "0.02", "--", "status")
        self.assertEqual(status, 1)
        self.assertIn("boot", self.capture())
        self.assertIn(">>> status", self.capture())
        self.assertIn("boot", out)
        self.assertIn("device disconnected", err)
        self.assertEqual(FakeSerial.last.events[-1], ("close",))

    def test_capture_survives_ctrl_c(self):
        FakeSerial.fail_write = KeyboardInterrupt()
        status, _, err = self.main("PORT", self.out, "--boot", "0.05", "--wait", "0.02", "--", "status")
        self.assertEqual(status, 1)
        self.assertIn("boot", self.capture())
        self.assertIn("interrupted", err)
        self.assertEqual(FakeSerial.last.events[-1], ("close",))

    def test_unwritable_outfile_fails_before_the_port_is_touched(self):
        status, _, err = self.main("PORT", os.path.join(self.tmp.name, "no", "dir", "x.txt"), "--", "status")
        self.assertEqual(status, 1)
        self.assertIsNone(FakeSerial.last)
        self.assertIn("cannot write", err)


if __name__ == "__main__":
    unittest.main()
