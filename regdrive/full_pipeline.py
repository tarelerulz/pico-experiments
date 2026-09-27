#!/usr/bin/env python3
# full_pipeline.py -- combines the two things you've learned separately:
#   1. setup_examples.txt (from capture_setup.py): is the boot-time setup
#      the same every time? (reset/mux/OE)
#   2. live trial-and-reward against regdrive (same idea as drive_learn.py):
#      which register write turns the LED on, which turns it off?
# Prints ONE combined confidence report for the whole pipeline (setup +
# on/off), then blinks the LED for real using ONLY what was confirmed/
# learned -- if this blinks correctly, both halves are proven together.
#
# Needs: setup_examples.txt already has entries (run capture_setup.py first,
# unplug/replug a few times), AND the Pico currently plugged in and running
# regdrive firmware (so this script can do the live on/off trials).
import os, re, time, select, random, subprocess
from collections import Counter

SETUP_FILE = "setup_examples.txt"
PORT = "/dev/ttyACM0"
SETUP_PATTERN = re.compile(
    r"setup reset_done=([0-9a-f]+) gpio_oe=([0-9a-f]+) funcsel20-29=(\d+)"
)

# ---------- Part 1: setup confidence (reads the file, no hardware needed) ----------
def setup_confidence():
    counts = {"reset_done": Counter(), "gpio_oe": Counter(), "funcsel20-29": Counter()}
    total = 0
    if os.path.exists(SETUP_FILE):
        with open(SETUP_FILE) as f:
            for line in f:
                m = SETUP_PATTERN.search(line)
                if not m:
                    continue
                total += 1
                counts["reset_done"][m.group(1)] += 1
                counts["gpio_oe"][m.group(2)] += 1
                counts["funcsel20-29"][m.group(3)] += 1
    if total == 0:
        return 0, 0.0
    agree = min(c.most_common(1)[0][1] for c in counts.values())
    return total, agree / total

# ---------- Part 2: live on/off discovery against the plugged-in Pico ----------
def readline(fd, timeout=2.0):
    buf = b""; end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            ch = os.read(fd, 1)
            if ch == b"\n": return buf.decode(errors="replace").strip()
            if ch != b"\r": buf += ch
    return buf.decode(errors="replace").strip()

def led_of(reply):
    if "led=" in reply:
        try: return int(reply.split("led=")[1][0])
        except Exception: return None
    return None

def learn_on_off(fd, trials=400):
    CAND = list(range(20, 30))
    ACTIONS = [(b, v) for b in CAND for v in (0, 1)]
    GOALS = ["on", "off"]; goal_led = lambda g: 1 if g == "on" else 0
    Q = {g: {a: 0.0 for a in ACTIONS} for g in GOALS}
    N = {g: {a: 0 for a in ACTIONS} for g in GOALS}

    def cmd(s):
        os.write(fd, (s + "\n").encode())
        return readline(fd)

    for t in range(trials):
        g = random.choice(GOALS)
        cmd("R")
        a = random.choice(ACTIONS) if random.random() < 0.3 else max(ACTIONS, key=lambda x: Q[g][x])
        led = led_of(cmd(f"{a[0]} {a[1]}"))
        if led is None:
            continue
        reward = 1.0 if led == goal_led(g) else 0.0
        N[g][a] += 1
        Q[g][a] += (reward - Q[g][a]) / N[g][a]

    on_act = max(ACTIONS, key=lambda x: Q["on"][x])
    off_act = max(ACTIONS, key=lambda x: Q["off"][x])
    return on_act, Q["on"][on_act], off_act, Q["off"][off_act]

# ---------- run both, then demo the combined result ----------
print("=== Part 1: setup confidence (from past unplug/replug cycles) ===")
n, conf = setup_confidence()
if n == 0:
    print(f"  No data in {SETUP_FILE} yet -- run capture_setup.py first. Stopping.")
    raise SystemExit
print(f"  {n} examples, setup agreed {conf:.0%} of the time")

print("\n=== Part 2: live on/off discovery (talking to the Pico right now) ===")
subprocess.run(["stty", "-F", PORT, "raw", "-echo", "115200"], check=False)
fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY)
time.sleep(0.5); os.write(fd, b"\n"); readline(fd, 0.5)   # drain startup

on_act, on_conf, off_act, off_conf = learn_on_off(fd)
print(f"  ON  = write bit {on_act[0]}={on_act[1]}   (confidence {on_conf:.0%})")
print(f"  OFF = write bit {off_act[0]}={off_act[1]}   (confidence {off_conf:.0%})")

print("\n=== combined pipeline confidence ===")
overall = min(conf, on_conf, off_conf)
print(f"  setup: {conf:.0%}   on/off: {min(on_conf, off_conf):.0%}   -> overall: {overall:.0%}")

print("\n=== proof: blinking using ONLY the confirmed setup + learned actions ===")
for i in range(6):
    r1 = os.write(fd, f"{on_act[0]} {on_act[1]}\n".encode()); print(f"  {readline(fd)}"); time.sleep(0.4)
    r2 = os.write(fd, f"{off_act[0]} {off_act[1]}\n".encode()); print(f"  {readline(fd)}"); time.sleep(0.4)
os.close(fd)
print("\nIf that blinked the real LED, the whole pipeline -- setup AND on/off -- is confirmed together.")
