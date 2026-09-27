#!/usr/bin/env python3
# learn_led.py — a tiny model that WATCHES the Pico's register stream and figures
# out, by itself, WHICH bit is the LED. Same math as the XOR net: inputs x weights,
# sigmoid, gradient descent. Here the inputs are the 32 bits of GPIO_IN (what the
# model "gets to look at"), and the label is whether the LED was commanded on.
import sys, math, os

LOG = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/pico-regs.log")

# --- read the dataset the Pico streamed ---
rows = []
for line in open(LOG, errors="replace"):
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    parts = line.split()
    if len(parts) < 3:
        continue
    try:
        out_reg = int(parts[0], 16)   # commanded pins
        in_reg  = int(parts[1], 16)   # ACTUAL pin levels  <- the model looks at THIS
    except ValueError:
        continue
    led = (out_reg >> 25) & 1         # ground truth: was the LED commanded on?
    bits = [(in_reg >> i) & 1 for i in range(32)]   # 32 inputs
    rows.append((bits, led))

if len(rows) < 4:
    print(f"Only {len(rows)} samples in {LOG}. Run ~/watch-registers.sh a bit longer first.")
    sys.exit(1)

# --- the model: one neuron, 32 weights + bias (like XOR, just wider) ---
w = [0.0] * 32
b = 0.0
lr = 0.5
l2 = 0.01          # weight decay: pushes USELESS bits toward 0 (so noise/constant bits fade)

def sigmoid(z):
    if z < -60: return 0.0
    if z >  60: return 1.0
    return 1.0 / (1.0 + math.exp(-z))

# --- train: nudge weights to predict the LED from the bits ---
for epoch in range(400):
    for bits, y in rows:
        z = sum(w[i] * bits[i] for i in range(32)) + b
        p = sigmoid(z)
        err = p - y                       # how wrong
        for i in range(32):
            w[i] -= lr * (err * bits[i] + l2 * w[i])
        b -= lr * err

# --- check accuracy ---
correct = 0
for bits, y in rows:
    p = sigmoid(sum(w[i] * bits[i] for i in range(32)) + b)
    if (1 if p > 0.5 else 0) == y:
        correct += 1
acc = 100.0 * correct / len(rows)

# --- reveal what it learned: which bit got the big weight? ---
ranked = sorted(range(32), key=lambda i: -abs(w[i]))
print(f"trained on {len(rows)} samples from the Pico   |   accuracy: {acc:.0f}%\n")
print("the model's weight for each input bit (biggest first):")
for i in ranked[:6]:
    mark = "   <-- THE LED (GPIO 25)" if i == 25 else ("   (bit 24: always-on noise)" if i == 24 else "")
    print(f"   bit {i:2d}:  weight = {w[i]:+7.3f}{mark}")
winner = ranked[0]
print(f"\n==> the model decided bit {winner} is the LED"
      f"{'  — CORRECT (that IS GPIO 25).' if winner == 25 else '.'}")
print("   it learned this only by watching the register stream — nobody told it.")
