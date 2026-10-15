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

- **Todd Word and Michael Bean: the BeanLifter.** They broke ground with the
  BeanLifter and took the holofan in a droid from a good idea to a working
  build.
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
Commands are case-sensitive. Actions never reply, not even on an error;
queries reply as a mesh broadcast that every WCB writes to its
broadcast-out ports.

Motion:

    #HLU / #HLD     move up / down to the soft limit, slowing before it
    #HLS            stop now (also cuts the holofan's power)
    #HLH            run homing
    #HLG<pos>       go to a position, in encoder counts
    #HLX            clear an error (e.g. after a stall); keeps homing

The lift never homes by itself at power-up. The first #HLU or #HLG homes it
first; #HLD before homing runs at homing speed down to the lower switch.

Holofan power:

    #HLF1 / #HLF0   relay on / off

Settings (saved):

    #HLV<0-255>     speed; below 50 counts as 50 (the lift stalls uphill)
    #HLM<n>         stop n counts short of each limit switch (default 40)
    #HLCFGRST       reset the saved settings to their defaults

Queries:

    #HLQ  -> #HL:STATUS state=<s> pos=<n> target=<n> homed=<0|1> speed=<n> limits=<u>,<l> fan=<0|1>
    #HLP  -> #HL:POS <n>
    #HLL  -> #HL:LIMITS upper=<0|1> lower=<0|1>
    #HLI  -> #HL:INFO ver=<firmware> board=ESP32ThingPlus slot=<app0|app1>
    #HL?  -> #HL:HELP <one-line command list>

    #HLR            reboot (stops the lift first; homing is lost)

Node settings (saved):

    #HLZN<alias>    name on the mesh (after a reboot)
    #HLZI<1-19>     node id (after a reboot)
    #HLZP<0|1>      query replies off / on
    #HLZKS<p>=name  Wi-Fi profile p (1 or 2): network name
    #HLZKP<p>=pass  Wi-Fi profile p: passphrase (empty = open network)
    #HLZX<p>        maintenance mode on Wi-Fi profile p ("Updates over Wi-Fi")
    #HLZQ  -> #HL:NODE id=.. name=.. reply=on|off ...

For example `;wHoloLifter,#HLZKS1=My Home`, then
`;wHoloLifter,#HLZKP1=secret123`. A name or passphrase that contains `^`, or
ends in a space, has to go in hex instead (docs/SERIAL_COMMANDS.md) or
through the USB console (`mesh ssid 1 My Home`, `mesh pass 1 <passphrase>`;
`help` lists them all). The console is also the only place to set the mesh
password, MAC octets and channel ("First setup").

The full reference, with every reply field and the USB console, is
[docs/SERIAL_COMMANDS.md](docs/SERIAL_COMMANDS.md).

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

## Licence

GPL-3.0, see [LICENSE](LICENSE). lib/WcbCore contains code ported from
WCB_Client under the MIT licence; its notice is in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
