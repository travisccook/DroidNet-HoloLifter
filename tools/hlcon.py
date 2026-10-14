#!/usr/bin/env python3
"""Send console lines to the HoloLifter over USB and capture replies.
Usage: hlcon.py PORT OUTFILE [--boot SECONDS] [--wait SECONDS] -- cmd1 cmd2 ...
A command of the form 'sleep:N' waits N seconds while still capturing."""
import sys, time, serial
args = sys.argv[1:]
port, out = args[0], args[1]
boot, wait, reset = 4.0, 1.5, True
i = 2
while args[i] != '--':
    if args[i] == '--boot': boot = float(args[i+1]); i += 2
    elif args[i] == '--wait': wait = float(args[i+1]); i += 2
    elif args[i] == '--noreset': reset = False; i += 1
cmds = args[i+1:]
s = serial.Serial()
s.port = port; s.baudrate = 115200; s.timeout = 0.1
s.dtr = False; s.rts = False          # do not hold EN/IO0 on open
s.open()
if reset:
    s.dtr = False; s.rts = True; time.sleep(0.15); s.rts = False   # clean EN reset pulse
log = []
def drain(sec):
    end = time.time() + sec
    while time.time() < end:
        d = s.read(4096)
        if d: log.append(d.decode('utf-8', 'replace'))
drain(boot)
for c in cmds:
    if c.startswith('sleep:'):
        log.append(f'\n>>> (waiting {c[6:]} s)\n'); drain(float(c[6:])); continue
    log.append(f'\n>>> {c}\n')
    s.write((c + '\r').encode()); s.flush()
    drain(wait)
s.close()
open(out, 'w').write(''.join(log))
print(''.join(log))
