#include "StarCmd.h"

StarCmd::StarCmd() {
}

StarCmd::~StarCmd() {}

std::array<LCB::Frame, 9> StarCmd::command_sequence(int hccid, int abcid, int address, bool write, uint32_t value) {
  std::array<LCB::Frame, 9> result;

  // Start is a K2 + ABC/HCC + HCC ID
  result[0] = LCB::build_pair(LCB::K3, SixEight::encode(hccid&0xf));
  result[1] = LCB::command_bits(((abcid&0xf) << 2) | ((address>>6)&3));
  result[2] = LCB::command_bits((address&0x3f)<<1);

  for(int i=0; i<5; i++) {
    uint16_t part = (value >> ((4-i)*7)) & 0x7f;
    result[3+i] = LCB::command_bits(part);
  }

  result[8] = LCB::build_pair(LCB::K2, SixEight::encode(hccid&0xf));

  return result;
}
