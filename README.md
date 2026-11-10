# CAN Gear Lever → ESP32 (ESPHome)

Reads a CAN-bus automatic gear selector (Mercedes 722.6-style lever with tip-shift and
W/S program button) with an ESP32-S and an MCP2515 + TJA1050 SPI CAN module, and exposes
the lever position, tip +/- presses, W/S program and park flag to a web page and to
Home Assistant via ESPHome.

Status: working on the bench. Lever decoded completely from live captures.

---

## 1. Hardware

### 1.1 Parts

| Item | Notes |
|---|---|
| Gear lever | 8-pin + 2-pin connector, CAN at 500 kbit/s, one frame ID 0x230 |
| ESP32-S dev board | NodeMCU-32S / ESP32-WROOM-32 class board |
| MCP2515 + TJA1050 CAN module | Blue SPI module, 8 MHz crystal (X1), J1 = 120 Ω terminator |
| 12 V DC power brick | Powers the lever only |
| USB cable | Powers the ESP32 and the CAN module on the bench |
| Optional | 27 kΩ + 10 kΩ resistors for the hard-wired reverse-lamp input |

### 1.2 Lever connector pinout

Numbering looking into the lever's socket: 8-pin block has 4 top-left, 8 top-right,
1 bottom-left, 5 bottom-right (columns 1-2-3-4 upward on the left, 5-6-7-8 upward on the
right). The 2-pin block is 10 (top) and 9 (bottom).

| Pin | Function | Bench connection |
|---|---|---|
| 1 | Kickdown switch input | not used, leave open |
| 2 | Instrument lamp input (+12 V) | +12 V if you want the P/R/N/D legend lit, else open |
| 3 | DIAG | not used, leave open |
| 4 | GND | 12 V brick negative **and** ESP32 GND (common ground) |
| 5 | Shift-lock solenoid, +12 V switched by ignition | +12 V |
| 6 | CAN H | CAN module screw terminal **H** |
| 7 | CAN L | CAN module screw terminal **L** |
| 8 | +12 V switched | 12 V brick positive |
| 9 | Black/blue: +12 V in for reverse lamp | +12 V, optional |
| 10 | Grey/yellow: +12 V out when in R | ESP32 GPIO27 via 27k/10k divider, optional |

### 1.3 CAN module to ESP32-S

| Module J4 | ESP32-S | Note |
|---|---|---|
| VCC | 5V / VIN | TJA1050 needs 5 V; 3.3 V will not drive the bus |
| GND | GND | shared with lever pin 4 |
| CS | GPIO5 | |
| SO | GPIO19 (MISO) | 5 V output into a 3.3 V pin, see 1.5 |
| SI | GPIO23 (MOSI) | |
| SCK | GPIO18 | |
| INT | not connected | ESPHome polls the MCP2515 |

Leave the **J1 jumper ON**. The module is one end of a two-node bus and the lever is the other.

### 1.4 Power

```
USB ──► ESP32-S ──► 5V pin ──► CAN module VCC
12 V brick + ──► lever pin 8 (and pins 5, 2, 9 as wanted)
12 V brick − ──► lever pin 4 ──► ESP32 GND ──► CAN module GND      (all grounds common)
```

For a permanent install replace USB with a 12 V → 5 V buck converter feeding the ESP32's
VIN pin from the same brick. Never put 12 V on the ESP32 or the CAN module. Do not feed
VIN and USB from different supplies at the same time.

### 1.5 Known compromise: 5 V SO line

With VCC at 5 V the MCP2515's SO pin swings to 5 V, above the ESP32's 3.6 V absolute
maximum. It works on the bench for most boards. Clean fixes if you want them:

1. 1 kΩ series resistor + 3.3 V zener, or a 74LVC1G125 level shifter, on SO; or
2. power the MCP2515 from 3.3 V and only the TJA1050 from 5 V (cut the VCC trace to U1
   and jumper 5 V to its pin 3).

### 1.6 Shift lock

Pin 5 only powers the solenoid; the lever module decides when to release it, normally after
a brake-pressed message from the car. On the bench it may stay locked in P. Use the
mechanical override next to the lever, or send the frame the lever expects once known.

---

## 2. CAN protocol (decoded)

Bus: 500 kbit/s, standard 11-bit IDs. The lever sends **one** frame continuously at
roughly 40 Hz:

| ID | Length | Content |
|---|---|---|
| 0x230 | 1 byte | lever state |

### 2.1 Byte layout

| Bits | Mask | Meaning |
|---|---|---|
| 0–3 | 0x0F | lever position (table below) |
| 4 | 0x10 | parked flag: set shortly after reaching P, cleared shortly after leaving P |
| 5–6 | 0x60 | never seen set |
| 7 | 0x80 | W/S program: 1 = W (winter), 0 = S (standard). Latched by the lever |

### 2.2 Position nibble

| Value | Meaning |
|---|---|
| 0x5 | D |
| 0x6 | N |
| 0x7 | R |
| 0x8 | P |
| 0x9 | tip + (momentary, lever in D gate) |
| 0xA | tip − (momentary, lever in D gate) |
| 0xB | transit between N and D |
| 0xC | transit between R and N |
| 0xD | transit between P and R |

### 2.3 Example captures

| Byte | Meaning |
|---|---|
| 0x05 | D, S program |
| 0x85 | D, W program |
| 0x06 | N |
| 0x07 | R |
| 0x08 | P (just arrived) |
| 0x18 | P, parked flag set |
| 0x1D | leaving P towards R, parked flag still set |
| 0x17 | R, parked flag not yet cleared |
| 0x09 → 0x05 | tip + then release |
| 0x0A → 0x05 | tip − then release |

---

## 3. Software

### 3.1 Files

```
gear-lever/
├── gear-lever.yaml     ESPHome configuration
├── decode_gear.h       C++ decoder for the 0x230 frame (included by the YAML)
├── secrets.yaml        wifi_ssid / wifi_password
└── README.md           this file
```

### 3.2 Build and flash

```bash
esphome clean gear-lever.yaml
esphome run gear-lever.yaml
```

ESP-IDF toolchain download is several GB; keep at least 5 GB free. If space is tight,
change `framework: type: esp-idf` to `type: arduino` in the YAML; everything else is unchanged.

### 3.3 Web GUI

Open `http://gear-lever.local` (or the device IP). If WiFi fails the ESP opens an access
point `gear-lever` / `gearlever123` and the page is at `http://192.168.4.1`.

| Entity | Meaning |
|---|---|
| Gear lever position | P / R / N / D, updates only on steady positions |
| Tip + / Tip − | on while the lever is held against the stop, held 750 ms minimum |
| Tip + count / Tip − count | increments per press |
| Winter program (W) | on = W, off = S |
| Parked flag (bit 4) | mirrors bit 4 |
| Reverse (hardwired) | optional GPIO27 input from lever pin 10 |
| Last CAN frame | most recent frame whose payload changed |
| CAN frame table | latest payload per CAN ID |
| Changed IDs (last 2s) | IDs that changed in the last two seconds |
| CAN frames received / payload changes | diagnostics |
| Clear frame table | resets the sniffer tables and counters |
| Send test frame | stub for transmitting to the lever later (ID 0x200, all zeros) |
| Restart | reboots the ESP |

The log panel shows one line per payload change, e.g. `[can:104] 0x230: 85`, plus
`TIP +`, `TIP -` and `Program: W/S` lines. The built-in canbus component's per-frame
DEBUG spam is silenced with `logs: canbus: WARN`.

### 3.4 Troubleshooting

| Symptom | Cause / fix |
|---|---|
| CAN frames received stays 0 | wrong bit rate (change `can_bitrate`), H/L swapped, J1 off, no common ground |
| MCP2515 init errors in log | SPI wiring (CS/SO/SI/SCK) or module not powered with 5 V |
| Gear position shows "unknown" | old stub `decode_gear.h` was compiled; replace file, `esphome clean`, rebuild |
| Entities missing from web page | old YAML was flashed; replace file and rebuild |
| Tip +/− never visible | fixed by 750 ms hold and counters; check counts climb |
| Log flooded with `received can message` | add `logs: canbus: WARN` under `logger:` |
| Lever won't leave P | shift lock; use mechanical override or send brake frame (future work) |
| "No space left on device" during build | free disk, `rm -rf ~/Library/Caches/esphome/idf`, rebuild |

---

## 4. decode_gear.h

```cpp
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
```

---

## 5. gear-lever.yaml

```yaml
# ESPHome: CAN gear lever -> ESP32-S via MCP2515 + TJA1050 SPI module (8 MHz crystal)
#
# Web GUI: http://gear-lever.local  (or the IP from your router)
#   - "Last CAN frame"       : most recent frame whose payload changed
#   - "CAN frame table"      : one entry per CAN ID with its latest payload
#   - "Changed IDs (last 2s)": which IDs changed while you moved the lever  <- the useful one
#   - "Logs" panel at the bottom of the page shows the same stream as the serial log
#
# Fallback: if WiFi fails, the ESP opens an access point "gear-lever" (password below)
# and the GUI is at http://192.168.4.1

substitutions:
  name: gear-lever
  spi_sck_pin: GPIO18      # module SCK
  spi_mosi_pin: GPIO23     # module SI
  spi_miso_pin: GPIO19     # module SO
  can_cs_pin: GPIO5        # module CS
  reverse_in_pin: GPIO27   # optional: lever pin 10 (grey/yellow) via 27k/10k divider
  can_bitrate: 500KBPS     # try 500KBPS first, then 125KBPS, 250KBPS, 100KBPS, 83K3BPS

esphome:
  name: ${name}
  friendly_name: Gear Lever
  includes:
    - decode_gear.h

esp32:
  board: nodemcu-32s
  framework:
    type: esp-idf

logger:
  level: DEBUG
  baud_rate: 115200
  logs:
    canbus: WARN           # the canbus component logs every frame at DEBUG; we log changes ourselves
    mcp2515: WARN

api:

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "gear-lever"
    password: "gearlever123"

captive_portal:

web_server:
  port: 80
  version: 3
  log: true                # show the log stream in the browser

globals:
  - id: last_frames
    type: std::map<uint32_t, std::vector<uint8_t>>
  - id: change_time
    type: std::map<uint32_t, uint32_t>
  - id: frame_count
    type: uint32_t
    initial_value: '0'
  - id: change_count
    type: uint32_t
    initial_value: '0'
  - id: last_frame_str
    type: std::string
    initial_value: '"none yet"'
  - id: tip_up_count
    type: uint32_t
    initial_value: '0'
  - id: tip_down_count
    type: uint32_t
    initial_value: '0'

spi:
  clk_pin: ${spi_sck_pin}
  mosi_pin: ${spi_mosi_pin}
  miso_pin: ${spi_miso_pin}

canbus:
  - platform: mcp2515
    id: can0
    cs_pin: ${can_cs_pin}
    clock: 8MHZ
    mode: NORMAL           # we must ACK the lever's frames on a 2-node bus
    can_id: 0x7FF
    bit_rate: ${can_bitrate}
    on_frame:
      - can_id: 0
        can_id_mask: 0     # accept every standard-ID frame
        use_extended_id: false
        then:
          - lambda: |-
              id(frame_count)++;
              auto &prev = id(last_frames)[can_id];
              if (prev == x) return;                      // unchanged payload -> ignore
              bool first = prev.empty();
              prev = x;
              id(change_count)++;
              if (!first) id(change_time)[can_id] = millis();
              char buf[48];
              int n = snprintf(buf, sizeof(buf), "0x%03X:", can_id);
              for (size_t i = 0; i < x.size() && i < 8; i++)
                n += snprintf(buf + n, sizeof(buf) - n, " %02X", x[i]);
              id(last_frame_str) = buf;
              ESP_LOGI("can", "%s", buf);
              std::string g = decode_gear(can_id, x);
              if (!g.empty() && g != id(gear).state) id(gear).publish_state(g);
              bool up = lever_tip_up(can_id, x), down = lever_tip_down(can_id, x);
              if (up)   { id(tip_up_count)++;   ESP_LOGI("lever", "TIP +"); }
              if (down) { id(tip_down_count)++; ESP_LOGI("lever", "TIP -"); }
              id(tip_up).publish_state(up);
              id(tip_down).publish_state(down);
              id(parked_flag).publish_state(lever_parked_flag(can_id, x));
              bool w = lever_winter_program(can_id, x);
              if (w != id(winter_program).state) ESP_LOGI("lever", "Program: %s", w ? "W" : "S");
              id(winter_program).publish_state(w);
      - can_id: 0
        can_id_mask: 0
        use_extended_id: true
        then:
          - lambda: |-
              ESP_LOGI("can", "EXT 0x%08X len %u", can_id, (unsigned) x.size());

text_sensor:
  - platform: template
    id: gear
    name: "Gear lever position"
    update_interval: never
    icon: mdi:car-shift-pattern

  - platform: template
    name: "Last CAN frame"
    icon: mdi:code-brackets
    update_interval: 500ms
    lambda: return id(last_frame_str);

  - platform: template
    name: "CAN frame table"
    icon: mdi:table
    update_interval: 1s
    lambda: |-
      std::string out;
      for (auto &kv : id(last_frames)) {
        char buf[48];
        int n = snprintf(buf, sizeof(buf), "%03X:", kv.first);
        for (uint8_t b : kv.second) n += snprintf(buf + n, sizeof(buf) - n, "%02X", b);
        if (!out.empty()) out += " | ";
        out += buf;
        if (out.size() > 220) { out += " ..."; break; }   // HA/web limit is 255 chars
      }
      return out.empty() ? std::string("no frames") : out;

  - platform: template
    name: "Changed IDs (last 2s)"
    icon: mdi:swap-horizontal
    update_interval: 500ms
    lambda: |-
      std::string out;
      uint32_t now = millis();
      for (auto &kv : id(change_time)) {
        if (now - kv.second < 2000) {
          char buf[8];
          snprintf(buf, sizeof(buf), "%03X ", kv.first);
          out += buf;
        }
      }
      return out.empty() ? std::string("-") : out;

sensor:
  - platform: template
    name: "Tip + count"
    icon: mdi:plus-box
    lambda: return id(tip_up_count);
    update_interval: 500ms
    accuracy_decimals: 0
  - platform: template
    name: "Tip - count"
    icon: mdi:minus-box
    lambda: return id(tip_down_count);
    update_interval: 500ms
    accuracy_decimals: 0
  - platform: template
    name: "CAN frames received"
    lambda: return id(frame_count);
    update_interval: 2s
    accuracy_decimals: 0
    entity_category: diagnostic
  - platform: template
    name: "CAN payload changes"
    lambda: return id(change_count);
    update_interval: 2s
    accuracy_decimals: 0
    entity_category: diagnostic
  - platform: wifi_signal
    name: "WiFi signal"
    update_interval: 60s
    entity_category: diagnostic

binary_sensor:
  - platform: template
    id: tip_up
    name: "Tip +"
    icon: mdi:plus-box
    filters:
      - delayed_off: 750ms   # hold the indicator so a quick tap is visible
  - platform: template
    id: tip_down
    name: "Tip -"
    icon: mdi:minus-box
    filters:
      - delayed_off: 750ms
  - platform: template
    id: winter_program
    name: "Winter program (W)"
    icon: mdi:snowflake
  - platform: template
    id: parked_flag
    name: "Parked flag (bit 4)"
    icon: mdi:car-brake-parking
  - platform: gpio
    name: "Reverse (hardwired)"
    pin:
      number: ${reverse_in_pin}
      mode: INPUT_PULLDOWN
    filters:
      - delayed_on_off: 30ms

button:
  - platform: template
    name: "Clear frame table"
    icon: mdi:broom
    on_press:
      - lambda: |-
          id(last_frames).clear();
          id(change_time).clear();
          id(frame_count) = 0;
          id(change_count) = 0;
          id(last_frame_str) = "cleared";
  - platform: template
    name: "Send test frame"
    icon: mdi:send
    on_press:
      - canbus.send:
          canbus_id: can0
          can_id: 0x200       # placeholder until you know what the lever listens for
          data: [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
  - platform: restart
    name: "Restart"
```

---

## 6. secrets.yaml

```yaml
wifi_ssid: "your-ssid"
wifi_password: "your-password"
```

---

## 7. Future work

- **Shift-lock release**: sniff what the lever expects from the car (brake status frame)
  and transmit it from the ESP so the lever leaves P without the manual override.
- **W/S and gear indicator lamps**: the lever's indicator LEDs are normally driven by the
  car over CAN; identify the frame and send it so the display follows the state.
- **Home Assistant**: the `api:` block is enabled, so the device can be adopted in the
  ESPHome add-on. Tip +/− counts and the position text sensor make clean automation triggers.
- **Level shifting on SO** per section 1.5 for a permanent build.
