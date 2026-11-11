# CLAUDE.md

Guidance for Claude Code (and humans) working in this repository.

## What this is

An ESPHome project that reads a Mercedes 722.6-style CAN gear selector with an ESP32-S
(NodeMCU-32S) and an MCP2515 + TJA1050 SPI CAN module. It decodes lever position, tip +/-,
the W/S program switch and the parked flag, and exposes them through ESPHome's web GUI and
Home Assistant API. Everything was verified on real hardware; see `README.md` for photos,
pinouts and the decoded protocol.

## Files

| File | Purpose |
|---|---|
| `gear-lever.yaml` | The ESPHome configuration. Single source of truth for pins, bit rate, entities. |
| `decode_gear.h` | C++ helpers that decode the 0x230 frame. Included by the YAML via `esphome: includes:`. |
| `secrets.yaml.example` | Template for `secrets.yaml` (WiFi credentials). The real file is git-ignored. |
| `README.md` | Full system documentation: hardware, wiring, protocol, troubleshooting. |
| `WIRING.md` | Short bench wiring tables. |
| `Makefile` | Convenience wrappers around the esphome CLI. |
| `images/` | Bench photos referenced from the README. |

## Building

Prerequisites: Python 3.10+ and ESPHome 2024.x or newer (`pip install esphome`, or
`brew install esphome` on macOS). ESP-IDF toolchain download is several GB; keep 5 GB free.

```bash
cp secrets.yaml.example secrets.yaml   # then edit with real WiFi credentials
make config                            # validate YAML only, no toolchain needed
make compile                           # full firmware build
make run                               # build + flash over USB (first time) or OTA
make logs                              # tail the device log
```

Without the Makefile the equivalents are `esphome config|compile|run|logs gear-lever.yaml`.

`make config` is the fast check and runs in seconds. Use it after every YAML edit.
`make compile` takes 5-15 minutes on first run (toolchain + ESP-IDF build), then about a minute.

## Conventions and gotchas

- **Keep the YAML and header in step.** The lambda in `on_frame` calls `decode_gear`,
  `lever_tip_up`, `lever_tip_down`, `lever_parked_flag` and `lever_winter_program` from
  `decode_gear.h`. Renaming a function in one place without the other fails at compile time,
  not at `esphome config`.
- **A stale header silently ships.** ESPHome copies included files at build time. If the
  device shows "unknown" for gear position, the old stub header was compiled: run
  `esphome clean gear-lever.yaml` and rebuild.
- **Do not log inside the hot path per frame.** The lever sends ~40 frames/s. The lambda
  returns early when the payload is unchanged, and the built-in canbus component is set to
  `WARN` under `logger: logs:` to suppress its per-frame DEBUG lines. Keep both.
- **Bit rate and clock are hardware facts.** `bit_rate: 500KBPS` matches the lever and
  `clock: 8MHZ` matches the crystal on this module. Do not change them to fix a problem
  unless the hardware changed.
- **`mode: NORMAL`, not `LISTENONLY`.** On a two-node bus the ESP must ACK frames or the
  lever goes error-passive.
- **Pins are substitutions at the top of the YAML.** Change them there, not inline.
- **Text sensor state is capped at 255 chars.** The "CAN frame table" lambda truncates at 220.
- **Never commit `secrets.yaml`.** It is in `.gitignore`; keep it there.
- **Framework.** `esp-idf` is used. `arduino` also works with no other changes if disk
  space or toolchain downloads are a problem.

## Protocol summary (for quick reference)

Single frame, ID `0x230`, 1 byte, ~40 Hz:

| Bits | Meaning |
|---|---|
| 0-3 | 5=D 6=N 7=R 8=P 9=tip+ A=tip- B/C/D=transit between gates |
| 4 | parked flag (lags P entry/exit slightly) |
| 7 | 1 = W program, 0 = S program (latched by the lever) |

Full tables and example captures are in `README.md` section 2.

## Verifying changes

1. `make config` must print `Configuration is valid!`.
2. `make compile` must finish with the RAM/Flash summary and no `error:` lines.
3. On hardware: open the web GUI, confirm "CAN frames received" climbs, move the lever
   through P/R/N/D and check "Gear lever position" follows; tap +/- and watch the counts.

There are no unit tests. The decoder is pure C++ and small enough to reason about; if you
change it, update the tables in `README.md` and this file.

## Open work

- Shift-lock release: the lever expects a brake-status frame from the car before it will
  leave P. The "Send test frame" button is a stub for this once the frame is known.
- Lever indicator lamps are driven by the car over CAN; frame not yet identified.
