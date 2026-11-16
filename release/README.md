# HoloLifter firmware release

Version 1.0.2, built from commit b9751c5 with pio run (pioarduino 55.03.312-1, Arduino-ESP32 3.3.12).
Not yet run on hardware (manifest.json: tested_on_hardware false).

    manifest.json    version, commit, and each file's name, size and SHA-256
    hololifter.bin   the application image: the one to send over Wi-Fi
    hololifter.factory.bin whole flash image for offset 0x0 over USB (bootloader, partitions, app)

manifest.json is the file for a program to read: it names the OTA image
(files.ota.file) and gives its size and SHA-256 to check before sending.

## Updating over Wi-Fi

Keep a WCB switched on and in range while you update: the new image must receive a mesh frame within 30 s of booting, or the node rolls back to the previous image.

Put the node in maintenance mode, then:

    curl -X POST -H 'Content-Type: application/octet-stream' \
         --data-binary @release/hololifter.bin http://hololifter.local/api/ota

docs/MAINTENANCE_API.md has the whole procedure and the rollback rule.

## Making a release

Build on a clean checkout, copy the images here under the same names,
and update manifest.json (version, commit, sizes, SHA-256) in the same
commit. The binaries hold the build machine's paths as text, so build in a
directory whose path says nothing private, and run
`python3 tools/check_private.py` before committing.
