#!/usr/bin/env python3
# capture_setup.py -- builds a dataset out of the ONE-TIME setup line regdrive.c
# prints at boot (reset_done / gpio_oe / funcsel20-29). That line only ever
# prints once per boot, so each physical unplug+replug of the Pico is one
# training example.
#
# Run this FIRST, then unplug/replug (or power-cycle) the Pico as many times
# as you want examples. The script waits for the USB port to appear, grabs
# the one setup line, waits for the port to vanish again, then goes back to
# waiting -- so you can just keep cycling the board without re-running
# anything. Ctrl+C to stop.
import os, time, select, subprocess

PORT = "/dev/ttyACM0"
OUTFILE = "setup_examples.txt"

def wait_for_port(present):
    while os.path.exists(PORT) != present:
        time.sleep(0.2)

def capture_one():
    subprocess.run(["stty", "-F", PORT, "raw", "-echo", "115200"], check=False)
    fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY)
    buf = b""
    end = time.time() + 5.0
    line_out = None
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            try:
                ch = os.read(fd, 1)
            except OSError:
                break
            if ch == b"\n":
                text = buf.decode(errors="replace").strip()
                buf = b""
                if text.startswith("setup "):
                    line_out = text
                    break
            elif ch != b"\r":
                buf += ch
    os.close(fd)
    return line_out

print(f"Watching for {PORT}. Plug in (or power-cycle) the Pico now...")
print(f"Each example gets appended to {OUTFILE}. Press Ctrl+C to stop.\n")

count = 0
try:
    while True:
        wait_for_port(True)
        time.sleep(0.3)          # let the device node settle before opening it
        line = capture_one()
        if line:
            count += 1
            stamp = time.strftime("%Y-%m-%d %H:%M:%S")
            with open(OUTFILE, "a") as f:
                f.write(f"{stamp}  {line}\n")
            print(f"[{count}] captured: {line}")
        else:
            print("  (port appeared but no setup line seen in time -- skipped)")
        wait_for_port(False)
        print(f"Unplugged. Replug (or power-cycle) for example #{count + 1}...")
except KeyboardInterrupt:
    print(f"\nStopped. {count} examples saved to {OUTFILE}.")
