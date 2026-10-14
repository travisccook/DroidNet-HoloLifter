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

## 2. Bench tests over USB (lift out of the droid)

USB console at 115200 baud (PlatformIO monitor, screen, or tools/hlcon.py).
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
values (README, "Mesh setup"), from a droid controller or the WCB's own
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
