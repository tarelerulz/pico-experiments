#!/usr/bin/env python3
# train_setup.py -- learns the setup registers from setup_examples.txt (built by
# capture_setup.py, one line per unplug/replug). Same idea as drive_learn.py's
# running-average Q-value: every time a value shows up again, confidence in it
# goes up. Since the real setup writes are identical every boot, this should
# converge to 100% confidence fast -- if it doesn't, that itself is a finding
# (the "setup" isn't as deterministic as assumed).
import re
from collections import Counter

INFILE = "setup_examples.txt"
PATTERN = re.compile(
    r"setup reset_done=([0-9a-f]+) gpio_oe=([0-9a-f]+) funcsel20-29=(\d+)"
)

reset_done = Counter()
gpio_oe    = Counter()
funcsel    = Counter()
total = 0

with open(INFILE) as f:
    for line in f:
        m = PATTERN.search(line)
        if not m:
            continue
        total += 1
        reset_done[m.group(1)] += 1
        gpio_oe[m.group(2)]    += 1
        funcsel[m.group(3)]    += 1

if total == 0:
    print(f"No examples found in {INFILE} yet -- run capture_setup.py first.")
    raise SystemExit

print(f"Learning from {total} examples in {INFILE}\n")
print("=== what the model learned about setup ===")
for name, counts in [("reset_done", reset_done), ("gpio_oe", gpio_oe), ("funcsel20-29", funcsel)]:
    value, n = counts.most_common(1)[0]
    confidence = n / total
    flag = "" if confidence == 1.0 else "  <-- setup wasn't identical every boot!"
    print(f"  {name:14s} = {value}   (seen in {n}/{total} examples, confidence {confidence:.0%}){flag}")

print("\nAs you capture more examples (more unplug/replug cycles), confidence")
print("should climb toward 100% -- the setup writes are the same every boot,")
print("so this is really just confirming that, the same way Q-values in")
print("drive_learn.py converge toward 1.0 when a register write always works.")
