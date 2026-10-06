# Floyd 4-Operator FM Engine & Vector UI: Architecture, Learnings & Release Notes

## 1. Overview & Accomplishments
In SLOOP 2.4, we designed and implemented a full **Floyd 4-Operator FM Synthesizer Engine** on the M-VAVE FM-1 (Felucca codebase), complete with:
- 8 classic Yamaha DX-style FM routing algorithms.
- 7-page visual UI architecture (`ALGO 1/7`, `OP1 2/7`..`OP4 5/7`, `LEVEL 6/7`, `RATIO 7/7`).
- Per-operator ADSR envelopes with individual carrier/modulator color coding.
- Live real-time vector-locked ADSR playhead tracer dots across active voices.
- Full test coverage, golden hashes, and automated static site deployment to GitHub Pages (`docs/` -> `http://www.rene-bohne.de/sloop-fm1/`).

---

## 2. Floyd Engine Architecture

### Parameter Mapping (`params.c` & `eng_floyd.c`)
- **ALGO (1/7)**: `P_E0` (Algorithm 0..7), `P_E7` (Feedback 0..127).
- **OP1..OP4 (2/7 .. 5/7)**:
  - OP1 (Carrier): Uses core track parameters `P_ATK`, `P_DEC`, `P_SUS`, `P_REL`.
  - OP2..OP4 (Modulators): Uses `floyd_state[part].atk[k]`, `.dec[k]`, `.sus[k]`, `.rel[k]`.
- **LEVEL (6/7)**: `floyd_state.lvl[0]`, `P_E4` (OP2 Level), `P_E6` (OP3 Level), `floyd_state.lvl[3]`.
- **RATIO (7/7)**: `P_E1` (OP2 Ratio), `P_E2` (OP3 Ratio), `P_E3` (OP4 Ratio), `P_E7` (Feedback).

### State Storage in `voice_t.s[8]`
A critical fix was made to voice state indexing in `eng_floyd.c`. The 8 slots of `v->s` are partitioned cleanly with zero collisions:
- `s[0]`: OP2 envelope level (Q24, $0 \dots 2^{24}$)
- `s[1]`: OP3 envelope level (Q24)
- `s[2]`: OP4 envelope level (Q24)
- `s[3]`: OP2 stage ($1 = \text{Attack}, 2 = \text{Decay/Sustain}, 3 = \text{Release}, 0 = \text{Off}$)
- `s[4]`: OP3 stage
- `s[5]`: OP4 stage
- `s[6]`: OP4 1-sample feedback history ($fb_1$)
- `s[7]`: OP4 oscillator phase ($ph_4$)
- `v->ph[0..2]`: OP1, OP2, OP3 oscillator phases.

---

## 3. Vector-Locked Live ADSR Tracer Mathematics

### Problem Identified
Previously, the playhead dot ($tx, ty$) tried to advance $tx$ via linear fixed-point ratios independently of $ty$. Because audio decay/release is exponential (`mulq16`), $ty$ plummeted fast while $tx$ remained stuck, causing vertical dropping artifacts at vertex $x_1$ (decay) and $x_3$ (release).

### Solution: Algebraic Line Segment Coupling
To guarantee the point $(tx, ty)$ is always 100% on the drawn diagonal vector line:

1. **Attack Phase ($stage == 1$):**
   - Segment: $(x_0, y_0) \to (x_1, y_1)$ where $y_0 = y_{\text{bot}}$, $y_1 = y_{\text{bot}} - pk$.
   - $ty = \text{clamp}(y_{\text{bot}} - \frac{\text{env\_val} \cdot pk}{2^{24}}, y_1, y_0)$.
   - $$tx = x_0 + (x_1 - x_0) \cdot \frac{y_0 - ty}{y_0 - y_1}$$

2. **Decay Phase ($stage == 2$, $ty < y_2$):**
   - Segment: $(x_1, y_1) \to (x_2, y_2)$ where $y_2 = y_{\text{bot}} - \frac{s\_lvl \cdot pk}{h}$.
   - $$tx = x_1 + (x_2 - x_1) \cdot \frac{ty - y_1}{y_2 - y_1}$$

3. **Sustain Plateau ($stage == 2$, $ty \ge y_2$):**
   - Segment: $(x_2, y_2) \to (x_3, y_2)$.
   - $ty = y_2$
   - $$tx = x_2 + \text{clamp}(\text{age} \times 2, 0, x_3 - x_2)$$

4. **Release Phase ($stage == 3$):**
   - Segment: $(x_3, y_3) \to (x_4, y_4)$ where $y_3 = y_2$, $y_4 = y_{\text{bot}}$, $x_4 = x_0 + w$.
   - $ty = \text{clamp}(ty_{\text{env}}, y_3, y_4)$
   - $$tx = x_3 + (x_4 - x_3) \cdot \frac{ty - y_3}{y_{\text{bot}} - y_3}$$

### Ghost Dot Prevention
- In `eng_floyd.c`, when $v\rightarrow s[s\_idx] < (1 \ll 12)$, we explicitly zero both $v\rightarrow s[s\_idx] = 0$ and $v\rightarrow s[st\_idx] = 0$ (stage off).
- In `ui_draw.c`, we filter out dead voices: `if (stage <= 0 || stage > 3 || env_val <= 0) continue;`.
- In `graph_signature()`, `h ^= v_active + (ui.frame / 2u) * (v_active != 0);` ensures continuous frame updates until all voices have completely faded out.

---

## 4. Build, Test & Release Workflow

### Build Toolchain
- **Compiler:** JieLi Toolchain running in Linux container / Docker environment.
- **Build Command:** `PYTHON=.venv/bin/python3 ./build.sh --release 2.4`
  - Generates `build/felucca.bin`, `build/loader/ota.bin`, and `build/felucca-2.4.fwsc` with product identity `FM-1_924`.

### Test Suite Execution
- **Run Tests:** `SOAK_MIN=1 sh tests/run_tests.sh`
- **Update Golden Hashes (when audio algorithms intentionally improve):**
  `GOLDEN_UPDATE=1 SOAK_MIN=1 sh tests/run_tests.sh`

### Web Installer & GitHub Pages Deployment
- **Web Build Script:** `.venv/bin/python3 web/make_site.py build/felucca-2.4.fwsc 2.4 docs`
- **Hosting:** GitHub Pages is configured to serve from `main:/docs` at `http://www.rene-bohne.de/sloop-fm1/`.
- **Release:**
  - Git tag `v2.4` pushed to `origin`.
  - GitHub release created via `gh api` with attached package `sloop-2.4.fwsc`.
