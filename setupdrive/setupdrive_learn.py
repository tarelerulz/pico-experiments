#!/usr/bin/env python3
# setupdrive_learn.py -- like drive_learn.py, but learns the WHOLE thing: not
# just which bit/value turns the LED on or off, but which pin, which mux
# (function-select) value, and whether output-enable is on. Nothing about
# pin configuration is hardcoded here -- setupdrive.c leaves every candidate
# pin unconfigured until this script's actions configure it.
import os, time, select, random, subprocess

PORT = "/dev/ttyACM0"
subprocess.run(["stty","-F",PORT,"raw","-echo","115200"], check=False)
fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY)

def readline(timeout=2.0):
    buf=b""; end=time.time()+timeout
    while time.time()<end:
        r,_,_=select.select([fd],[],[],0.1)
        if r:
            ch=os.read(fd,1)
            if ch==b"\n": return buf.decode(errors="replace").strip()
            if ch!=b"\r": buf+=ch
    return buf.decode(errors="replace").strip()

def cmd(s):
    os.write(fd,(s+"\n").encode()); return readline()

def led_of(reply):
    if "led=" in reply:
        try: return int(reply.split("led=")[1][0])
        except Exception: return None
    return None

time.sleep(0.5); os.write(fd,b"\n"); readline(0.5)   # drain startup

PINS    = list(range(20,30))
FUNCS   = list(range(0,9))     # real RP2040 function numbers are 0..8
OES     = [0,1]
VALS    = [0,1]
ACTIONS = [(p,f,oe,v) for p in PINS for f in FUNCS for oe in OES for v in VALS]
GOALS   = ["on","off"]; goal_led = lambda g: 1 if g=="on" else 0

Q = {g:{a:0.0 for a in ACTIONS} for g in GOALS}
N = {g:{a:0   for a in ACTIONS} for g in GOALS}

print(f"Learning the WHOLE setup (pin + mux + output-enable) plus on/off,")
print(f"from {len(ACTIONS)} possible actions -- no pin config hardcoded at all.")
eps, TRIALS = 0.3, 3000
for t in range(TRIALS):
    goal = random.choice(GOALS)
    cmd("R")
    if random.random() < eps:
        a = random.choice(ACTIONS)
    else:
        a = max(ACTIONS, key=lambda x: Q[goal][x])
    p,f,oe,v = a
    led = led_of(cmd(f"A {p} {f} {oe} {v}"))
    if led is None: continue
    reward = 1.0 if led == goal_led(goal) else 0.0
    N[goal][a]+=1
    Q[goal][a]+= (reward - Q[goal][a]) / N[goal][a]
    if t % 500 == 0: print(f"  trial {t}...")

print("\n=== what the model learned (pin, mux, output-enable, value) ===")
for g in GOALS:
    best = max(ACTIONS, key=lambda x: Q[g][x])
    p,f,oe,v = best
    ok = "  <-- pin 25, mux=SIO(5), OE on: fully correct" if (p==25 and f==5 and oe==1) else ""
    print(f"  to make LED {g:3s}: pin={p} funcsel={f} oe={oe} val={v}   (confidence {Q[g][best]:.2f}){ok}")

print("\n=== proof: blinking using ONLY what was just learned (setup included) ===")
on_a  = max(ACTIONS, key=lambda x: Q["on"][x])
off_a = max(ACTIONS, key=lambda x: Q["off"][x])
for i in range(6):
    cmd("R")
    r1 = cmd(f"A {on_a[0]} {on_a[1]} {on_a[2]} {on_a[3]}");  print(f"  ON  -> {r1}")
    time.sleep(0.4)
    r2 = cmd(f"A {off_a[0]} {off_a[1]} {off_a[2]} {off_a[3]}"); print(f"  OFF -> {r2}")
    time.sleep(0.4)
os.close(fd)
print("\nThat blink used a pin, mux value, and output-enable the model")
print("discovered itself -- not just the on/off value.")
