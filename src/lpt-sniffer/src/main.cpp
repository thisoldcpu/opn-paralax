/*
 * PARALAX LPT Sniffer - 17-signal capture on STROBE edge (as-built wiring)
 * ThisOldCPU Project - https://github.com/thisoldcpu
 *
 * Hardware: RP2040 Pico/Pico W (earlephilhower Arduino core)
 * Purpose : Capture raw parallel-port activity (Covox/DSS/OPL2LPT) with 17 signals
 *
 * Trigger : GP10 (STROBE) falling edge
 * Capture : One CSV line per STROBE event
 *
 * Output  : t_us,data_hex,strobe,ack,busy,autofeed,init,selectin,paper_out,select,error
 *
 * 
 * TODO - Add device list: Unlatched Covox-style DAC
 * 
 * 
 * License : MIT
 */

#include <Arduino.h>

#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"

#include "pico/time.h"

// -------------------- AS-BUILT PIN MAP --------------------
static constexpr uint PIN_D0_D7_BASE = 2; // GP2..GP9
static constexpr uint PIN_STROBE = 10;    // DB25-1
static constexpr uint PIN_ACK = 11;       // DB25-10
static constexpr uint PIN_BUSY = 12;      // DB25-11

static constexpr uint PIN_AUTOFEED = 21;       // DB25-14
static constexpr uint PIN_INIT = 19;           // DB25-16 (RESET/INIT)
static constexpr uint PIN_SELECT_PRINTER = 18; // DB25-17 (SELECTIN)

static constexpr uint PIN_PAPER_OUT = 14;     // DB25-12
static constexpr uint PIN_SELECT_STATUS = 15; // DB25-13 (SELECT)
static constexpr uint PIN_ERROR = 20;         // DB25-15
// ----------------------------------------------------------

// Output controls
static constexpr bool PRINT_HEADER_ON_BOOT = true;
static constexpr bool PRINT_HEARTBEAT_IDLE = true;

// Serial speed (CSV is heavy; go fast)
static constexpr uint32_t SERIAL_BAUD = 921600;

// Ring buffer sizing (frames, power-of-two)
static constexpr uint32_t RB_SIZE = 4096;
static_assert((RB_SIZE & (RB_SIZE - 1)) == 0, "RB_SIZE must be power-of-two");

// Frame coalescing deadband (microseconds)
// Prevents multi-frame spam from bit-skew/ripple during a single write.
static constexpr uint32_t FRAME_DEADBAND_US = 3;

struct CaptureFrame
{
  uint32_t t_us; // timestamp since start
  uint8_t  data; // D0..D7
  uint16_t bits; // packed 9-bit control/status snapshot
  // bits layout:
  // 0 STROBE
  // 1 ACK
  // 2 BUSY
  // 3 AUTOFEED
  // 4 INIT
  // 5 SELECTIN
  // 6 PAPER_OUT
  // 7 SELECT
  // 8 ERROR
};

// NOTE: buffer is NOT volatile; only indices are volatile.
// ISR is the only writer; loop() is the only reader.
static CaptureFrame rb[RB_SIZE];
static volatile uint32_t rb_w = 0;
static volatile uint32_t rb_r = 0;
static volatile uint32_t dropped = 0;

static uint32_t start_us = 0;
static uint32_t frames_captured = 0;
static uint32_t last_frame_ms = 0;

// For deadband coalescing
static volatile uint32_t last_frame_t_us = 0;

static inline uint32_t gpio_snapshot()
{
  return sio_hw->gpio_in;
}

static inline uint8_t read_data_bus(uint32_t snap)
{
  return (uint8_t)((snap >> PIN_D0_D7_BASE) & 0xFFu);
}

static inline uint16_t pack_bits(uint32_t snap)
{
  uint16_t b = 0;
  b |= ((snap >> PIN_STROBE) & 1u) << 0;
  b |= ((snap >> PIN_ACK) & 1u) << 1;
  b |= ((snap >> PIN_BUSY) & 1u) << 2;

  b |= ((snap >> PIN_AUTOFEED) & 1u) << 3;
  b |= ((snap >> PIN_INIT) & 1u) << 4;
  b |= ((snap >> PIN_SELECT_PRINTER) & 1u) << 5;

  b |= ((snap >> PIN_PAPER_OUT) & 1u) << 6;
  b |= ((snap >> PIN_SELECT_STATUS) & 1u) << 7;
  b |= ((snap >> PIN_ERROR) & 1u) << 8;
  return b;
}

static inline uint8_t bit_at(uint16_t bits, uint8_t idx)
{
  return (uint8_t)((bits >> idx) & 1u);
}

// ---- IRQ handler: ANY edge on ANY monitored pin -> enqueue a FRAME ----
static void __not_in_flash_func(any_irq)(uint gpio, uint32_t events)
{
  (void)gpio;
  (void)events;

  // Timestamp close to edge
  uint32_t t = (uint32_t)(time_us_32() - start_us);

  // Deadband to coalesce bus ripple into one frame
  uint32_t prev = last_frame_t_us;
  if ((t - prev) <= FRAME_DEADBAND_US)
    return;
  last_frame_t_us = t;

  // Snapshot all pins once
  uint32_t snap = gpio_snapshot();

  // Compute next slot
  uint32_t w = rb_w;
  uint32_t next = (w + 1) & (RB_SIZE - 1);

  if (next == rb_r)
  {
    dropped++;
    return;
  }

  rb[w].t_us = t;
  rb[w].data = read_data_bus(snap);
  rb[w].bits = pack_bits(snap);

  // Publish write index last
  rb_w = next;
}

static void setup_inputs()
{
  // Data bus: define quiet as low
  for (uint pin = PIN_D0_D7_BASE; pin < PIN_D0_D7_BASE + 8; ++pin)
  {
    pinMode(pin, INPUT_PULLDOWN);
  }

  // Control/status: pull-ups for defined idle while disconnected
  pinMode(PIN_STROBE, INPUT_PULLUP);
  pinMode(PIN_ACK, INPUT_PULLUP);
  pinMode(PIN_BUSY, INPUT_PULLUP);

  pinMode(PIN_AUTOFEED, INPUT_PULLUP);
  pinMode(PIN_INIT, INPUT_PULLUP);
  pinMode(PIN_SELECT_PRINTER, INPUT_PULLUP);

  pinMode(PIN_PAPER_OUT, INPUT_PULLUP);
  pinMode(PIN_SELECT_STATUS, INPUT_PULLUP);
  pinMode(PIN_ERROR, INPUT_PULLUP);
}

static void arm_all_irqs()
{
  // Set callback once using any one pin, then enable others with gpio_set_irq_enabled().
  gpio_set_irq_enabled_with_callback(
      PIN_STROBE,
      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
      true,
      &any_irq);

  // DATA bus GP2..GP9
  for (uint pin = PIN_D0_D7_BASE; pin < PIN_D0_D7_BASE + 8; ++pin)
  {
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  }

  // Control/status pins
  gpio_set_irq_enabled(PIN_ACK,            GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_BUSY,           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_AUTOFEED,       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_INIT,           GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_SELECT_PRINTER, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_PAPER_OUT,      GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_SELECT_STATUS,  GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_ERROR,          GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}

static void print_banner()
{
  Serial.println();
  Serial.println("========================================");
  Serial.println("PARALAX LPT Sniffer - FRAME capture (17 signals)");
  Serial.println("ThisOldCPU - Raw parallel truth stream");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Trigger: ANY edge on DATA or control/status pins");
  Serial.println("CSV: t_us,data_hex,strobe,ack,busy,autofeed,init,selectin,paper_out,select,error");
  Serial.print("Deadband(us): ");
  Serial.println((uint32_t)FRAME_DEADBAND_US);
  Serial.println();
}

static bool rb_pop(CaptureFrame &out)
{
  // Critical section so rb_r / rb_w and rb[] slot read can't tear
  uint32_t irq_state = save_and_disable_interrupts();

  uint32_t r = rb_r;
  if (r == rb_w)
  {
    restore_interrupts(irq_state);
    return false;
  }

  out = rb[r];
  rb_r = (r + 1) & (RB_SIZE - 1);

  restore_interrupts(irq_state);
  return true;
}

static void drain_and_print()
{
  CaptureFrame ev;
  while (rb_pop(ev))
  {
    Serial.print(ev.t_us);
    Serial.print(",");

    if (ev.data < 0x10)
      Serial.print("0");
    Serial.print(ev.data, HEX);
    Serial.print(",");

    Serial.print(bit_at(ev.bits, 0));
    Serial.print(",");
    Serial.print(bit_at(ev.bits, 1));
    Serial.print(",");
    Serial.print(bit_at(ev.bits, 2));
    Serial.print(",");
    Serial.print(bit_at(ev.bits, 3));
    Serial.print(",");
    Serial.print(bit_at(ev.bits, 4));
    Serial.print(",");
    Serial.print(bit_at(ev.bits, 5));
    Serial.print(",");
    Serial.print(bit_at(ev.bits, 6));
    Serial.print(",");
    Serial.print(bit_at(ev.bits, 7));
    Serial.print(",");
    Serial.println(bit_at(ev.bits, 8));

    frames_captured++;
    last_frame_ms = millis();
  }
}

static void print_stats_periodic()
{
  static uint32_t last_stats_ms = 0;
  uint32_t now = millis();
  if (now - last_stats_ms < 5000)
    return;
  last_stats_ms = now;

  Serial.println();
  Serial.println("--- Statistics ---");
  Serial.print("Frames captured: ");
  Serial.println(frames_captured);
  Serial.print("Ring dropped   : ");
  Serial.println((uint32_t)dropped);
  Serial.println("------------------");
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  uint32_t s = millis();
  while (!Serial && (millis() - s) < 3000)
    delay(10);

  setup_inputs();

  start_us = time_us_32();
  last_frame_ms = millis();
  last_frame_t_us = 0;

  if (PRINT_HEADER_ON_BOOT)
    print_banner();

  arm_all_irqs();

  Serial.println("# Armed: waiting for ANY bus activity...");
  Serial.println();
}

void loop()
{
  drain_and_print();

  if (PRINT_HEARTBEAT_IDLE)
  {
    static uint32_t last_hb = 0;
    uint32_t now = millis();
    if (frames_captured == 0 && (now - last_hb) > 10000)
    {
      Serial.println("# idle: no activity yet");
      last_hb = now;
    }
  }

  if (frames_captured > 0 && (millis() - last_frame_ms) > 5000)
  {
    print_stats_periodic();
  }
}
