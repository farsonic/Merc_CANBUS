# CAN gear lever -> ESP32 bench wiring

## Lever connector (from the manual)

| Pin | Function                          | Bench connection                                   |
|-----|-----------------------------------|----------------------------------------------------|
| 1   | Kickdown switch input (not used)  | leave open                                         |
| 2   | Instrument lamp input (+12 V)     | +12 V if you want the P/R/N/D legend lit, else open |
| 3   | DIAG (not used)                   | leave open                                         |
| 4   | GND                               | 12 V supply negative AND ESP32 GND (common ground) |
| 5   | Shift-lock solenoid, +12 V ign.   | +12 V (see shift-lock note)                        |
| 6   | CAN H                             | transceiver CANH                                   |
| 7   | CAN L                             | transceiver CANL                                   |
| 8   | +12 V switched                    | +12 V supply positive                              |
| 9   | Black/blue: +12 V in (rev lamp)   | +12 V if you want to use pin 10 as a reverse input |
| 10  | Grey/yellow: +12 V out in R       | ESP32 GPIO via 27k/10k divider or optocoupler      |

Connector face numbering (looking into the lever's socket): 8-pin block has 4 top-left,
8 top-right, 1 bottom-left, 5 bottom-right, so columns run 1-2-3-4 upward on the left and
5-6-7-8 upward on the right. The 2-pin block is 10 (top) and 9 (bottom).

## ESP32-S side (MCP2515 + TJA1050 module, 8 MHz crystal)

| Module J4 | ESP32-S (NodeMCU-32S) | Note |
|-----------|-----------------------|------|
| VCC       | 5V / VIN              | the TJA1050 transceiver needs 5 V; 3.3 V will not drive the bus |
| GND       | GND                   | same ground as the lever's pin 4 |
| CS        | GPIO5                 | |
| SO        | GPIO19 (MISO)         | 5 V output into a 3.3 V pin, see below |
| SI        | GPIO23 (MOSI)         | |
| SCK       | GPIO18                | |
| INT       | not needed            | ESPHome polls the MCP2515; wire to GPIO4 if you want it for later |

Screw terminal: H -> lever pin 6 (CAN H), L -> lever pin 7 (CAN L).

- J1 (2-pin header near the bottom) is the 120 ohm terminator. Leave the jumper ON: the
  module is one end of the bus and the lever is the other.
- Levels: with VCC at 5 V the MCP2515's SO pin swings to 5 V, which is above the ESP32's
  3.6 V absolute maximum. It usually survives on the bench, but the clean fix is one of:
  1. a 1k series resistor plus a 3.3 V zener, or a 74LVC1G125 / level shifter on SO, or
  2. the common mod: power the MCP2515 from 3.3 V and feed only the TJA1050 with 5 V by
     cutting the VCC trace to U1 and jumpering 5 V to its pin 3.
  The MCP2515 also wants 3.5 V minimum on SCK/SI/CS at 5 V supply; 3.3 V is slightly under
  spec but works in practice on these modules.
- Power the ESP32 from USB for the bench, or from a 12 V -> 5 V buck feeding VIN.
  Never 12 V into the ESP32 or the module.
- Reverse-lamp input (optional): lever pin 10 -> 27k -> GPIO27, GPIO27 -> 10k -> GND.
  Don't use GPIO34-39 for this without the external pulldown; they have no internal one.

## Shift-lock note

Pin 5 only powers the solenoid; the lever module decides when to release it, normally after
it hears "brake pressed" on CAN from the car. On the bench it may refuse to leave P. Options:
1. Use the mechanical shift-lock override (usually a slot under a cap next to the lever).
2. Once you have sniffed the bus, send the frame the lever expects (the "Send test frame"
   button in the YAML is a stub for this).

## Procedure

1. Flash gear-lever.yaml, open the log, power the lever. You should see "ID 0x... len 8 : ..."
   lines within a second. Nothing at all -> wrong bit rate (change `can_bitrate`) or swapped
   CANH/CANL. Only bus-off / error logs -> termination or wiring.
2. Move the lever P -> R -> N -> D -> + -> - slowly. The ID whose bytes change in step with the
   lever is the position frame; note the byte/nibble per position.
3. Write the mapping into decode_gear.h, add a `canbus.send` for any frame the lever needs
   to hear (brake status, ignition status) and the shift-lock will behave.
4. No frames at all: check the POW LED is lit, J1 jumper is on, H/L not swapped, and cycle
   the bit rate. MCP2515 init errors in the log mean an SPI wiring problem, not a CAN one.
