#!/usr/bin/env python3
# drive_learn.py — the LEARNER. Given a goal (LED on/off), it must discover WHICH
# register write on the Pico controls the light, using only the LED feedback as reward.
# No knowledge that bit 25 is the LED — it learns that from trial and error.
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
        except: return None
    return None

time.sleep(0.5); os.write(fd,b"\n"); readline(0.5)   # drain startup

CAND_BITS = list(range(20,30))            # the 10 candidate "levers" (25 is secretly the LED)
ACTIONS   = [(b,v) for b in CAND_BITS for v in (0,1)]   # 20 possible register writes
GOALS     = ["on","off"]
goal_led  = lambda g: 1 if g=="on" else 0

Q = {g:{a:0.0 for a in ACTIONS} for g in GOALS}   # learned value of each action per goal
N = {g:{a:0   for a in ACTIONS} for g in GOALS}

print("Learning to drive the LED (no idea which bit it is)...")
eps, TRIALS = 0.3, 600
for t in range(TRIALS):
    goal = random.choice(GOALS)
    cmd("R")                                        # randomize the LED to start the trial
    if random.random() < eps:
        a = random.choice(ACTIONS)                  # explore
    else:
        a = max(ACTIONS, key=lambda x: Q[goal][x])  # exploit best known
    led = led_of(cmd(f"{a[0]} {a[1]}"))             # execute the write, read result
    if led is None: continue
    reward = 1.0 if led == goal_led(goal) else 0.0  # did the LED match the goal?
    N[goal][a]+=1
    Q[goal][a]+= (reward - Q[goal][a]) / N[goal][a] # running average
    if t % 150 == 0: print(f"  trial {t}...")

print("\n=== what the model learned (best register write per goal) ===")
for g in GOALS:
    best = max(ACTIONS, key=lambda x: Q[g][x])
    ok = "  <-- CORRECT (bit 25 is the LED)" if best[0]==25 else ""
    print(f"  to make LED {g:3s}:  write bit {best[0]} = {best[1]}   (confidence {Q[g][best]:.2f}){ok}")

print("\n=== demo: the model now drives the LED purely from goals ===")
for g in ["on","off","on","off"]:
    a = max(ACTIONS, key=lambda x: Q[g][x])
    reply = cmd(f"{a[0]} {a[1]}")
    print(f"  goal {g:3s} -> model writes bit {a[0]}={a[1]} -> {reply}")
    time.sleep(0.5)
os.close(fd)
print("\nIt discovered how to control the light from feedback alone.")
