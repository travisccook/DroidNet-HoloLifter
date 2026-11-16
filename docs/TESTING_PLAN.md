# HoloLifter Testing Plan

Three levels, from no hardware to the lift in the droid. Command details are
in docs/SERIAL_COMMANDS.md; where this page and that one differ, that one is
right.

## 1. Native unit tests (no hardware)

The pure C++ libraries are tested on the computer with PlatformIO's native
environment and Unity:

    pio test -e native                 every suite
    pio test -e native -f "test_wcb_*"   one family (quote the pattern)

    SUITE                 COVERS
    -------------------   ----------------------------------------------
    test_lift_commands    #HL parser and reply formatting
    test_motion_profile   speed ramp before the stop point, margin load
    test_node_config      #HLZ commands, "mesh ..." console, validation
    test_maint_core       maintenance-mode state machine, OTA validity gate
    test_power_cycle      three short power cycles enter maintenance mode
                          (shared logic; only the HoloFan uses it)
    test_wcb_*            WCB mesh client: packets, CRC, peers, WDP,
                          retries, sequence window, send queue, RTERM

test/i2c_scanner is an on-hardware sketch and is never built natively.

The tools have Python tests (tools/test_hlcon.py with a fake serial port,
tools/test_check_private.py):

    python3 -B -m unittest discover -s tools -p 'test_*.py'

## 2. Bench tests over USB (lift out of the droid)

USB console at 115200 baud (PlatformIO monitor, screen, or tools/hlcon.py).
tools/hlcon.py resets the board when it opens the port unless it is given
--noreset: homing is lost and the relay drops.
Console words: help, status, limits, encoder, pos, home, up, down, goto,
stop, reset, speed, margin, relay; `hl <#HL...>` runs a mesh command locally
and prints its reply.

Do these in order. Keep a hand near the power: the limit switches are the
safety, and at full speed the lift can coast onto them.

    STEP  WHAT                                          EXPECT
    ----  --------------------------------------------  ---------------------------------------
    B1    Power on                                      no movement (never homes on power-up)
    B2    limits, press each switch by hand             upper/lower change 0 -> 1 and back
    B3    encoder, turn the motor shaft by hand          count changes, sign matches direction
    B4    speed 60, then home                           up to the upper switch, down to the
                                                        lower one, then homed=1
    B5    up 500 / down 500 (relative moves)            moves about 500 counts each way
    B6    goto <mid-travel>, stop during the move        stops at once, state STOPPED
    B7    up, then down (full travel, slow speed)       stops short of each switch (margin)
    B8    pull a limit-switch wire while moving         motor stops (open switch = triggered)
    B9    relay on / relay off                          relay clicks, fan power on / off
    B10   stop while the relay is on                    lift stops and fan power goes off
    B11   speed 255 only after B1-B10 pass              same results at full speed

## 3. Mesh tests (through a WCB)

With the node's mesh password, MAC octets and channel set to your mesh's
values (README, "First setup"), from a droid controller or the WCB's own
serial console:

    STEP  SEND                          EXPECT
    ----  ----------------------------  -------------------------------------------------
    M1    ?WDP,LIST on a WCB            the node is listed a few seconds after it boots
    M2    ;wHoloLifter,#HLQ             #HL:STATUS ... on every WCB broadcast-out port
    M3    ;w15,#HLQ                     the same reply (by node id)
    M4    ;wHoloLifter,#HLU / #HLS      moves / stops; actions never reply
    M5    ;wHoloLifter,#HLZP0, #HLQ     no reply; #HLZP1 turns replies back on
    M6    ;wHoloLifter,#HLF1 / #HLF0    fan power on / off
    M7    ;wHoloLifter,#HLZQ            #HL:NODE ... with the link counters

Replies are best-effort broadcasts: if one is missing, ask again before
assuming a fault.

## 4. Maintenance mode and over-the-air update

Follow docs/MAINTENANCE_API.md: store a Wi-Fi profile, enter maintenance
mode, check that the node answers on Wi-Fi, upload a new image and confirm
that it returns to the mesh. A new image that cannot reach the mesh within
30 s of booting rolls back to the previous one on its own.

## 5. Firmware 1.0.2: not yet tried on hardware

1.0.2 (code-review fixes) has passed the native and Python tests only. It
has not been compiled for the ESP32 or run on the lift. Until each step
below has been done on the lift, treat its EXPECT column as what the code
is meant to do, not as a result. Start with `pio run`, then B1-B10 again.

    STEP  WHAT                                          EXPECT
    ----  --------------------------------------------  ---------------------------------------
    N1    terminal sending CRLF (pio device monitor):   each runs once, with its argument
          speed 80, hl #HLQ                             (before 1.0.2 the argument was lost)
    N2    two lines sent in one write                   both run, in order
    N3    up 0, up -100, up abc, goto 12x, speed abc    "ERROR Usage: ..." each, no movement
    N4    hl #hlq; hl #HLZKS1=My Home, then mesh        a reply; the SSID keeps its case and
                                                        space
    N5    homed, resting on the lower switch: up        leaves the switch, no "Motor stalled"
    N6    the same after `disable`                      ERROR "Motor stalled" after about 0.5 s
    N7    speed 50, full travel up, then down           each ends inside 30 s; if it ends in
                                                        ERROR "Movement timeout", the 30 s
                                                        limit is too short for this lift
    N8    cause an ERROR (N6), then stop                state stays ERROR, up is refused;
                                                        clear, then up works
    N9    power up with the Qwiic cable unplugged       "Motor driver not found", then the
                                                        console and the mesh start; status
                                                        ERROR; home, up and clear refused;
                                                        relay off
    N10   not homed: #HLU, #HLU again while homing      homing is not started again; the lift
                                                        goes up once it is done
    N11   #HLS, then #HLU at once, several times        every #HLU moves the lift (a WCB's
                                                        retry of the #HLS must not stop it)
    N12   disable, mesh maint 1, POST /api/done         the motor driver stays disabled
    N13   enable, mesh maint 1, POST /api/done          the motor driver is enabled again
    N14   OTA with a WCB switched on                    "[ota] new image marked valid" once a
                                                        mesh frame has arrived (a WCB's
                                                        heartbeat: about every 10 s)
    N15   OTA with every WCB switched off               "[ota] ... rolling back" after 30 s;
                                                        version shows the old firmware
    N16   maintenance and back, several times           on the mesh again each time; the line
                                                        "[mesh] radio is down; restarting it"
                                                        means the 5 s retry had to step in
