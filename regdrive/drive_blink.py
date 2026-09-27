#!/usr/bin/env python3
# drive_blink.py — the model LEARNS which bit is the LED, then BLINKS it visibly.
# IMPORTANT: this script never mentions bit 25. It blinks ONLY the bit the model
# DISCOVERED controls the light. If the learning failed, there would be no blink.
# So a visible blink = proof the model learned it, not the app.
import os, time, select, random, subprocess

PORT = "/dev/ttyACM0"
subprocess.run(["stty","-F",PORT,"raw","-echo","115200"], check=False)
fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY)

def readline(t=2.0):
    buf=b""; end=time.time()+t
    while time.time()<end:
        r,_,_=select.select([fd],[],[],0.1)
        if r:
            ch=os.read(fd,1)
            if ch==b"\n": return buf.decode(errors="replace").strip()
            if ch!=b"\r": buf+=ch
    return buf.decode(errors="replace").strip()
def cmd(s): os.write(fd,(s+"\n").encode()); return readline()
def led_of(r):
    try: return int(r.split("led=")[1][0]) if "led=" in r else None
    except: return None

time.sleep(0.5); os.write(fd,b"\n"); readline(0.5)

CAND = list(range(20,30))                      # 10 candidate bits — model doesn't know which is the LED
ACTIONS = [(b,v) for b in CAND for v in (0,1)]
GOALS = ["on","off"]; goal_led = lambda g: 1 if g=="on" else 0
Q = {g:{a:0.0 for a in ACTIONS} for g in GOALS}
N = {g:{a:0   for a in ACTIONS} for g in GOALS}

print("Phase 1: model learns which bit controls the light (from feedback only)...")
for t in range(400):
    g=random.choice(GOALS); cmd("R")
    a=random.choice(ACTIONS) if random.random()<0.3 else max(ACTIONS,key=lambda x:Q[g][x])
    led=led_of(cmd(f"{a[0]} {a[1]}"))
    if led is None: continue
    rw=1.0 if led==goal_led(g) else 0.0
    N[g][a]+=1; Q[g][a]+=(rw-Q[g][a])/N[g][a]

on_act  = max(ACTIONS, key=lambda x: Q["on"][x])   # what the MODEL learned = "on"
off_act = max(ACTIONS, key=lambda x: Q["off"][x])  # what the MODEL learned = "off"
print(f"model discovered:  ON = write bit {on_act[0]}={on_act[1]}   OFF = write bit {off_act[0]}={off_act[1]}")
print("(this script has NO hardcoded pin — it will blink ONLY the bit it just learned)\n")

print("Phase 2: the model blinks the LED using ONLY what it learned — watch the light:")
for i in range(10):
    r1 = cmd(f"{on_act[0]} {on_act[1]}");  print(f"  blink {i+1}: ON  -> {r1}");  time.sleep(0.5)
    r2 = cmd(f"{off_act[0]} {off_act[1]}"); print(f"           OFF -> {r2}");      time.sleep(0.5)
os.close(fd)
print("\nThat blink was driven by the model's learned knowledge, not a hardcoded blink.")
