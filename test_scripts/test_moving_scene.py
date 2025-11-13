import csv
import subprocess
import time
import re
from statistics import mean
from pathlib import Path
import os
import pandas as pd
import matplotlib.pyplot as plt

CONFIGURATIONS = [
    # [512, 1, 5],
    # [512, 3, 5],
    # [512, 5, 5],
    # [1024, 1, 5],
    # [1024, 3, 5],
    [1024, 5, 5],
]


def run_and_record(program_path, working_dir, args, duration=60, out_csv="results/time_series.csv"):
    """Run the program and record per-second averages for FPS, msPf, steps, and memory."""
    working_dir = Path(working_dir)
    os.makedirs("results", exist_ok=True)

    # Regex patterns
    fps_pattern = re.compile(r"FPS:\s*([\d.]+),\s*msPf:\s*([\d.]+)")
    steps_pattern = re.compile(r"Average Steps per ray:\s*([\d.]+)")
    memory_pattern = re.compile(r"Memory used:\s*([\d.]+)\s*MB", re.IGNORECASE)

    # Start process
    process = subprocess.Popen(
        [program_path, *args],
        cwd=working_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        bufsize=1,
    )

    print(f"Running {program_path} for {duration} seconds...\n")

    # Storage
    results = []
    start_time = time.time()
    next_record_time = start_time + 1

    # Buffers for per-second averages
    fps_buf, mspf_buf, steps_buf, mem_buf = [], [], [], []

    try:
        for line in process.stdout:
            line = line.strip()

            if match := fps_pattern.search(line):
                fps_buf.append(float(match.group(1)))
                mspf_buf.append(float(match.group(2)))
            elif match := steps_pattern.search(line):
                steps_buf.append(float(match.group(1)))
            elif "Memory used" in line:
                matches = memory_pattern.findall(line)
                if matches:
                    total_mem = sum(map(float, matches))
                    mem_buf.append(total_mem)

            # Time check — record each second
            now = time.time()
            if now >= next_record_time:
                elapsed = int(now - start_time)
                results.append({
                    "second": elapsed,
                    "fps": mean(fps_buf) if fps_buf else 0,
                    "mspf": mean(mspf_buf) if mspf_buf else 0,
                    "steps": mean(steps_buf) if steps_buf else 0,
                    "memory_mb": mean(mem_buf) if mem_buf else 0,
                })
                # Clear buffers for next second
                fps_buf.clear()
                mspf_buf.clear()
                steps_buf.clear()
                mem_buf.clear()
                next_record_time += 1

            # Stop after duration
            if now - start_time > duration:
                process.terminate()
                break

        process.wait(timeout=5)
    except KeyboardInterrupt:
        process.terminate()
    except subprocess.TimeoutExpired:
        process.kill()

    # Write to CSV
    with open(out_csv, mode='a', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=["gridsize", "gridheight", "maxchunkresolution", "total_resolution",
                                               "second", "fps", "mspf", "steps", "memory_mb"])
        if f.tell() == 0:
            writer.writeheader()
        total_res = args[args.index("--res") + 1]
        grid_size = args[args.index("--grid") + 1]
        grid_height = args[args.index("--gridheight") + 1]
        for r in results:
            writer.writerow({
                "gridsize": grid_size,
                "gridheight": grid_height,
                "maxchunkresolution": total_res,
                "total_resolution": float(total_res) * float(grid_size) * 0.5,
                **r
            })
    print(f"Recorded {len(results)} seconds of data to {out_csv}\n")


def plot_time_series(csv_path="results/time_series.csv", output_dir="results/time_series_plots"):
    """Plot memory, msPf, and steps over time — one line per total resolution."""
    os.makedirs(output_dir, exist_ok=True)
    df = pd.read_csv(csv_path)

    grouped = df.groupby("total_resolution")

    # MSPF over time
    plt.figure()
    for total_res, data in grouped:
        plt.plot(data["second"], data["mspf"], label=f"Res={total_res}")
    plt.xlabel("Time (s)")
    plt.ylabel("ms per frame")
    plt.title("MSPF over Time")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "mspf_over_time.png"))
    plt.close()

    # Steps over time
    plt.figure()
    for total_res, data in grouped:
        plt.plot(data["second"], data["steps"], label=f"Res={total_res}")
    plt.xlabel("Time (s)")
    plt.ylabel("Average Steps per Ray")
    plt.title("Steps per Ray over Time")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "steps_over_time.png"))
    plt.close()

    # Memory over time
    plt.figure()
    for total_res, data in grouped:
        plt.plot(data["second"], data["memory_mb"], label=f"Res={total_res}")
    plt.xlabel("Time (s)")
    plt.ylabel("Memory Used (MB)")
    plt.title("Memory Usage over Time")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "memory_over_time.png"))
    plt.close()

    print(f"Time series graphs saved to {output_dir}")


if __name__ == "__main__":
    program = "../cmake-build-release-mingw/clion_vulkan.exe"
    working_directory = "C:\\Users\\roeld\\Documents\\git\\Uni\\UU\\_Thesis\\clion-vulkan"

    csv_file = "results/time_series.csv"
    # Clear previous results if any
    if os.path.exists(csv_file):
        os.remove(csv_file)

    for config in CONFIGURATIONS:
        resolution, grid_size, grid_height = config
        args = ["--test", "2", "--res", str(resolution),
                "--grid", str(grid_size), "--gridheight", str(grid_height)]
        run_and_record(program, working_directory, args, duration=60, out_csv=csv_file)

    plot_time_series(csv_file)
