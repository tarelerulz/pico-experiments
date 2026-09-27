#!/usr/bin/env python3
# Train the smallest neural net that can learn XOR ("exactly one button -> LED on").
# Architecture: 2 inputs -> 2 hidden neurons -> 1 output.  Total weights = 9.
# Pure gradient descent (backprop), from scratch. No ML library beyond numpy for the math.
import os
import numpy as np

# The ENTIRE training set: the 4 possible button combinations and the LED we want.
X = np.array([[0, 0], [0, 1], [1, 0], [1, 1]], dtype=float)   # button A, button B
Y = np.array([[0],    [1],    [1],    [0]],    dtype=float)   # LED (1 = on)

def sigmoid(z):
    return 1.0 / (1.0 + np.exp(-z))

def train(seed, epochs=30000, lr=0.5):
    rng = np.random.default_rng(seed)
    # 9 numbers, started as random noise:
    W1 = rng.uniform(-1, 1, (2, 2))   # input -> hidden   (4 weights)
    b1 = rng.uniform(-1, 1, (1, 2))   # hidden biases     (2)
    W2 = rng.uniform(-1, 1, (2, 1))   # hidden -> output  (2 weights)
    b2 = rng.uniform(-1, 1, (1, 1))   # output bias       (1)
    init = (W1.copy(), b1.copy(), W2.copy(), b2.copy())
    hist = []
    for e in range(epochs + 1):
        # ---- forward pass: exactly the LLM's math, just tiny ----
        h   = sigmoid(X @ W1 + b1)     # hidden activations (4x2)
        out = sigmoid(h @ W2 + b2)     # LED prediction     (4x1)
        loss = np.mean((out - Y) ** 2)
        if e in (0, 200, 1000, 5000, 15000, epochs):
            hist.append((e, loss))
        # ---- backprop: nudge the 9 numbers to reduce the error ----
        d_out = (out - Y) * out * (1 - out)
        dW2 = h.T @ d_out;  db2 = d_out.sum(0, keepdims=True)
        d_h = (d_out @ W2.T) * h * (1 - h)
        dW1 = X.T @ d_h;    db1 = d_h.sum(0, keepdims=True)
        W1 -= lr * dW1;  b1 -= lr * db1
        W2 -= lr * dW2;  b2 -= lr * db2
    return dict(W1=W1, b1=b1, W2=W2, b2=b2, loss=loss, hist=hist, init=init)

# 2-hidden XOR sometimes lands in a bad local minimum, so try seeds until one nails it.
best, best_seed = None, None
for s in range(80):
    r = train(s)
    if best is None or r["loss"] < best["loss"]:
        best, best_seed = r, s
    if r["loss"] < 0.002:
        best, best_seed = r, s
        break
r = best

W1, b1, W2, b2 = r["W1"], r["b1"], r["W2"], r["b2"]
W1i, b1i, W2i, b2i = r["init"]

print(f"=== XOR net: 2 inputs -> 2 hidden -> 1 output  (9 weights) ===")
print(f"chosen seed {best_seed}\n")

print("BEFORE training  (9 random numbers, knows nothing):")
print(f"  input->hidden W1 = {np.round(W1i,2).tolist()}   hidden bias b1 = {np.round(b1i,2).tolist()}")
print(f"  hidden->out  W2 = {np.round(W2i.flatten(),2).tolist()}   out bias b2 = {np.round(b2i.flatten(),2).tolist()}")

print("\ntraining (error shrinking as it learns the rule):")
for e, l in r["hist"]:
    print(f"  epoch {e:>5}   error = {l:.5f}")

print("\nAFTER training  (the 9 numbers that ARE the model):")
print(f"  input->hidden  W1 = [[{W1[0,0]:+.3f}, {W1[0,1]:+.3f}],")
print(f"                       [{W1[1,0]:+.3f}, {W1[1,1]:+.3f}]]")
print(f"  hidden bias    b1 = [{b1[0,0]:+.3f}, {b1[0,1]:+.3f}]")
print(f"  hidden->output W2 = [{W2[0,0]:+.3f}, {W2[1,0]:+.3f}]")
print(f"  output bias    b2 = [{b2[0,0]:+.3f}]")

print("\ndoes it actually compute XOR? (feed all 4 button combos through the 9 numbers)")
print("   A  B  |  hidden1 hidden2 |  raw    -> LED   want")
for i in range(4):
    h = sigmoid(X[i] @ W1 + b1).flatten()
    out = float(sigmoid(h @ W2 + b2).item())
    led = 1 if out > 0.5 else 0
    print(f"   {int(X[i,0])}  {int(X[i,1])}  |  {h[0]:.3f}   {h[1]:.3f}  | {out:.3f} ->  {led}    ({int(Y[i,0])})")
print(f"\nfinal error = {r['loss']:.6f}")

# save the learned weights for a possible Pico flash later
out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xor_weights.npz")
np.savez(out_path, W1=W1, b1=b1, W2=W2, b2=b2)
print(f"\nsaved weights -> {out_path}")
