# Firmware Specification: 4-Operator FM Synthesizer UI & Hardware Interface

* **Document Version:** 1.0.0
* **Target Hardware:** Embedded MCU (e.g., ESP32-P4 / ESP32-S3 / STM32 / RP2350)
* **Display:** Non-touch color TFT/IPS display (e.g., $320 \times 240$, $480 \times 320$, or larger)
* **Controls:** 4× incremental rotary encoders (without integrated push buttons)
* **Document Purpose:** Complete architectural and functional blueprint for firmware implementation.

---

## 1. Executive Summary & Core Concept

This document defines the implementation of a 4-Operator FM Synthesizer control interface. Traditional FM interfaces suffer from high cognitive load because envelope graphs only show amplitude over time, completely hiding the resulting timbre (spectral brightness).

Our solution adapts the visual paradigm introduced by Floyd Steinberg and translates it to **a non-touch, 4-knob hardware workflow**:
1. **Spectral Brightness Mapping:** The amplitude envelope of the Carrier operator is drawn with a live dynamic color gradient from **Deep Blue** (pure sine wave, no overtones) to **Bright Red** (high modulation index, dense overtone spectrum).
2. **Ghost Overlays:** Inactive modulators are faintly rendered in the background, providing immediate context for which phase causes spectral changes.
3. **Ergonomic "Option B" Knob Mapping:** To compensate for encoders without push buttons, Decay and Sustain are mapped to a single unified macro-encoder, optimizing control for real-world FM synthesis workflows.

---

## 2. Hardware Interface & Encoder Mapping

The physical interface consists strictly of four rotary encoders labeled **ENC 1** to **ENC 4** (left to right).

```
 +-------------------------------------------------------+
 |                      COLOR DISPLAY                    |
 +-------------------------------------------------------+
     (ENC 1)           (ENC 2)        (ENC 3)        (ENC 4)
  OP / MODE SELECT      ATTACK     DECAY / SUSTAIN   RELEASE
```

### 2.1 Encoder 1: Context & Mode Selection
Encoder 1 is an indexed detent selector with 5 distinct states:

| Value / Position | Target Mode | Function of Encoders 2, 3, 4 |
| :--- | :--- | :--- |
| `0` | **Operator 1 (Carrier)** | Controls Envelope of OP 1 |
| `1` | **Operator 2** | Controls Envelope of OP 2 |
| `2` | **Operator 3** | Controls Envelope of OP 3 |
| `3` | **Operator 4** | Controls Envelope of OP 4 |
| `4` | **ALGO / GLOBAL** | Configures Synthesizer Routing (see Section 5) |

---

### 2.2 Encoders 2, 3, 4 (Envelope Editing Modes: OP 1 to OP 4)

When Encoder 1 selects an operator (`OP 1`–`OP 4`), the remaining three encoders act directly on that operator's envelope:

#### Encoder 2: Attack Time ($t_A$)
* **Range:** $1.0\text{ ms} \dots 5000.0\text{ ms}$ (Logarithmic mapping).
* **Display Output:** e.g., `A: 12 ms` or `A: 1.45 s`.

#### Encoder 3: "Option B" Combined Decay / Sustain Macro
FM modulators require $Sustain = 0\%$ in $85\%$ of natural sounds (plucks, bells, keys, brass hits). To avoid submenus on buttonless encoders, Encoder 3 maps both Decay Time ($t_D$) and Sustain Level ($L_S$) across its $0 \dots 127$ tick range:

* **Segment 1: Percussive Range ($Ticks \in [0, 63]$)**
  * $L_S = 0.0$ (Sustain level locked at $0\%$).
  * $t_D$ scales logarithmically from $10.0\text{ ms} \dots 4000.0\text{ ms}$.
  * UI readout: `D: 340 ms` (Sustain indicator dim/hidden).
* **Segment 2: Sustained / Pad Range ($Ticks \in [64, 127]$)**
  * $t_D$ locked at $4000.0\text{ ms}$ (maximum decay).
  * $L_S$ scales linearly from $0.0 \dots 1.0$ ($0\% \dots 100\%$).
  * UI readout: `D: MAX | S: 65%`.

**Mathematical Definition for Firmware:**
Given an encoder normalized value $x \in [0.0, 1.0]$:

$$\text{If } x \le 0.5: \quad \begin{cases} t_D(x) = 10 \cdot \left(\frac{4000}{10}\right)^{2x} \text{ [ms]} \\ L_S(x) = 0.0 \end{cases}$$

$$\text{If } x > 0.5: \quad \begin{cases} t_D(x) = 4000.0 \text{ [ms]} \\ L_S(x) = 2 \cdot (x - 0.5) \end{cases}$$

#### Encoder 4: Release Time ($t_R$)
* **Range:** $5.0\text{ ms} \dots 8000.0\text{ ms}$ (Logarithmic mapping).
* **Display Output:** e.g., `R: 450 ms`.

---

### 2.3 Encoders 2, 3, 4 (ALGO / GLOBAL Mode)

When Encoder 1 is turned to position `4` (`ALGO`), the encoder assignments rebind dynamically:
* **Encoder 2 (Algorithm Preset):** Cycles through algorithm presets ($1 \dots 4$).
* **Encoder 3 (Modulation Strength / Index Scale):** Master multiplier $k_{mod} \in [0.0, 4.0]$.
* **Encoder 4 (Master Frequency Ratio of selected OP):** Steps through integer and musical ratios ($0.5, 1.0, 2.0, 3.0, 4.0 \dots$).

---

## 3. Display Layout & UI Architecture

The screen is split into three functional layers:

```
+-------------------------------------------------------------------+
| [ALGO: 1]  CARRIER: OP1  |  EDITING: OP2 (MODULATOR)  | RATIO: 2.0| (Header: 12%)
+-------------------------------------------------------------------+
|                                                                   |
|         --- Ghost Modulator Curve (OP3)                           |
|      /                                                            |
|    /     ================ CARRIER CURVE ================          |
|  /     / (Deep Blue) -> (Bright Red Peak) -> (Fade Blue) \        | (Stage: 73%)
| /    /                                                    \       |
|/___/_______________________________________________________\_____ |
+-------------------------------------------------------------------+
|  [ENC 1: SELECT]   |  [ENC 2: ATTACK]  | [ENC 3: DEC/SUST] | [ENC 4: REL] | (Footer: 15%)
|      OP 2 (MOD)    |      15 ms        |    D: 450 ms      |    820 ms    |
+-------------------------------------------------------------------+
```

1. **Header Bar (12% height):** Displays current Algorithm diagram, active editing operator, and current ratio.
2. **Main Oscilloscope / Envelope Canvas (73% height):**
   * **Background:** Inactive modulator envelopes rendered as faint, translucent lines ("Ghost curves").
   * **Foreground:** Carrier envelope(s) rendered as thick ($3\text{ px}$) anti-aliased polyline with per-segment spectral gradient coloring.
   * **Cursor Node:** A hollow circular halo ($6\text{ px}$ diameter) tracks the node currently influenced by the active encoder.
3. **Footer Status / Encoder Strip (15% height):** Four dedicated boxes positioned directly above the physical knobs, displaying real-time numerical values.

---

## 4. Color Palette & Gradient Specification

To guarantee readability on standard industrial and hobbyist TFT displays (RGB565 or RGB888), use the following precise color values:

### 4.1 System & Canvas Colors

| Element | Hex Code | RGB888 | Purpose |
| :--- | :--- | :--- | :--- |
| **Canvas Background** | `#0D1117` | `(13, 17, 23)` | Deep dark charcoal (reduces glare, enhances contrast) |
| **Grid / Axis Lines** | `#21262D` | `(33, 38, 45)` | Subtle 0dB and time interval division lines |
| **Text Primary** | `#F0F6FC` | `(240, 246, 252)` | High-contrast white for values |
| **Text Dimmed / Units** | `#8B949E` | `(139, 148, 158)` | Secondary labels (`ms`, `Hz`, `%`) |
| **Active Focus Accent** | `#58A6FF` | `(88, 166, 255)` | Indicates selected operator / active boundary |

### 4.2 Ghost Modulator Lines (Background)

| Operator | Hex Code | Alpha / Blending | Visual Weight |
| :--- | :--- | :--- | :--- |
| **OP 2 Ghost** | `#388BFD40` | $25\%$ Opacity | $1\text{ px}$ dashed line |
| **OP 3 Ghost** | `#3FB95040` | $25\%$ Opacity | $1\text{ px}$ dashed line |
| **OP 4 Ghost** | `#D2992240` | $25\%$ Opacity | $1\text{ px}$ dashed line |

---

### 4.3 12-Step Spectral Gradient Palette (Lookup Table)

The Carrier line color is selected from a precomputed 12-step look-up table based on the normalized instantaneous modulation index $I_{\text{norm}}(t) \in [0.0, 1.0]$:

```
[0.0] Blue (Pure Sine) --------------------------> Red (Saturated Overtones) [1.0]
```

| Index | Normalized Value | Hex Code | RGB888 | Visual Perceived Timbre |
| :---: | :---: | :--- | :--- | :--- |
| **0** | $0.000$ | `#1F4068` | `(31, 64, 104)` | Pure fundamental sinus (dull, dark) |
| **1** | $0.091$ | `#162447` | `(22, 36, 71)` | Warm, subtle low-order overtone |
| **2** | $0.182$ | `#1B1A55` | `(27, 26, 85)` | Deep blue-violet, gentle presence |
| **3** | $0.273$ | `#533483` | `(83, 52, 131)` | Rich purple, initial audible sidebands |
| **4** | $0.364$ | `#7B1FA2` | `(123, 31, 162)` | Electric violet, bright brass texture |
| **5** | $0.455$ | `#A2126C` | `(162, 18, 108)` | Magenta, metallic bite |
| **6** | $0.545$ | `#C70039` | `(199, 0, 57)` | Crimson red, cutting edge |
| **7** | $0.636$ | `#E02401` | `(224, 36, 1)` | Bright orange-red, aggressive lead |
| **8** | $0.727$ | `#F35588` | `(243, 85, 136)` | Hot coral, heavy sidebands |
| **9** | $0.818$ | `#FF5722` | `(255, 87, 34)` | Brilliant electric orange |
| **10**| $0.909$ | `#FF7844` | `(255, 120, 68)` | Intense amber, near-noise saturation |
| **11**| $1.000$ | `#FF1E00` | `(255, 30, 0)` | Saturated flame red (maximum FM clash) |

---

## 5. Supported Algorithms & Gradient Mathematics

The firmware must support 4 primary routing algorithms.

### 5.1 Algorithm Definitions

* **Algorithm 1 (Floyd Steinberg Default: 1 Carrier, 3 Parallel Modulators):**
  * Routing: $(\text{OP4} + \text{OP3} + \text{OP2}) \rightarrow \text{OP1 (Carrier)}$
  * Display: 1 Main Curve (OP1) + 3 Ghost Curves.
* **Algorithm 2 (Dual Carrier / Dual Tone: 2 Carriers, 2 Modulators):**
  * Routing: $\text{OP2} \rightarrow \text{OP1 (Carrier 1)}$ and $\text{OP4} \rightarrow \text{OP3 (Carrier 2)}$
  * Display: 2 Foreground Curves (OP1 and OP3). OP1 receives color from OP2; OP3 receives color from OP4.
* **Algorithm 3 (Serial Cascade / 4-Stack):**
  * Routing: $\text{OP4} \rightarrow \text{OP3} \rightarrow \text{OP2} \rightarrow \text{OP1 (Carrier)}$
  * Display: 1 Main Curve (OP1) with non-linear cascading color derivation.
* **Algorithm 4 (Organ / Additive Mode: 4 Parallel Carriers):**
  * Routing: $\text{OP1} + \text{OP2} + \text{OP3} + \text{OP4} \rightarrow \text{Audio Out}$
  * Display: 4 Foreground Curves. Since modulation index is zero, all curves remain pure Blue (`#1F4068`).

---

### 5.2 Mathematical Formulation for Gradient Sampling

Let the display canvas width be $W$ pixels ($x \in [0, W-1]$). Each pixel column $x$ represents a normalized time slice $t$.

#### For Parallel Modulators (Algorithm 1):
The instantaneous modulation index $I(x)$ is the normalized arithmetic average of all modulator amplitudes $Y_m(x) \in [0.0, 1.0]$ scaled by their depth parameter $D_m \in [0.0, 1.0]$:

$$I(x) = \frac{1}{N_{mod}} \sum_{m \in Modulators} (Y_m(x) \cdot D_m)$$

$$\text{LUT Index}(x) = \text{clamp}\left( \left\lfloor I(x) \cdot 11.99 \right\rfloor, 0, 11 \right)$$

#### For Cascaded / Serial Chains (Algorithm 3):
In a serial stack, if an intermediate modulator has zero amplitude, upstream modulation cannot pass through. The effective index is calculated multiplicatively:

$$I(x) = Y_2(x) \cdot D_2 \cdot \left(1.0 + Y_3(x) \cdot D_3 \cdot (1.0 + Y_4(x) \cdot D_4)\right)$$

This value is subsequently normalized against maximum scaling before mapping into the 12-color LUT.

---

## 6. Firmware Data Structures & State Machine

```c
#ifndef FM_SYNTH_UI_H
#define FM_SYNTH_UI_H

#include <stdint.h>
#include <stdbool.h>

#define NUM_OPERATORS 4
#define LUT_STEPS 12
#define DISPLAY_WIDTH 320

typedef enum {
    MODE_OP1 = 0,
    MODE_OP2 = 1,
    MODE_OP3 = 2,
    MODE_OP4 = 3,
    MODE_ALGO = 4
} UI_Mode_t;

typedef enum {
    ALGO_3_TO_1_PARALLEL = 0, // Floyd Steinberg setup
    ALGO_DUAL_CARRIER    = 1, // 2x (Mod -> Carrier)
    ALGO_SERIAL_CASCADE  = 2, // 4 -> 3 -> 2 -> 1
    ALGO_ALL_PARALLEL    = 3  // Additive (4 Carriers)
} FM_Algorithm_t;

typedef struct {
    uint16_t attack_ms;       // 1 - 5000 ms
    uint16_t decay_ms;        // 10 - 4000 ms
    float    sustain_level;   // 0.0f - 1.0f
    uint16_t release_ms;      // 5 - 8000 ms
    float    depth;           // 0.0f - 1.0f (Modulation intensity)
    float    ratio;           // 0.5f - 16.0f (Frequency multiplier)
    bool     is_carrier;
} Operator_t;

typedef struct {
    UI_Mode_t      active_mode;
    FM_Algorithm_t current_algo;
    Operator_t     ops[NUM_OPERATORS];
    bool           ui_is_dirty; // Set when encoders turn; cleared after render
} SynthState_t;

// Color Lookup Table (RGB565 format for hardware LCD controllers)
static const uint16_t SPECTRAL_LUT_RGB565[LUT_STEPS] = {
    0x1A0D, // Step 0:  Deep Navy Blue
    0x1128, // Step 1:  Midnight Blue
    0x18CA, // Step 2:  Deep Indigo
    0x51B0, // Step 3:  Rich Violet
    0x7914, // Step 4:  Purple-Magenta
    0xA08D, // Step 5:  Magenta
    0xC007, // Step 6:  Deep Crimson
    0xE120, // Step 7:  Orange-Red
    0xF2B1, // Step 8:  Coral Pink
    0xFAAA, // Step 9:  Electric Orange
    0xFBC8, // Step 10: Bright Amber
    0xF900  // Step 11: Fiery Red (Max FM brightness)
};

// Core Prototypes
void UI_Init(SynthState_t* state);
void UI_HandleEncoderTicks(SynthState_t* state, int8_t e1, int8_t e2, int8_t e3, int8_t e4);
void UI_Render(const SynthState_t* state);

#endif // FM_SYNTH_UI_H
```

---

## 7. Performance & Real-Time Considerations

1. **Dirty Flag Execution:** Redrawing the canvas must happen **only** when `ui_is_dirty == true`. When keys are pressed via MIDI or audio is synthesizing, UI drawing loops are bypassed to conserve $100\%$ CPU for DSP processing.
2. **Scanline Rendering:** To prevent screen flickering on non-buffered displays, do not clear the whole screen with black. Draw the polyline segments directly into an offscreen line-buffer or use DMA double-buffering provided by frameworks like LVGL.
3. **Decoupled Math:** Envelope pixel points are computed via look-up tables rather than calling math functions (`pow`, `log`) within the rendering inner-loop.