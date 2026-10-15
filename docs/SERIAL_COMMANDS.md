# HoloLifter command reference (#HL over the WCB mesh)

The HoloLifter and the HoloFan are both nodes on the droid's WCB ESP-NOW mesh,
and a droid controller reaches them through any WCB. This reference covers the
HoloLifter (node 15) and the #HLZ node commands both nodes share; the fan's
own commands are documented in DroidNet-HoloFan.

## Overview

    Prefix          #HL (HashTag HoloLifter)
    Transport       WCB ESP-NOW mesh (ETM, CRC on), via any WCB serial port
    Terminator      \r on the controller's serial line (the WCB strips it)
    Encoding        ASCII, case-sensitive (#HLU, not #hlu)
    Nodes           HoloLifter  default id 15, alias "HoloLifter"
                    HoloFan     default id 16, alias "HoloFan"

## Addressing a node from a droid controller

Write a WCB command on any WCB serial port:

    ;wHoloLifter,#HLG5000\r     by alias (the node's WDP device name)
    ;w15,#HLG5000\r             by node id

The WCB unicasts the text after the comma to that node with ETM (ACK and
retry). Both forms work once the WCB has heard the node's WDP advert (a few
seconds after the node boots).

## Replies: only queries answer

Action commands (move, stop, set speed, relay, play, ...) never reply, not
even on error. Query replies are BROADCAST on the mesh as plain text, so any
reader can see them: every WCB writes the line to each of its serial ports
that has broadcast output on (?BCAST,OUT,S<n>,ON on the WCB; S0 is its USB,
which is how DroidNet reads them). Ports owned by an HCR, Maestro, MP3 or
DFPlayer are skipped by the WCB.

    controller -> WCB1:            ;wHoloLifter,#HLQ\r
    HoloLifter -> all WCBs:        #HL:STATUS state=IDLE pos=0 ...
    each WCB -> its BCAST-out ports: #HL:STATUS state=IDLE pos=0 ...\r

Replies are on by default; #HLZP0 silences them, #HLZP1 turns them back on.
A reply is sent once (best effort, never retried); if it is lost, ask again.
Each WCB still ACKs the broadcast frame, which the node simply ignores.
Replies never start with { and never contain ^ (both are special to WCBs).

## HoloLifter commands (node 15)

Motion (silent):

    #HLU            move up to the upper soft limit (slowing down before it)
                    or until a stop; if not homed yet, runs homing first and
                    then continues up (#HLS during that homing cancels both)
    #HLD            move down to the lower soft limit (slowing down before it)
                    or until a stop; if not homed yet, no homing: it runs at
                    the homing-down speed until the lower limit switch stops
                    it, zeroes the position there and stays un-homed
    #HLS            STOP now; also cuts holofan power (fail-safe). Handled
                    ahead of any queued command; moves (U, D, H, G) that
                    were queued before it are discarded.
    #HLH            run homing
    #HLG<pos>       go to absolute position (encoder counts, may be negative);
                    if not homed yet, runs homing first and then goes to
                    <pos>, checked against the soft limits homing just set
                    (out of range: refused with an ERROR line, the lift stays
                    put); #HLS during that homing cancels both
    #HLX            clear an ERROR (e.g. after a stall); keeps homing

Configuration (silent; speed and margin auto-save):

    #HLV<0-255>     speed, clamped to 50-255 (below 50 the lift stalls uphill)
    #HLM<n>         soft-limit margin in counts (default 40): the lift stops
                    n counts short of each limit switch, and gotos outside
                    that range are refused. Takes effect at once, no re-home.
                    Range 40 to travel/2 (40-500 before homing); out of
                    range it is ignored (USB `margin n` prints an ERROR), and
                    a stored value out of range is clamped on load.
                    (Before config v2 the margin was applied twice; a stored
                    v1 margin is doubled on load to keep the same stops.)
    #HLON / #HLOF   motor driver enable / disable
    #HLSAVE         save config to flash
    #HLLOAD         reload config from flash
    #HLCFGRST       reset config to defaults
    #HLR            reboot (stops the lift first; homing is lost)

Holofan power (silent):

    #HLF1 / #HLF0   holofan relay on / off

Queries (reply):

    #HLQ  -> #HL:STATUS state=<s> pos=<n> target=<n> homed=<0|1> speed=<n> limits=<u>,<l> fan=<0|1>
             states: IDLE HOMING MOVING MOVING_UP MOVING_DOWN STOPPED ERROR
             in ERROR an extra field error=<message_with_underscores> follows
    #HLP  -> #HL:POS <n>
    #HLL  -> #HL:LIMITS upper=<0|1> lower=<0|1>
    #HLE  -> #HL:ENC <raw encoder count>
    #HLI  -> #HL:INFO ver=<firmware> board=ESP32ThingPlus slot=<app0|app1>
    #HL?  -> #HL:HELP <one-line command list>

Not implemented: #HLC (calibrate). Fan playback commands (#HLFG, #HLFP, ...)
go to the HoloFan node; the HoloLifter ignores them.

## HoloFan commands (node 16)

The fan's own commands (#HLF..., except the relay #HLF1 / #HLF0 above) are documented in
DroidNet-HoloFan: https://github.com/travisccook/DroidNet-HoloFan/blob/main/docs/SERIAL_COMMANDS.md

## Node commands: #HLZ (both nodes)

    #HLZP<0|1>      query replies off / on (broadcast; default on)    saved, live
    #HLZI<1-19>     node id                                           saved, after reboot
    #HLZN<alias>    alias / WDP name: 1-24 of A-Z a-z 0-9 _ . -,
                    not starting with a digit                          saved, after reboot
    #HLZKS<p><hex>  Wi-Fi profile p (1 = home, 2 = DroidNet) SSID, hex  saved
    #HLZKP<p><hex>  profile p passphrase, hex (empty = open network)    saved
    #HLZX<p>        enter maintenance mode on profile p (see docs/MAINTENANCE_API.md)
    #HLZQ           query (reply):
                    #HL:NODE id=.. name=.. reply=on|off ch=.. q=.. v2=<0|1> up=<0|1>
                             rx=.. rej=.. crc=.. dup=.. rt=.. gu=..
                    (rx frames accepted, rej gate rejects, crc checksum
                     failures, dup duplicates dropped, rt retries, gu sends
                     given up)

Hex example: SSID "My Home" is 4D7920486F6D65, so #HLZKS14D7920486F6D65.

Security: WCB meshes are unencrypted, so these can be sniffed by anyone in
radio range. The mesh password, MAC octets, channel and peer floor can only be
changed over USB (HoloLifter) or the fan's UART header, so a mesh command can
never cut a node off the mesh.

## USB console (HoloLifter, 115200 baud)

The USB console keeps its word commands (help, status, up, down, goto, home,
stop, reset, speed, margin, relay, ...) and adds:

    up <n> / down <n>   relative move by n counts; homed only (before
                        homing: "ERROR Not homed - relative move refused,
                        home first"). Plain up/down behave like #HLU/#HLD.

    hl <#HL...>     run a mesh command locally and print any reply
    mesh            show mesh config and counters (passwords hidden)
    mesh pw <pw>                 mesh password (mesh is OFF until set)
    mesh oct <hh> <hh>           mesh MAC octets 2 and 3 (from ?MAC on a WCB)
    mesh ch <1-11>               mesh channel
    mesh q <0-20>                peer floor (?WCBQ on the WCBs)
    mesh crc on|off              must match ?ETM,CHKSM
    mesh id <1-19>, mesh name <alias>, mesh reply on|off
    mesh ssid <1|2> <name>       rest of the line, spaces allowed
    mesh pass <1|2> [passphrase] rest of the line; empty = open network
    mesh maint <1|2>             enter maintenance mode
    mesh off                     clear the mesh password (mesh OFF after restart)
    mesh clear <1|2>             forget a Wi-Fi profile (ssid and passphrase)
    mesh restart                 reboot to apply mesh settings

Everything printed on the USB console is also mirrored to the mesh as WCB
remote-terminal lines ([TERM:15] in the WCB Wizard terminal) to the WCB that
last sent the node a command (rate-limited to 20 lines/s).

## Design rationale

Why #HL: no overlap with the ~316 commands of the droid ecosystem (MarcDuino,
WCB, JawaLite, FlthyHPs, HCR Vocalizer, motor controllers). Prefixes deliberately avoided: @ (MarcDuino),
? (WCB config), ; (WCB routing), <> (Vocalizer), DT and S (FlthyHPs), #LE
(JawaLite), and bare single letters.
