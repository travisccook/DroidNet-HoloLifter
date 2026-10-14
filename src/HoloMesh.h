// The HoloLifter as a WCB mesh node: radio
// bring-up from the stored NodeConfig, #HL / #HLZ dispatch, query replies,
// the #HLS fast path, maintenance mode and the OTA validity check.
// Everything here runs on the Arduino loop task, except the ESP-NOW receive
// callback (MeshRadio) and the maintenance HTTP task (MaintServer).
#ifndef HOLO_MESH_H
#define HOLO_MESH_H
#include <Arduino.h>

// Console = USB Serial, mirrored to the mesh as RTERM lines.
extern Print& Console;

void holoMeshSetup();
// Call once per loop, before motion.update().
void holoMeshLoop();
// True once if a mesh #HLS arrived since the last call (set on the radio task).
bool holoMeshTakeStop();
// USB "mesh ..." console command; `rawLine` keeps its original case.
void holoMeshUsb(const char* rawLine);
// USB "hl <#HL...>" bench command: dispatch as if from the mesh, print the reply.
void holoMeshBench(const char* line);

#endif
