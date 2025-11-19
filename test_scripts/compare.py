import re
import numpy as np
import matplotlib.pyplot as plt

# ------------------------------------------------------
# Helper functions to parse the logs
# ------------------------------------------------------

def parse_ours_file(path):
    """Extract msPf and memory usage (MB) from /results_ours/ logs."""
    ms_values = []
    mem_values = []

    with open(path, "r") as f:
        for line in f:
            # msPf
            m = re.search(r"msPf:\s*([0-9.]+)", line)
            if m:
                ms_values.append(float(m.group(1)))

            # Memory (take e.g. Octree Nodes Buffer Memory used: 58.63 MB)
            m2 = re.search(r"Memory used:\s*([0-9.]+)\s*MB", line)
            if m2:
                mem_values.append(float(m2.group(1)))

    return np.mean(ms_values), np.mean(mem_values)


def parse_voxel_file(path):
    """Extract FPS and BYTES → MB from /results_voxel_illinous/ logs."""
    fps_values = []
    mem_values = []

    with open(path, "r") as f:
        for line in f:
            # FPS
            m = re.search(r"INFO:\s*([0-9]+)\s*FPS", line)
            if m:
                fps_values.append(float(m.group(1)))

            # Bytes memory usage
            m2 = re.search(r"\s([0-9]+)\s*BYTES", line)
            if m2:
                mem_values.append(int(m2.group(1)) / (1024 * 1024))  # bytes → MB

    # Convert FPS to ms per frame: ms = 1000 / FPS
    ms_values = [1000.0 / fps for fps in fps_values]

    return np.mean(ms_values), np.mean(mem_values)


# ------------------------------------------------------
# Filenames & resolutions
# ------------------------------------------------------

resolutions = [512, 2048, 4096]

ours_base = "results_ours/"
voxel_base = "results_illinois-voxel-sandbox/"

ours_ms = []
ours_mem = []

voxel_ms = []
voxel_mem = []

for r in resolutions:
    # Parse ours
    ms_o, mem_o = parse_ours_file(f"{ours_base}{r}.txt")
    ours_ms.append(ms_o)
    ours_mem.append(mem_o)

    # Parse voxel
    ms_v, mem_v = parse_voxel_file(f"{voxel_base}{r}.txt")
    voxel_ms.append(ms_v)
    voxel_mem.append(mem_v)

# ------------------------------------------------------
# Plot 1 — ms per frame
# ------------------------------------------------------

plt.figure(figsize=(8, 5))
plt.plot(resolutions, ours_ms, marker='o', label="ours")
plt.plot(resolutions, voxel_ms, marker='o', label="hybrid voxel")

plt.xlabel("Resolution")
plt.ylabel("Milliseconds per Frame (ms)")
plt.title("Average Frame Time (ms) vs Resolution")
plt.grid(True)
plt.legend()
plt.xticks([512, 2048, 4096])
plt.tight_layout()
plt.savefig("frametimes.png", dpi=150)


plt.show()

# ------------------------------------------------------
# Plot 2 — Memory usage
# ------------------------------------------------------

plt.figure(figsize=(8, 5))
plt.plot(resolutions, ours_mem, marker='o', label="ours")
plt.plot(resolutions, voxel_mem, marker='o', label="hybrid voxel")

plt.xlabel("Resolution")
plt.ylabel("Memory Usage (MB)")
plt.title("Average Memory Usage vs Resolution")
plt.grid(True)
plt.legend()

plt.xticks([512, 2048, 4096])
plt.tight_layout()
plt.savefig("memory.png", dpi=150)
plt.show()
