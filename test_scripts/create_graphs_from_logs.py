import re
import matplotlib.pyplot as plt

log_file = "big_scene_output.txt"

# Data storage
times = []
mspf = []
memory = []
steps = []

start_time = None

# Regex patterns
time_re = re.compile(r"\[([0-9:]+)\.([0-9]+)\]")
mspf_re = re.compile(r"msPf:\s*([0-9.]+)")
memory_re = re.compile(r"Octree Nodes Buffer Memory used:\s*([0-9.]+)\s*MB")
steps_re = re.compile(r"Average Steps per ray:\s*([0-9.]+)")

def time_to_seconds(t, ms):
    """Convert HH:MM:SS and millisecond string to total seconds."""
    h, m, s = map(int, t.split(":"))
    return h*3600 + m*60 + s + int(ms)/1000.0

with open(log_file, "r") as f:
    for line in f:
        # Match timestamp
        tmatch = time_re.search(line)
        if not tmatch:
            continue

        timestamp = time_to_seconds(tmatch.group(1), tmatch.group(2))

        # Initialize start time
        if start_time is None:
            start_time = timestamp

        elapsed = timestamp - start_time

        # Stop after 60 seconds
        if elapsed > 60:
            break

        # Read msPf
        m = mspf_re.search(line)
        if m:
            mspf.append(float(m.group(1)))
            times.append(elapsed)

        # Read memory usage
        m2 = memory_re.search(line)
        if m2:
            memory.append(float(m2.group(1)))

        # Read average steps per ray
        m3 = steps_re.search(line)
        if m3:
            steps.append(float(m3.group(1)))

# -------------------------------------------------------
# Plot 1: ms per frame over time
# -------------------------------------------------------
plt.figure(figsize=(10, 4))
plt.plot(times, mspf, marker='')
plt.xlabel("Time (s)")
plt.ylabel("ms per frame (msPf) ")
plt.title("ms per frame over time (Flying configuration)")
plt.grid(True)
plt.xlim(0, 60)
plt.tight_layout()
plt.savefig("mspf_over_time_flying.png", dpi=150)
plt.close()

# -------------------------------------------------------
# Plot 2: Memory over time
# -------------------------------------------------------
plt.figure(figsize=(10, 4))
plt.plot(times[:len(memory)], memory, marker='')
plt.xlabel("Time (s)")
plt.ylabel("Memory (MB)")
plt.title("Octree Memory Usage over Time (Flying configuration)")
plt.grid(True)
plt.xlim(0, 60)
plt.tight_layout()
plt.savefig("memory_over_time_flying.png", dpi=150)
plt.close()

# -------------------------------------------------------
# Plot 3: Steps per ray over time
# -------------------------------------------------------
plt.figure(figsize=(10, 4))
plt.plot(times[:len(steps)], steps, marker='')
plt.xlabel("Time (s)")
plt.ylabel("Average Steps per Ray")
plt.title("Average Steps per Ray over Time (Flying configuration)")
plt.grid(True)
plt.xlim(0, 60)
plt.tight_layout()
plt.savefig("steps_over_time_flying.png", dpi=150)
plt.close()

print("Saved: mspf_over_time.png, memory_over_time.png, steps_over_time.png")
