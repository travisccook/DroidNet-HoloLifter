# DroidNet-HoloLifter

A motorized lift that raises and lowers a holofan platform inside an astromech
droid's dome. It runs on a SparkFun ESP32 Thing Plus, drives a small geared
motor with encoder feedback between two limit switches, and switches the
holofan's power with a relay. It is a node (id 15, alias "HoloLifter") on Greg
Hulette's WCB ESP-NOW mesh, so any droid controller wired to a WCB can move it
with a line like `;wHoloLifter,#HLU`.

Its companion is [DroidNet-HoloFan](https://github.com/travisccook/DroidNet-HoloFan),
replacement firmware that puts the holofan itself on the same mesh.

---

## Standing on shoulders

This project exists because other people built things and shared them.

- **Greg Hulette: the Wireless Communication Board (WCB)**
  <https://github.com/greghulette/Wireless_Communication_Board-WCB> (GPL-3.0)
  and **WCB_Client** <https://github.com/greghulette/WCBClient> (MIT).
  The mesh this lift lives on is Greg's: the WCB boards, their ESP-NOW
  protocol, the delivery acknowledgements and retries, the device discovery
  that lets a controller address a node by name. lib/WcbCore is a port of
  parts of WCB_Client (see THIRD_PARTY_NOTICES.md), and nothing here would
  talk to a droid without it. Thank you, Greg.
- **ReelTwo: R2UppitySpinner**
  <https://github.com/reeltwo/R2UppitySpinner> (LGPL-2.1). When this lift's
  platform sank under load, we studied the Uppity Spinner's motor control
  for ideas (docs/POSITION_HOLDING_RESEARCH.md). No code was copied.
- **Kevin Harrington (madhephaestus): ESP32Encoder**
  <https://github.com/madhephaestus/ESP32Encoder>, which reads the motor's
  quadrature encoder with the ESP32's hardware pulse counter.
- **SparkFun Electronics**: the ESP32 Thing Plus, the Qwiic Motor Driver and
  its Serial Controlled Motor Driver library
  <https://github.com/sparkfun/SparkFun_Serial_Controlled_Motor_Driver_Arduino_Library>.
- **Pololu Robotics and Electronics**: the 298:1 micro metal gearmotor with
  encoder and the 24 V to 5 V step-down regulator.
- The **pioarduino** community platform
  <https://github.com/pioarduino/platform-espressif32>, which brings
  Arduino-ESP32 3.x to PlatformIO.

---

## Use at your own risk

This is a hobby project, shared as is. Under the GPL-3.0 (sections 15 and 16) it comes with no warranty of any kind: if it breaks your droid, your
parts or your fingers, that is on the person who built it, not on the
authors.

- It is a motor inside your droid, with pinch points. Keep fingers, wires and
  dome parts clear of the platform whenever it has power.
- The two limit switches are the safety. They are wired normally closed, so a
  broken wire reads as "limit reached" and stops the motor. Test both by hand
  before every motion test.
- It never moves on power-up: homing runs only when you ask for it, or before
  the first move. Even so, power it up the first few times with a hand on the
  power switch.
- At full speed the lift can coast onto the limit switches. Start slow.
- The droid's 24 V battery and the buck converter can destroy parts if wired
  wrongly. Check every voltage with a meter before connecting the board.
- The ESP32 is 3.3 V logic. Nothing connected to its pins may be 5 V.

Not affiliated with Lucasfilm, SparkFun, Pololu, Espressif or the WCB
project. Product names belong to their owners.

---

## What has been tested

One lift, in one droid, since October 2026: motion, homing, limit switches,
the fan relay, the mesh node, and over-the-air updates with automatic
rollback. Nothing else has been tested. If you build one, you are the second.

---

## Hardware

Full parts list in [BOM.md](BOM.md), every connection in [WIRING.md](WIRING.md).

    PART                                        ROLE
    ------------------------------------------  ------------------------------------
    SparkFun ESP32 Thing Plus (WRL-15663)       controller, Wi-Fi / ESP-NOW radio
    SparkFun Qwiic Motor Driver (ROB-15451)     motor power over I2C, address 0x5D
    Pololu 5172 gearmotor, 298:1, with encoder  run at 5 V; encoder on GPIO 32 / 33
    Pololu 4763 encoder cable                   6-pin JST-SH; check pin 1 (WIRING.md)
    Two micro limit switches, wired NC          upper GPIO 14, lower GPIO 27
    3.3 V relay module                          holofan power, GPIO 4 (label A5)
    Pololu 2858 step-down, 24 V to 5 V          powers everything from the droid

---

## Build and flash

You need [PlatformIO](https://platformio.org/) (the VS Code extension or the
command line). The platform is pinned in platformio.ini to pioarduino
55.03.312-1 (Arduino-ESP32 3.3.12 on ESP-IDF 5.5), which the mesh needs for
ESP-NOW v2.

    pio run                  build
    pio run -t upload        flash over USB
    pio device monitor       USB console, 115200 baud

The first flash is over USB. After that, updates can go over Wi-Fi (below).

---

## First setup

1. Wire it as in WIRING.md, with the motor disconnected the first time.
   Power it and open the USB console. Type `help`.
2. Check the switches and the encoder: `limits` while pressing each switch,
   `encoder` while turning the motor by hand.
3. Join your mesh. The node stays off the mesh until it has a password. Use
   the values your WCBs are set up with:

       mesh pw <password>      the mesh password
       mesh oct <hh> <hh>      MAC octets 2 and 3, in hex
       mesh ch <1-11>          the Wi-Fi channel
       mesh restart            reboot to apply

   `mesh` on its own shows the settings (passwords hidden). Within a few
   seconds of the restart the node appears on the WCBs.

4. Connect the motor. Set a slow speed (`speed 60`), then `home`: the lift
   finds the upper switch, then the lower one, and measures its travel. Try
   short moves (`up 500`, `down 500`) before full ones, and full ones before
   a higher speed.
5. From a droid controller or a WCB console: `;wHoloLifter,#HLQ` asks for the
   status, and the reply appears on the WCBs' broadcast-out ports.

docs/TESTING_PLAN.md has the full bench and mesh test sequence.

---

## Commands

Send them through any WCB as `;wHoloLifter,<command>` or `;w15,<command>`.
Actions never reply; queries reply as a mesh broadcast.

    #HLU / #HLD          move up / down to the soft limit
    #HLS                 stop now (also cuts the holofan's power)
    #HLH                 run homing
    #HLG<pos>            go to a position, in encoder counts
    #HLX                 clear an error
    #HLV<0-255>          speed (saved)
    #HLM<n>              soft-limit margin in counts (saved)
    #HLF1 / #HLF0        holofan power on / off
    #HLQ #HLP #HLL #HLE  status, position, limits, encoder (reply)
    #HLI / #HL?          version / help (reply)
    #HLR                 reboot
    #HLZ...              node settings: replies, id, alias, Wi-Fi profiles,
                         maintenance mode

The full reference, with every reply format, is under "Command reference"
below and in [docs/SERIAL_COMMANDS.md](docs/SERIAL_COMMANDS.md).

---

## Updates over Wi-Fi

After the first USB flash the lift can be updated without opening the droid.
Store a Wi-Fi profile (`mesh ssid 1 <name>`, `mesh pass 1 <passphrase>` on the
USB console, or `#HLZKS` / `#HLZKP` over the mesh), then send `#HLZX1` (or
`mesh maint 1` on USB). The node leaves the mesh, joins that network and
answers at http://hololifter.local:

    curl -X POST -H 'Content-Type: application/octet-stream' \
         --data-binary @.pio/build/esp32thing_plus/firmware.bin \
         http://hololifter.local/api/ota

The new image must reach the mesh within 30 s of booting, or it rolls back to
the previous one on its own. Maintenance mode has no password: anyone on that
network can upload firmware while it is on. Details in
[docs/MAINTENANCE_API.md](docs/MAINTENANCE_API.md).

---

## Tests

The mesh client, the command parser, the motion profile and the maintenance
logic are plain C++ and run on your computer:

    pio test -e native

---

## Companion project

[DroidNet-HoloFan](https://github.com/travisccook/DroidNet-HoloFan) is the
firmware for the holofan this lift carries. The mesh libraries lib/WcbCore,
lib/NodeConfig, lib/MaintCore and lib/LiftCommands are shared by both
repositories and kept identical; a change to one copy is made to both.

---

## Command reference

<!-- copy of docs/SERIAL_COMMANDS.md: change both together -->

The HoloLifter and the HoloFan are both nodes on the droid's WCB ESP-NOW mesh,
and a droid controller reaches them through any WCB. This reference covers the
HoloLifter (node 15) and the #HLZ node commands both nodes share; the fan's
own commands are documented in DroidNet-HoloFan.

### Overview

    Prefix          #HL (HashTag HoloLifter)
    Transport       WCB ESP-NOW mesh (ETM, CRC on), via any WCB serial port
    Terminator      \r on the controller's serial line (the WCB strips it)
    Encoding        ASCII, case-sensitive (#HLU, not #hlu)
    Nodes           HoloLifter  default id 15, alias "HoloLifter"
                    HoloFan     default id 16, alias "HoloFan"

### Addressing a node from a droid controller

Write a WCB command on any WCB serial port:

    ;wHoloLifter,#HLG5000\r     by alias (the node's WDP device name)
    ;w15,#HLG5000\r             by node id

The WCB unicasts the text after the comma to that node with ETM (ACK and
retry). Both forms work once the WCB has heard the node's WDP advert (a few
seconds after the node boots).

### Replies: only queries answer

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

### HoloLifter commands (node 15)

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

### HoloFan commands (node 16)

The fan's own commands (#HLF..., except the relay #HLF1 / #HLF0 above) are documented in
DroidNet-HoloFan: https://github.com/travisccook/DroidNet-HoloFan/blob/main/docs/SERIAL_COMMANDS.md

### Node commands: #HLZ (both nodes)

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

### USB console (HoloLifter, 115200 baud)

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

### Design rationale

Why #HL: no overlap with the ~316 commands of the droid ecosystem (MarcDuino,
WCB, JawaLite, FlthyHPs, HCR Vocalizer, motor controllers). Prefixes deliberately avoided: @ (MarcDuino),
? (WCB config), ; (WCB routing), <> (Vocalizer), DT and S (FlthyHPs), #LE
(JawaLite), and bare single letters.

<!-- end of the copy -->

---

## Licence

GPL-3.0, see [LICENSE](LICENSE). lib/WcbCore contains code ported from
WCB_Client under the MIT licence; its notice is in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
