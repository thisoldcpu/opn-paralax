# OPN-Paralax: LPT Sound System  
### Universal DB25 Audio Interface for OPN-TrueVGM

**OPN-Paralax** is a cycle-accurate parallel-port sound interface that replaces classic LPT audio devices such as **Covox Speech Thing**, **Disney Sound Source**, and **OPL2LPT / OPL3LPT**.

It captures raw DB25 bus activity using an **RP2040** and translates it into deterministic audio and register streams — completely **driverless**, **IRQ-free**, and **timing-faithful**.

OPN-Paralax can operate **standalone** or as a high-performance front-end for **OPN-TrueVGM**, where it unlocks real Yamaha OPL and PSG hardware playback.

---

## 🎥 Review & Capture Workflow (Why This Exists)

Parallel-port audio has always been painful to demonstrate.

Reviewing Covox, Disney Sound Source, or OPL2LPT hardware traditionally requires:

- obscure ISA cards or fragile DACs
- analog line-in capture with noise and level issues
- external mixers, adapters, and gain staging
- timing drift between audio and video capture

As a result, nearly every LPT audio review online sounds wrong.

### OPN-Paralax eliminates the entire capture problem.

 **Typical Reviewer Setup**

- **DB25 → Retro PC**  
  *(486 / Pentium / DOSBox host)*

- **USB-C → Capture / Streaming PC**  
  *(OBS, DAW, NLE, broadcast chain)*

- Launch a legacy title (e.g. *Arachnophobia*)  
  Select **Disney Sound Source**, **Covox**, or **OPLxLPT**

Audio appears as a clean, clock-stable digital stream with no analog capture, mixers, or re-routing.

The retro PC believes it is driving a classic parallel-port sound device.  
The capture PC receives production-ready audio.

---

## 🎛️ Supported Modes

### 🟤 Covox Speech Thing
- DATA0–7 mapped to 8-bit unsigned PCM.
- STROBE edge latches samples.
- Typical use: speech, samples, digitized effects.

**Implementation**
- RP2040 PIO captures write edges
- Samples buffered with timestamps
- Streamed as PCM audio

---

### 🟡 Disney Sound Source
- Parallel FIFO with internal playback clock
- Handshaked write pacing

**Implementation**
- FIFO behavior emulated in hardware
- Back-pressure preserved
- Accurate playback without TSRs

---

### 🔵 FTL Sound Adapter
- DAC + timer-based PCM output
- Common in early DOS titles

**Implementation**
- Timing-aware capture
- Converted to deterministic audio streams

---

### 🔴 OPL2LPT / OPL3LPT
- Parallel-port AdLib compatibles
- Register index/data multiplexed via control lines

**Implementation**
- Register writes captured verbatim
- Forwarded losslessly to:
  - Soft-OPL engine
  - **Real YM3812 / YMF262 hardware** (via OPN-TrueVGM Tier-3)

No ISA slots. No emulation guesswork. No reinterpretation.

---

## 🔀 System Architecture

### RP2040 — The Front-End Interface

- **Core 0**
  - PIO state machines
  - DB25 protocol decoding
  - Edge timestamping

- **Core 1**
  - Mode detection
  - Packet framing
  - Optional local monitoring

- **I/O**
  - DB25 (Parallel Port)
  - High-speed SPI or I²S (to OPN-TrueVGM Tier-3)

The RP2040 handles all timing-critical parallel-port work so the host PC doesn’t have to.

---

### OPN-TrueVGM Tier-3 — The Audio Engine

When paired with **OPN-TrueVGM**:

- **Daemon Core**
  - Watchdog
  - Clock supervision
  - Bus arbitration

- **Director Core**
  - Auto-detects incoming PARALAX traffic
  - Routes to correct playback engine

- **Player Core**
  - Soft-OPL emulation **or**
  - Real hardware bus driver (YM2612, YM3812, YMF262, SN76489)

Visualization and UI reflect **actual register activity**, not reconstructed waveforms.

**Host I/O**
- **USB-C Audio out** (to capture / streaming PC)

---

## 📦 Why PARALAX Exists

- **Zero Configuration**
  - No `SOUND.COM`
  - No `SET BLASTER`
  - No IRQ or DMA conflicts

- **Universal Compatibility**
  - Any PC with a standard DB25 parallel port
  - Real hardware, DOSBox, or virtualized hosts

- **Timing-Correct**
  - Cycle-accurate edge capture
  - Deterministic playback clocking

- **Hardware-Forward**
  - Native support for real Yamaha OPL silicon
  - Designed for long-term preservation

- **Modern UX**
  - OTA firmware updates
  - Web-based scopes and meters
  - Real-time diagnostics

---

## 🎚️ Typical User Flow

1. User connects PARALAX to PC with DB25 parallel port and a USB-C cable. Plug in USB-C cable. Paralax is now listening and ready to stream audio, or plug into a PC and select Paralax Audio as the inpout device.
2. Audio is routed to speakers or real chips with effectively zero added latency.

The PC believes it is talking to a classic parallel-port sound device.

It is not.

---

## 🔗 Relationship to OPN-TrueVGM

PARALAX and OPN-TrueVGM are **independent projects** that can operate separately:

- **PARALAX alone**
  - Parallel-port audio capture
  - Diagnostic and monitoring tool

- **OPN-TrueVGM alone**
  - VGM playback
  - Chip emulation or 
  - Real-chip Playback

**Together**, they form a complete, modern replacement for:
- ISA sound cards
- LPT DACs
- Ad-hoc DOS audio hacks

Designed separately. Engineered to interlock.

---

## 🧬 Philosophy

PARALAX does not reinterpret history.  
It listens.

Every edge, every write, every timing quirk — preserved, transported, and rendered faithfully, whether through emulation or real silicon.

Parallel ports deserved better than bit-banged DACs.

Now they have it.

---

## ❓ Why Not Parallel-Port Passthrough?

PARALAX intentionally does **not** provide a physical or electrical passthrough of the DB25 parallel port.

This is a deliberate design choice.

---

### 1. Signal Integrity Comes First

Classic parallel-port audio devices rely on:
- CPU-driven busy loops
- edge timing measured in microseconds
- undefined electrical behavior across chipsets

Introducing a passthrough would:
- add bus loading
- introduce propagation delay
- risk contention between devices

PARALAX guarantees **clean, deterministic capture** by being the *only* device on the bus.

---

### 2. There Is Nothing to “Share”

Most LPT sound devices are:
- write-only
- unidirectional
- unaware of other hardware

The PC is not querying a sound card — it is *shoving bits out a port and hoping something listens*.

PARALAX listens faithfully and completely.

A passthrough device would have no meaningful protocol to arbitrate and would interfere with capture.

---

### 3. Passthrough Breaks the Core Promise

PARALAX exists to:
- replace fragile DAC dongles
- eliminate ISA dependencies
- provide reproducible, clock-accurate playback

Passthrough implies:
- “this device might miss something”
- “timing might vary”
- “audio may or may not match”

That undermines the entire goal.

---

### 4. Software Passthrough Is Superior

If passthrough is needed, it can be done **at a higher level**, with full awareness:

- DB25 → PARALAX → OPN-TrueVGM
- OPN-TrueVGM mirrors traffic to:
  - real sound chips
  - emulation engines
  - visualizers
  - recording tools

This allows:
- inspection
- transformation
- logging
- replay

All things impossible with a passive passthrough.

---

### 5. Future Expansion Is Not Closed

While physical passthrough is excluded, the architecture intentionally allows:

- mirrored register streams
- simultaneous real + emulated playback
- live visualization of external hardware
- conditional routing based on detected protocol

If a *useful* passthrough scenario emerges, it will be implemented deliberately — not electrically.

---

### 6. In Short

OPN-Paralax does not sit *between* devices.

It **becomes** the device.

Parallel-port audio was always a hack.  
OPN-Paralax is the first time it’s been treated like a system.

---

> If you are looking to monitor or visualize an existing ISA or LPT sound card,
> that is a different (and interesting) problem — but not the one PARALAX solves.
