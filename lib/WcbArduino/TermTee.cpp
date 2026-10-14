#include "TermTee.h"

size_t TermTee::write(uint8_t c) {
  usb_.write(c);
  if (c == '\r') return 1;
  if (c == '\n') { flushLine(); return 1; }
  if (len_ < sizeof(line_) - 1) line_[len_++] = (char)c;
  else flushLine(), line_[len_++] = (char)c;   // over-long line: send it in pieces
  return 1;
}

size_t TermTee::write(const uint8_t* buf, size_t size) {
  for (size_t i = 0; i < size; i++) write(buf[i]);
  return size;
}

void TermTee::flushLine() {
  line_[len_] = '\0';
  size_t n = len_;
  len_ = 0;
  if (!node_ || target_ == 0 || n == 0 || !node_->started()) return;
  uint32_t now = millis();
  if (now - windowMs_ >= 1000) { windowMs_ = now; sentInWindow_ = 0; }
  if (sentInWindow_ >= MAX_LINES_PER_SEC) { dropped_++; return; }
  sentInWindow_++;
  node_->sendTerm(target_, line_);
}
