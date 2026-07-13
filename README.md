# ESP32 FreeRTOS Showcase

One cohesive ESP-IDF project for a **regular ESP32 DevKit** (dual-core WiFi/BT variant — not S2/S3/C3) that exercises three real peripherals — potentiometer (ADC), SG90 servo (PWM), SSD1306 OLED (I2C) — while deliberately demonstrating the core FreeRTOS primitives: **tasks & priorities, queues, mutexes, ISR → binary semaphore deferral, and software timers**. The code is the demo *and* the study material: every module's header states which concept it demonstrates and why.

```
pot (GPIO34) ──> [ADC Task] ──queue──> [Servo Task] ──PWM──> SG90 (GPIO18)
                     │                      │
                     └──────┐   ┌───────────┘
                            ▼   ▼
button (GPIO4) ─ISR─sem─> [Button Task] ─> ┌──────────────┐
                                           │ shared state │──> [Display Task] ──I2C──> OLED
[Stats Task] ────────────────────────────> │  (1 mutex)   │        (snapshots)
                                           └──────────────┘
[Heartbeat software timer] ──> LED (GPIO2)   (no task, no shared state)
```

## Hardware & wiring

| Signal | GPIO | Notes |
|---|---|---|
| Potentiometer wiper | **34** | ADC1_CH6. Must be **ADC1** — ADC2 is unusable while WiFi runs. Pot ends to 3V3 and GND. |
| SG90 servo signal | **18** | LEDC PWM, 50 Hz. Power the servo from 5 V (VIN/USB), **common GND** with the ESP32 — don't feed it from 3V3. |
| SSD1306 SDA | **21** | Standard ESP32 I2C pins. Internal pull-ups enabled; add external 4.7 kΩ for long wires. |
| SSD1306 SCL | **22** | 400 kHz. |
| Push button | **4** | Internal pull-up, button to GND (active-low), interrupt on falling edge. |
| Status LED | **2** | Onboard LED on most DevKits. Heartbeat via software timer. |

## Build & flash

Requires **ESP-IDF v5.3 or newer** (tested against v6.0): the project uses the new `driver/i2c_master.h` I2C API and requires the split `esp_driver_*` components introduced in v5.3. No external components — builds with a bare IDF install, no component manager downloads.

```sh
idf.py set-target esp32
idf.py build flash monitor
```

`sdkconfig.defaults` sets `CONFIG_FREERTOS_HZ=1000` (1 ms tick) so the 30/50/200 ms periods in this project are exact; the IDF default of 100 Hz would make anything under 10 ms round to zero ticks.

## Using it

- Turn the pot → servo follows (0–180°), OLED **Sensor View** shows raw ADC, voltage, and the applied servo angle.
- Press the button → toggles between **Sensor View** and **System Stats View** (free heap, uptime, per-task stack high-water marks).
- The onboard LED blinks at 1 Hz — if it ever stops, the timer service task is starved, which is itself a diagnostic.

---

# Architecture & FreeRTOS study guide

## The scheduling picture

| Task | Priority | Period / blocking behavior | Stack (bytes) | File |
|---|---|---|---|---|
| Button | 3 (highest) | Blocks forever on binary semaphore; runs only on a press | 2048 | `main/button_task.c` |
| ADC | 2 | Periodic, 50 ms via `vTaskDelayUntil` | 3072 | `main/adc_task.c` |
| Servo | 2 | Blocks forever on queue receive | 2048 | `main/servo_task.c` |
| Display | 1 | Periodic, 200 ms via `vTaskDelayUntil` | 3328 | `main/display_task.c` |
| Stats | 0 (idle) | Periodic, 1 s via `vTaskDelayUntil` | 2048 | `main/stats_task.c` |
| *(Tmr Svc)* | *IDF config (default 1)* | Runs timer callbacks (heartbeat) | *IDF config* | — |

Priorities follow one rule: **priority = urgency, not importance.** The button task is highest not because buttons matter most but because its work (a few assignments) is tiny and its latency requirement (feel instant to a human) is the tightest. The display is *below* the control path because one OLED frame is ~26 ms of I2C traffic — with priority 1 it gets preempted by every ADC sample and servo update, so slow rendering can never add jitter to control. Stats runs at priority 0, sharing the level with the idle task: pure bookkeeping that runs when nobody else wants the CPU (safe only because it sleeps ~99.9% of the time — a busy-loop at priority 0 would starve the idle task and trip the idle watchdog).

Stack sizes are per-task, not a blanket 4096 (rationale in `main/app_config.h`); the **Stats View shows each task's high-water mark in bytes** so you can verify the margins empirically and tune.

All tasks are created unpinned (`xTaskCreate`), so the SMP scheduler may run them on either core — none of the synchronization below depends on core affinity, which is the point: the primitives make the code correct regardless.

## Subsystems, one concept each

### Shared state & the mutex — `main/shared_state.c`
**What:** one `system_state_t` (ADC value, voltage, servo angle, display mode, button count, uptime) plus a stats side-struct, guarded by a single mutex from `xSemaphoreCreateMutex`.
**Concept: mutual exclusion + priority inheritance.** A mutex, not a binary semaphore, because mutexes give *priority inheritance*: if low-priority Stats holds the lock when high-priority Button wants it, Stats is temporarily boosted so it can't be preempted by medium-priority tasks while holding the lock — the classic priority-inversion fix. One mutex for everything keeps the design trivially deadlock-free (no lock-ordering rules to violate).
**The discipline:** take → touch a few fields → give, *immediately*. Nobody holds the mutex across I2C or any other slow I/O. The display task is the showcase of this: `shared_state_snapshot()` copies both structs under the lock (microseconds), then the ~26 ms OLED flush runs on the local copies with the lock free. Hold the lock during the flush instead, and every button press could stall ~26 ms behind a screen refresh.

### ADC Task — `main/adc_task.c`
**What:** samples GPIO34 every 50 ms with the **oneshot driver** (`esp_adc/adc_oneshot.h` — the legacy `driver/adc.h` is deprecated), applies an integer exponential moving average (α = 1/4, settles in ~8 samples), converts to volts via eFuse **line-fitting calibration** (nominal `raw·3300/4095` fallback if the chip has no calibration data), publishes to shared state, maps to 0–180°, and sends to the servo queue.
**Concepts:** *fixed-rate periodic task* — `vTaskDelayUntil` advances the wake time in exact 50 ms steps from a fixed reference, so the rate doesn't drift by however long the loop body took (plain `vTaskDelay` would accumulate that error). And the *producer* side of a queue.

### Servo Task — `main/servo_task.c`
**What:** blocks on the queue, converts degrees → LEDC duty (50 Hz, 14-bit → 1.22 µs/step ≈ 0.11°/step), writes the applied angle back to shared state so the display shows reality, not the request.
**Concepts:** *event-driven task* — blocked in `xQueueReceive(…, portMAX_DELAY)`, zero CPU between messages; no polling loop. Together with the time-driven ADC task it shows the two canonical task styles.
**The queue itself** is length 1, written with `xQueueOverwrite` — the *mailbox* pattern. For a position setpoint you always want the **latest** value: if the consumer ever falls behind, it should skip to the newest angle, not replay a backlog of stale ones. A deep queue is for data where every element matters (logs, packets); a mailbox is for "current value" semantics. Bonus: `xQueueOverwrite` never blocks the producer.

### Button — ISR + Task — `main/button_task.c`
**What:** falling edge on GPIO4 → ISR → binary semaphore → task debounces (30 ms, then re-check the level), increments `button_count`, toggles `display_mode`.
**Concept: deferred interrupt handling — the canonical pattern of this project.** The ISR is three lines: `xSemaphoreGiveFromISR`, `portYIELD_FROM_ISR`, return. Why not debounce or update state in the ISR?
- ISRs run above the scheduler: while one runs, no task runs on that core. Every µs in the ISR is latency added to *everything else*.
- Blocking APIs (`vTaskDelay`, plain `xSemaphoreTake`) are illegal in ISR context — you couldn't debounce there if you wanted to.
- Taking the shared-state mutex is impossible from an ISR (mutexes have no FromISR API, precisely because ownership/priority-inheritance make no sense in interrupt context).

So the ISR just *signals* and the real work happens in a task, where the full API is available. The pieces:
- **Binary semaphore, not counting:** contact bounce fires bursts of edges; a binary semaphore collapses a burst into one wake-up instead of queuing N phantom presses. A trailing `xSemaphoreTake(sem, 0)` drains any give that arrived during the debounce window — one physical press, one count.
- **`portYIELD_FROM_ISR(higher_prio_woken)`:** if the give unblocked the button task (priority 3 — higher than anything likely running), request the context switch *at ISR exit*, so the task runs immediately rather than at the next tick. This is why the button task has top priority: the semaphore is only useful for latency if the woken task actually preempts.
- **`IRAM_ATTR`:** the handler stays executable while the flash cache is off (e.g. during flash writes).

### Display Task — `main/display_task.c`
**What:** every 200 ms, snapshot shared state, render Sensor or Stats view into a RAM framebuffer, flush 1 KiB over I2C.
**Concepts:** the *snapshot pattern* (see mutex section) and *priority-based preemption* from the victim's perspective — this is the task that gets preempted mid-I2C by everything above it, harmlessly, because it holds no lock during I/O.

### Stats Task — `main/stats_task.c`
**What:** every 1 s records `esp_get_free_heap_size()`, minimum-ever heap, uptime, and `uxTaskGetStackHighWaterMark()` for all five tasks.
**Concept: task introspection.** The high-water mark is the *minimum free stack the task has ever had* — in **bytes** on the ESP-IDF port (vanilla FreeRTOS reports words). A value trending toward zero means the stack in `app_config.h` is undersized; keep ≥ ~512 B of measured headroom. Note it gathers everything *before* taking the mutex, then does five assignments under the lock — same discipline as everywhere else.

### Heartbeat — software timer, deliberately not a task — `main/heartbeat_timer.c`
**What:** `xTimerCreate`, 500 ms auto-reload, callback toggles GPIO2.
**Concept: software timers vs. tasks.** All timer callbacks execute in the context of the single timer service task ("Tmr Svc"), sharing its stack. Cost of this heartbeat: one ~48-byte timer struct. Cost as a task: ~2 KiB stack + TCB + a scheduler entry, to run two lines twice a second. The trade-off is that callbacks must **never block** — they'd stall every other timer in the system (this is also why the heartbeat doesn't touch shared state: nothing to lock means nothing to block on). Rule of thumb: short, non-blocking, periodic → timer; needs to block, wait, or take time → task. The blinking LED doubles as a system health indicator: it stops only if the timer task is starved.

## Startup order (`main/main.c`)

Mutex first, queue second, tasks after — a created task can start running *immediately* (if its priority beats `app_main`'s), so every object a task touches must exist before `xTaskCreate` returns. `app_main` then simply returns; the tasks and timer live on under the scheduler.

## Assumptions made (flagged per the spec)

- **SSD1306 driver: hand-rolled** (`main/ssd1306.c`, ~150 lines + font) instead of a managed component (e.g. `esp_lcd` + external font lib, or a third-party component). Reasons: zero external dependencies (builds offline with bare IDF), and the slow I2C flush is one explicit, visible function call — which matters when the whole lesson is "never hold the mutex across that call". `esp_lcd` has SSD1306 support but no text rendering, so it would still need a font layer.
- **I2C address 0x3C** (most 128×64 modules; some boards use 0x3D — change `SSD1306_I2C_ADDR` in `app_config.h`).
- **SG90 pulse range 500–2500 µs** for 0–180°. The datasheet says 1000–2000 µs but real SG90s need the wider range to reach full travel. If your servo buzzes at the end stops, narrow `SERVO_MIN/MAX_PULSE_US`.
- **Debounce: 30 ms** delay after the first edge, then the level must still be low. Covers typical 1–20 ms tactile-switch bounce.
- **ADC smoothing: EMA with α = 1/4** (integer math, ~400 ms settle). A moving average would work equally well; EMA needs no sample buffer.
- **ISR in default (non-IRAM) dispatch mode:** `gpio_install_isr_service(0)`; the handler itself is `IRAM_ATTR` but ultra-low-latency `ESP_INTR_FLAG_IRAM` dispatch isn't needed for a human button.
- **Tick rate 1000 Hz** via `sdkconfig.defaults` (see Build section).

## Future work

- **Event groups for startup synchronization** — the natural next primitive. Currently correctness relies on creation order in `app_main`. With an event group, each subsystem would set a bit (`xEventGroupSetBits`) when its hardware init completes, and every task would first block on `xEventGroupWaitBits(ALL_READY_BITS)` — multi-bit AND synchronization that no combination of queues/semaphores expresses cleanly. Deliberately left out of this pass to keep one-concept-per-subsystem.
- Networking (WiFi/BT) — out of scope for this showcase; note that enabling WiFi is why the pot sits on ADC1.
- `vTaskGetRunTimeStats()` for CPU-percentage per task in the Stats view (needs a run-time counter enabled in menuconfig).
