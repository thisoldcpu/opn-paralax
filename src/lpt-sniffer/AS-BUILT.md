# PARALAX LPT SNIFFER - AS-BUILT WIRING REFERENCE

## Complete Pin Mapping (Final Build)

```
┌─────────────────────────────────────────────────────────────┐
│ PARALAX LPT SNIFFER - AS-BUILT CONFIGURATION                │
└─────────────────────────────────────────────────────────────┘

ALL SIGNAL PINS: 470Ω series resistor protection
ALL GROUNDS: Paired to 4x Pico GND pins
UART-SAFE: GP0-1 free for USB serial
DATA BUS: GP2-9 contiguous for PIO optimization

═══════════════════════════════════════════════════════════════

DATA BUS (8-bit parallel, PIO captured)
─────────────────────────────────────────────────────────────
DB25 Pin 2  (D0)       → [470Ω] → GP2  (Pico Pin 4)
DB25 Pin 3  (D1)       → [470Ω] → GP3  (Pico Pin 5)
DB25 Pin 4  (D2)       → [470Ω] → GP4  (Pico Pin 6)
DB25 Pin 5  (D3)       → [470Ω] → GP5  (Pico Pin 7)
DB25 Pin 6  (D4)       → [470Ω] → GP6  (Pico Pin 9)
DB25 Pin 7  (D5)       → [470Ω] → GP7  (Pico Pin 10)
DB25 Pin 8  (D6)       → [470Ω] → GP8  (Pico Pin 11)
DB25 Pin 9  (D7)       → [470Ω] → GP9  (Pico Pin 12)

TIMING (Critical - PIO trigger)
─────────────────────────────────────────────────────────────
DB25 Pin 1  (STROBE)   → [470Ω] → GP10 (Pico Pin 14)

DSS STATUS (Disney Sound Source detection)
─────────────────────────────────────────────────────────────
DB25 Pin 10 (ACK)      → [470Ω] → GP11 (Pico Pin 15)
DB25 Pin 11 (BUSY)     → [470Ω] → GP12 (Pico Pin 16)

LPT STATUS (Full parallel port monitoring)
─────────────────────────────────────────────────────────────
DB25 Pin 12 (PAPER_OUT)→ [470Ω] → GP14 (Pico Pin 19)
DB25 Pin 13 (SELECT)   → [470Ω] → GP15 (Pico Pin 20)
DB25 Pin 15 (ERROR)    → [470Ω] → GP20 (Pico Pin 26)

CONTROL LINES (OPL2LPT detection)
─────────────────────────────────────────────────────────────
DB25 Pin 17 (SELECT PRINTER) → [470Ω] → GP18 (Pico Pin 24)
DB25 Pin 16 (INIT/RESET)     → [470Ω] → GP19 (Pico Pin 25)
DB25 Pin 14 (AUTOFEED)       → [470Ω] → GP21 (Pico Pin 27)

GROUND DISTRIBUTION (Star topology)
─────────────────────────────────────────────────────────────
DB25 Pins 18+19 → Pico GND (Pin 8)
DB25 Pins 20+21 → Pico GND (Pin 18)
DB25 Pins 22+23 → Pico GND (Pin 23)
DB25 Pins 24+25 → Pico GND (Pin 28)

RESERVED FOR FUTURE USE
─────────────────────────────────────────────────────────────
GP13 (Pico Pin 17) → Unused, available for expansion

═══════════════════════════════════════════════════════════════

Total Connections: 17 signals + 8 grounds = 25/25 DB25 pins
Protection: All signals have 470Ω current limiting
Ground Strategy: Paired grounds to 4 GND pins (low impedance)

```

## Output Format

### Verbose Mode (Default)
Every STROBE edge captures all 17 signal states:

```
TIMESTAMP,DATA,ACK,BUSY,POUT,SLCT,ERR,AUTO,INIT,SEL
0,00,1,1,1,1,0,1,1,1
45,80,1,1,1,1,0,1,1,1
90,FF,1,1,1,1,0,1,1,1
135,40,1,0,1,1,0,1,1,1  ← BUSY changed
```

Fields:
- TIMESTAMP: Microseconds since capture start
- DATA: Hex value from D0-D7 (parallel data bus)
- ACK: Acknowledge line (DSS)
- BUSY: Busy/FIFO status (DSS)
- POUT: Paper Out status
- SLCT: Select status line
- ERR: Error line
- AUTO: AutoFeed control
- INIT: Init/Reset control
- SEL: Select Printer control

### Compact Mode
Set `ENABLE_VERBOSE_OUTPUT = false` in firmware for:

```
TIMESTAMP,DATA
0,00
45,80
90,FF
```

## Device Detection Patterns

### Covox Speech Thing
- Continuous DATA writes at ~22 kHz (45 μs period)
- Control/status lines remain static (usually all HIGH)
- Full 8-bit data range (0x00-0xFF)
- No ACK/BUSY activity

### Disney Sound Source
- Burst DATA writes (filling FIFO)
- ACK/BUSY lines toggle during writes
- Lower sample rate (~7 kHz)
- May see periods of silence between bursts

### OPL2LPT
- Paired writes: address then data
- INIT/AUTOFEED/SELECT lines may toggle
- Slower timing (~100+ μs between writes)
- Address range typically 0x00-0xF5

## Testing Procedure

1. Flash firmware with PlatformIO:
   ```
   cd paralax-lpt-sniffer
   pio run -t upload
   ```

2. Connect to serial monitor:
   ```
   pio device monitor
   ```

3. Connect DB25 to DOS PC parallel port

4. Boot DOS machine and run Covox/DSS software

5. Observe continuous stream of captures

6. Save to file for analysis:
   ```
   pio device monitor > capture.csv
   ```

## Troubleshooting

**No data captured:**
- Check all 4 ground connections
- Verify STROBE on GP10 (most critical)
- Confirm DOS software configured for LPT1

**Garbage data:**
- Check for shorts between adjacent DB25 pins
- Verify all 17 resistors installed
- Test continuity on each signal wire

**Intermittent capture:**
- Resolder any cold joints
- Check USB cable quality
- Try different USB port on PC

## Hardware Notes

- **470Ω resistors** limit current to ~8 mA from 5V TTL
- **GP0-1 free** ensures no conflict with USB serial
- **GP2-9 contiguous** allows single PIO instruction for 8-bit read
- **GP13 reserved** for future bidirectional I/O if needed
- **Paired grounds** provides low-impedance return path

## Firmware Customization

To disable verbose output (data only):
```cpp
#define ENABLE_VERBOSE_OUTPUT   false
```

To disable optional status monitoring:
```cpp
#define ENABLE_FULL_LPT_STATUS  false
```

---

**Build Date:** 2026-02-02  
**Builder:** Terminal Cancer Patient Racing Against Time  
**Purpose:** Preserve DOS-era audio at electrical accuracy  

**"We are not emulating the past. We are letting it speak for itself."**
