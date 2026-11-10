// Decoder for the lever's CAN frame: ID 0x230, 1 data byte.
// Observed on the bench:
//   low nibble = lever position      5=D 6=N 7=R 8=P 9=tip+ A=tip-  B=N/D C=R/N D=P/R (transit)
//   bit 4 (0x10) = parked flag, set a moment after reaching P, cleared a moment after leaving
#pragma once
#include <map>
#include <string>
#include <vector>

static const uint32_t LEVER_CAN_ID = 0x230;

// Returns "" when the frame is not a steady lever position (transit states, other IDs).
static inline std::string decode_gear(uint32_t can_id, const std::vector<uint8_t> &d) {
  if (can_id != LEVER_CAN_ID || d.empty()) return "";
  switch (d[0] & 0x0F) {
    case 0x5: return "D";
    case 0x6: return "N";
    case 0x7: return "R";
    case 0x8: return "P";
    case 0x9: return "D";   // tip + : lever is still in the D gate
    case 0xA: return "D";   // tip - : same
    default:  return "";    // 0xB/0xC/0xD transit, anything else unknown
  }
}

static inline bool lever_tip_up(uint32_t can_id, const std::vector<uint8_t> &d) {
  return can_id == LEVER_CAN_ID && !d.empty() && (d[0] & 0x0F) == 0x9;
}
static inline bool lever_tip_down(uint32_t can_id, const std::vector<uint8_t> &d) {
  return can_id == LEVER_CAN_ID && !d.empty() && (d[0] & 0x0F) == 0xA;
}
static inline bool lever_parked_flag(uint32_t can_id, const std::vector<uint8_t> &d) {
  return can_id == LEVER_CAN_ID && !d.empty() && (d[0] & 0x10);
}

// W/S program switch: bit 7 (0x80). Latched by the lever itself.
//   0x05 = D in S (standard)    0x85 = D in W (winter)
static inline bool lever_winter_program(uint32_t can_id, const std::vector<uint8_t> &d) {
  return can_id == LEVER_CAN_ID && !d.empty() && (d[0] & 0x80);
}
