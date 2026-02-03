# PARALAX LPT Sniffer

Cycle-accurate parallel port capture for DOS-era audio devices.

## Hardware Requirements

- RP2040 Pico or Pico W
- DB25 Male connector
- 17x 470Ω resistors (current limiting / protection)
- Wire for connections

## Wiring Diagram

```
LPT Port (DB25 Male)          RP2040 Pico
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Pin 1  (STROBE)  ──[470Ω]──→  GP10 
Pin 2  (D0)      ──[470Ω]──→  GP2
Pin 3  (D1)      ──[470Ω]──→  GP3
Pin 4  (D2)      ──[470Ω]──→  GP4
Pin 5  (D3)      ──[470Ω]──→  GP5
Pin 6  (D4)      ──[470Ω]──→  GP6
Pin 7  (D5)      ──[470Ω]──→  GP7
Pin 8  (D6)      ──[470Ω]──→  GP8
Pin 9  (D7)      ──[470Ω]──→  GP9
Pin 10 (ACK)     ──[470Ω]──→  GP11
Pin 11 (BUSY)    ──[470Ω]──→  GP12
Pin 12 (PAPER_OUT)─[470Ω]──→  GP14 
Pin 13 (SELECT)  ──[470Ω]──→  GP15 
Pin 14 (AUTOFEED)──[470Ω]──→  GP21
Pin 15 (ERROR)   ──[470Ω]──→  GP20 
Pin 16 (INIT)    ──[470Ω]──→  GP19
Pin 17 (SELECTIN)──[470Ω]──→  GP18
Pins 18-25 (GND) ────────────→  GND (paired to GND pins 8, 18, 23, 28)
```

**IMPORTANT:** 
- The 470Ω resistors are **required** to protect the RP2040 from 5V TTL signals -
- This mapping avoids GP0/GP1 (UART0) to prevent USB serial conflicts
- GP2-9 provides a contiguous 8-bit bus for optimal PIO performance

## Supported Devices

- **Covox Speech Thing** - 8-bit parallel DAC (data writes only)
- **Disney Sound Source** - FIFO-based DAC with ACK/BUSY handshaking
- **OPL2LPT** - AdLib/OPL2 over parallel port (full control line monitoring)
- **Generic LPT devices** - Complete parallel port signal capture

All 17 LPT signal lines are monitored for complete device detection and analysis.

## Building and Flashing

### PlatformIO (Recommended)

```bash
cd paralax-lpt-sniffer
pio run -t upload
```

The firmware will automatically flash to the Pico when connected via USB.

### Arduino IDE (Alternative)

1. Install [Arduino-Pico](https://github.com/earlephilhower/arduino-pico)
2. Select **Tools → Board → Raspberry Pi Pico**
3. Open `src/main.cpp` in Arduino IDE
4. Upload

## Usage

### 1. Connect Hardware

- Wire DB25 to Pico as shown above
- Connect Pico to PC via USB
- Connect DB25 to target PC's parallel port

### 2. Start Capture

Open serial monitor at 115200 baud:

```bash
# Linux/Mac
screen /dev/ttyACM0 115200

# Windows (Device Manager to find COM port)
putty -serial COM3 -sercfg 115200

# PlatformIO
pio device monitor
```

### 3. Run DOS Software

On target PC:
- Boot DOS
- Configure audio for Covox/Disney Sound Source on LPT1
- Run game/demo
- Watch capture stream appear in serial monitor

### 4. Save Capture

```bash
# Redirect to file
pio device monitor > capture.csv

# Or with screen
screen -L /dev/ttyACM0 115200
# Creates screenlog.0
```

## Output Format

CSV format: `TIMESTAMP_US,HEX_BYTE`

```
0,00
125,80
250,FF
375,40
500,C0
...
```

Where:
- `TIMESTAMP_US` = Microseconds since capture start
- `HEX_BYTE` = Data byte (D0-D7) captured on STROBE edge

## Analysis

### Identifying Device Type

**Covox Speech Thing:**
- Continuous data stream
- ~22 kHz sample rate (45 μs between samples)
- Full 8-bit range (0x00-0xFF)

**Disney Sound Source:**
- Burst writes to FIFO
- Status line reads (will need ACK/BUSY monitoring)
- Lower sample rates (~7 kHz)

**OPL2LPT:**
- Paired writes (address, then data)
- Slower timing (register writes ~100 μs apart)
- Specific address patterns (0x00-0xF5 range)

### Sample Analysis Script

```python
import csv
import statistics

samples = []
with open('capture.csv') as f:
    reader = csv.reader(f)
    for row in reader:
        if row[0].startswith('#'):
            continue
        timestamp = int(row[0])
        data = int(row[1], 16)
        samples.append((timestamp, data))

# Calculate sample rate
if len(samples) > 1:
    deltas = [samples[i+1][0] - samples[i][0] 
              for i in range(len(samples)-1)]
    avg_delta = statistics.mean(deltas)
    sample_rate = 1_000_000 / avg_delta  # Convert μs to Hz
    
    print(f"Average sample period: {avg_delta:.1f} μs")
    print(f"Estimated sample rate: {sample_rate:.0f} Hz")
    
    # Check for Covox signature
    if 20_000 < sample_rate < 24_000:
        print("Device: Likely Covox Speech Thing (22 kHz)")
    elif 6_000 < sample_rate < 8_000:
        print("Device: Likely Disney Sound Source (7 kHz)")
```

## Troubleshooting

### No Data Captured

1. Check wiring (especially grounds)
2. Verify DOS software is configured for correct LPT port
3. Test with known-working Covox software
4. Check serial monitor is connected

### Garbage Data

1. Verify all 9 resistors are installed
2. Check for shorts between data lines
3. Ensure STROBE is connected to GP8
4. Check ground connections (need at least 3)

### Intermittent Capture

1. Check resistor values (should be 470Ω ±5%)
2. Verify tight solder connections on DB25
3. Test with shorter wires (<30 cm)

## Next Steps

This sniffer is the front-end for **PARALAX**, which will:
1. Auto-detect device type from capture patterns
2. Route to appropriate ESP32 TrueVGM decoder
3. Drive real Yamaha FM/PSG chips

## Project Links

- PARALAX: _(Coming soon)_
- ESP32 TrueVGM: _(In development)_
- OPN Megaboard: https://github.com/thisoldcpu/opn-megaboard

## License

MIT License - ThisOldCPU Project

---

**"We are not emulating the past. We are letting it speak for itself through better hardware."**
