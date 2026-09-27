# Pico learning experiments

A series of small RP2040 (Raspberry Pi Pico) projects exploring what "learning" can
mean on a microcontroller — from plain digital I/O, up through tiny neural nets
trained on-chip, to running an actual small language model on the Pico itself.

Each project is a standalone [Pico SDK](https://github.com/raspberrypi/pico-sdk)
CMake project. Build any one of them with the standard SDK flow:

```bash
cd <project-dir>
mkdir build && cd build
cmake ..
make
# flash the resulting <project>.uf2 by holding BOOTSEL while plugging in the Pico
```

## Foundations — no learning, just I/O

- **`picobutton/`** — the simplest possible starting point: read the BOOTSEL button,
  drive the onboard LED. Pure digital I/O, no model involved.
- **`blinkstream/`** — blinks the LED and streams the raw GPIO register over USB each
  step. The register dump is the training data for a Pi-side observer
  (`learn_led.py`) to figure out, from the bit pattern alone, which bit is the LED.

## Learning small things

- **`xorled/`** — the smallest neural net that can learn XOR: 2 inputs, 2 hidden
  neurons, 1 output, 9 weights total. `train_xor.py` trains it from scratch with
  plain NumPy (no ML framework — the gradient descent is hand-written), then the
  learned weights (`weights.h`) get compiled into `xorled.c`, which runs the exact
  same forward-pass math live on the Pico.
- **`learnled/`** — the same 9-weight XOR net, but trained *on the Pico itself* at
  boot — no Python step. It prints the weights it discovered over USB, then drives
  the LED live using what it learned.
- **`rawbyte/`** — learns, on-chip, which raw USB byte means "LED on" vs. "LED off"
  directly from the bits — no `if` statement decodes the byte, the model does.

## Learning without being told the answer

- **`regdrive/`** — the Pico is a "dumb" register box: it executes whatever register
  write it's told over USB and reports the resulting LED state, nothing more. The
  actual learning happens on the Pi side (`drive_learn.py`): given only the goal
  ("LED on") and the LED feedback, it has to discover *by trial and reward* which
  GPIO register write controls the light — it's never told bit 25 is the LED.
  `drive_blink.py` then proves the learning actually worked: it blinks *only* the
  bit the model discovered, nothing hardcoded.
- **`picolearn/`** — combines two learned models entirely on-chip: one learns to
  decode a raw byte into an on/off goal, the other learns which GPIO bit is the
  LED. No hardcoded decoding anywhere in the pipeline.

## Learning the whole setup, not just the toggle

- **`setupdrive/`** — extends `regdrive`'s idea to the setup step itself: candidate
  pins start completely unconfigured (no mux, no output-enable), and a Pi-side
  script (`setupdrive_learn.py`) has to discover *all of it* — which pin, which
  mux value, whether output-enable is even on — not just which value toggles the
  light. Same "dumb box, learn by reward" architecture as `regdrive`, just a
  bigger unknown.
- **`setuplearn/`** — the fully on-chip version: no host script, no serial commands
  controlling it. The Pico runs the same trial-and-reward search itself, at boot,
  then blinks the LED forever using only what it found. Nothing about pin
  configuration is written in the source as an answer — only the search space
  (candidate pins, candidate mux values) and where to read the real LED's state
  from are fixed.
- **`trainedled/`** — takes what `setuplearn` discovered and skips re-learning it:
  the pin/mux/output-enable are baked in as known-good constants (learned once,
  not re-searched every boot), plus a simple fixed keyboard command (`a`=on,
  `s`=off) on top.
- **`fulllearn/`** — combines everything: `setuplearn`'s on-chip setup+on/off
  search, plus a *second*, independently-trained tiny model that learns which two
  keys you choose — live, at boot, not hardcoded — mean "on" and "off". Nothing in
  this one is fixed except USB bring-up and the shape of the search itself; the
  pin, the mux, the output-enable, the value, and even which keys control it are
  all discovered live, in one boot.

## Running an actual language model

- **`picollama260k/`** — [Andrej Karpathy's `llama2.c`](https://github.com/karpathy/llama2.c)
  ported to run entirely on an RP2040. The transformer math (forward pass,
  tokenizer, sampler) is Karpathy's, unchanged; the Pico-specific work is
  everything around it — flash-backed weight loading instead of `mmap`, USB-serial
  I/O instead of stdio, and capping the context length so the KV-cache fits in
  264KB of SRAM. Runs the [`stories260K`](https://huggingface.co/karpathy/tinyllamas/tree/main/stories260K)
  checkpoint — also Karpathy's, not trained by this project. See
  `picollama260k/THIRD_PARTY_LICENSES.txt` for full attribution.

## What's not in this repo

`pico-sdk/` and `pico-examples/` are Raspberry Pi's own official repositories,
referenced by every project here via the standard `pico_sdk_import.cmake`
mechanism, not vendored into this repo. Point `PICO_SDK_PATH` at your own clone
of [raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk) to build any
project here.
