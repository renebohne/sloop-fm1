# Implementation Plan: FLOYD Visual FM Synthesizer Engine

**Target Engine Name:** `FLOYD`  
**Target Repository:** `sloop-fm1`  
**Branch:** `feature/floyd-fm-engine`  
**Reference Specification:** [`temp/fm_ui_specification.md`](file:///Users/rene/Documents/github/sloop-fm1/temp/fm_ui_specification.md)

---

## 1. Executive Summary & Strategy

We are implementing a brand-new, dedicated 4-operator FM synthesis engine named **`FLOYD`** for the FM-1 hardware running the Sloop firmware.

### Key Advantages of a Dedicated Engine:
- **Zero Risk to Existing Engines:** The existing classic `DIGITAL` (FM) engine and presets remain 100% untouched and backward-compatible.
- **Dedicated DSP Optimization:** Tailored specifically to the 4 Floyd Steinberg algorithms and the unified Option B Decay/Sustain macro envelope response.
- **Custom Visual Experience:** Dedicated full-screen visualization featuring the **12-Step Spectral Gradient** (Deep Navy Blue $\rightarrow$ Fiery Red), **Ghost Modulator Overlays** (Op2 Blue, Op3 Green, Op4 Amber), **Interactive Halo Cursor**, and **Live Real-time Note Tracers**.

---

## 2. Architecture & Data Structures

### 2.1 Engine Registration (`firmware/src/core.h` & `firmware/src/engines.c`)
- Increase `NENGINES` from `(9 + FELUCCA_SLICE)` to `(10 + FELUCCA_SLICE)`.
- Create [`firmware/src/eng_floyd.c`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/eng_floyd.c).
- Include `eng_floyd.c` and register `&ENG_FLOYD` in `ENGINES[]` in [`firmware/src/engines.c`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/engines.c).

### 2.2 Parameter Mapping (`P_E0` .. `P_E7`)
The 8 engine parameters are mapped to two pages (**ALGO** and **MOD** / **TIMBRE**):

| Param | ID | Name | Type / Range | Default | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `P_E0` | `ALG` | Algorithm | `F_ENUM` (0..3) | `0` | 0: 3-to-1 Parallel, 1: Dual Carrier, 2: Serial 4-Stack, 3: Additive Organ |
| `P_E1` | `R2` | Ratio 2 | `F_ENUM` (0..14) | `1` (x1.0) | Multiplier for Operator 2 (`0.5` .. `16.0`) |
| `P_E2` | `R3` | Ratio 3 | `F_ENUM` (0..14) | `2` (x2.0) | Multiplier for Operator 3 (`0.5` .. `16.0`) |
| `P_E3` | `R4` | Ratio 4 | `F_ENUM` (0..14) | `4` (x4.0) | Multiplier for Operator 4 (`0.5` .. `16.0`) |
| `P_E4` | `DEPTH` | Mod Depth | `F_PCT` (0..127) | `64` | Master modulation intensity index |
| `P_E5` | `DSUS` | Dec/Sus Macro | `F_TIME` (0..127)| `60` | Option B unified Decay ($0..63$) & Sustain ($64..127$) |
| `P_E6` | `FDBK` | Op 4 Feedback| `F_PCT` (0..127) | `0` | Feedback loop on Modulator 4 |
| `P_E7` | `OP` | OP Mode Focus| `F_ENUM` (0..4) | `0` | `OP1` (Carrier), `OP2`, `OP3`, `OP4`, or `ALGO` |

---

## 3. DSP Synthesis Engine Design (`firmware/src/eng_floyd.c`)

### 3.1 The 4 Core Algorithms
1. **Algorithm 0 (Floyd Steinberg 3-to-1 Parallel):**
   - Routing: $(4 + 3 + 2) \rightarrow 1$
   - Modulators $O_4$ (with feedback), $O_3$, $O_2$ summed to modulate Carrier $O_1$.
   - Output: $s = \sin(\phi_1 + \text{mod}( (o_2 + o_3 + o_4)/3, \text{idx} ))$.
2. **Algorithm 1 (Dual Carrier / Dual Tone):**
   - Routing: $2 \rightarrow 1$ and $4 \rightarrow 3$
   - Carrier 1 ($O_1$) modulated by $O_2$; Carrier 2 ($O_3$) modulated by $O_4$.
   - Output: $s = (o_1 + o_3) / 2$.
3. **Algorithm 2 (Serial Cascade 4-Stack):**
   - Routing: $4 \rightarrow 3 \rightarrow 2 \rightarrow 1$
   - $O_4 \rightarrow O_3 \rightarrow O_2 \rightarrow O_1$.
   - Rich harmonic cascading synthesis.
4. **Algorithm 3 (Organ / Additive Parallel):**
   - Routing: $1 + 2 + 3 + 4 \rightarrow \text{Out}$
   - 4 additive pure sine drawbar tones mixed into audio output.

### 3.2 "Option B" Decay / Sustain Macro Math
- **Percussive Region ($x \in [0, 63]$):**
  - Sustain target $L_S = 0$.
  - Decay time scales from $10\text{ ms} \dots 4000\text{ ms}$ via `ENV_EXP[(p[P_E5] * 2)]`.
- **Sustained / Pad Region ($x \in [64, 127]$):**
  - Decay fixed at maximum ($4000\text{ ms}$).
  - Sustain target scales linearly from $0\% \dots 100\%$ ($0 \dots 1 << 24$).
- Modulator envelope state `v->s[4]` tracks the target level per block and scales `idx`.

### 3.3 Factory Presets for `FLOYD`
1. `FLOYD TINE` — Classic dynamic EP / bell tine with responsive touch.
2. `DUAL LEAD` — Punchy dual-carrier cutting synthesizer lead.
3. `STACK BASS` — Thick, aggressive 4-stack cascading FM bassline.
4. `ORGAN 4` — 4-drawbar additive jazz / gospel organ.
5. `CRYSTAL` — Shimmering ambient bells with long decay.
6. `METALLIC` — Industrial metallic percussion hit with Op 4 feedback.
7. `WARM SINE` — Deep, mellow analog-style rounded sub keys.
8. `VELVET PAD` — Evolving dual-carrier warm stereo pad.

---

## 4. UI Drawing & Visual Rendering Engine

### 4.1 Color Lookup Tables & Palettes (`firmware/src/gfx.c`)
- **12-Step Spectral Gradient (`SPECTRAL_LUT_RGB565`):**
  ```c
  static const uint16_t SPECTRAL_LUT_RGB565[12] = {
      0x1A0D, /* Step 0:  Deep Navy Blue (Pure Sine) */
      0x1128, /* Step 1:  Midnight Blue */
      0x18CA, /* Step 2:  Deep Indigo */
      0x51B0, /* Step 3:  Rich Violet */
      0x7914, /* Step 4:  Purple-Magenta */
      0xA08D, /* Step 5:  Magenta */
      0xC007, /* Step 6:  Deep Crimson */
      0xE120, /* Step 7:  Orange-Red */
      0xF2B1, /* Step 8:  Coral Pink */
      0xFAAA, /* Step 9:  Electric Orange */
      0xFBC8, /* Step 10: Bright Amber */
      0xF900  /* Step 11: Fiery Red (Peak Modulation) */
  };
  ```
- **Ghost Modulator Colors:**
  - `GHOST_OP2 = RGB(24, 60, 140)` (Translucent Blue)
  - `GHOST_OP3 = RGB(20, 100, 40)` (Translucent Green)
  - `GHOST_OP4 = RGB(140, 100, 20)` (Translucent Amber)

### 4.2 Graph Viewport Layout (`graph_floyd` in `firmware/src/ui_draw.c`)
1. **Header Bar ($y = 0 \dots 14$):**
   - Displays active Algorithm, Carrier assignments, focused Operator, Ratio multiplier, and Modulation Depth.
   - Format: `[ALG: 1] CARRIER: OP1 | EDIT: OP2 (MOD) | R: 2.0 | DEPTH: 65%`
2. **Main Canvas ($y = 16 \dots 96$):**
   - **Ghost Overlays:** Dashed 1 px curves for inactive modulators showing their envelope shapes.
   - **Carrier Polyline:** 2-3 px thick polyline colored per pixel according to instantaneous modulation index sampled from `SPECTRAL_LUT_RGB565`.
   - **Subtle Gradient Fill:** Soft vertical shading below the carrier curve.
   - **Interactive Halo Cursor:** Hollow 5x5 cursor ring highlighting the node currently being edited.
   - **Live Note Tracer Dot:** Moving white dot tracking real-time playback and voice envelope amplitude.

---

## 5. Step-by-Step Implementation Roadmap

1. [ ] **Core Setup:** Update [`firmware/src/core.h`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/core.h) (`NENGINES`).
2. [ ] **DSP Engine:** Create [`firmware/src/eng_floyd.c`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/eng_floyd.c) with 4 algorithms, Option B math, and 8 factory presets.
3. [ ] **Engine Dispatch:** Update [`firmware/src/engines.c`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/engines.c).
4. [ ] **Visual Palette:** Add `SPECTRAL_LUT_RGB565` and ghost colors to [`firmware/src/gfx.c`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/gfx.c).
5. [ ] **UI Drawing:** Implement `graph_floyd()` in [`firmware/src/ui_draw.c`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/ui_draw.c) and wire into `draw_graph()`.
6. [ ] **Icons & Labels:** Update [`firmware/src/icons.c`](file:///Users/rene/Documents/github/sloop-fm1/firmware/src/icons.c) for `DSUS` and `FLOYD`.
7. [ ] **Build & Test:** Compile with `./build.sh` to generate firmware binaries (`felucca.bin`, `felucca.fwsc`) and verify DSP & UI.
8. [ ] **Commit & Push:** Commit to branch `feature/floyd-fm-engine` and push to GitHub.
