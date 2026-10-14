// A Print that writes to the USB console and mirrors each completed line to
// the WCB that last sent this node a command, as RTERM packets (docs/SERIAL_COMMANDS.md).
// Use only from the loop task. Mirroring is rate-limited so a burst of debug
// output cannot flood the mesh.
#ifndef TERM_TEE_H
#define TERM_TEE_H
#include <Arduino.h>
#include "WcbNode.h"

class TermTee : public Print {
 public:
  static constexpr uint8_t MAX_LINES_PER_SEC = 20;

  explicit TermTee(Print& usb) : usb_(usb) {}
  // Mirror to `target` (0 = stop mirroring) through `node`.
  void setTarget(wcb::WcbNode* node, uint8_t target) { node_ = node; target_ = target; }
  uint8_t target() const { return target_; }
  uint32_t dropped() const { return dropped_; }

  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t size) override;

 private:
  void flushLine();
  Print&        usb_;
  wcb::WcbNode* node_ = nullptr;
  uint8_t       target_ = 0;
  char          line_[161] = {0};
  size_t        len_ = 0;
  uint32_t      windowMs_ = 0;
  uint8_t       sentInWindow_ = 0;
  uint32_t      dropped_ = 0;
};

#endif
