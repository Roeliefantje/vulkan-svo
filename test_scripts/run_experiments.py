import csv
from datetime import datetime
import subprocess
import time
import re
from collections import defaultdict
from statistics import mean
from pathlib import Path
import os
import pandas as pd
import glob
import matplotlib.pyplot as plt
from experiment_one_configurations import CONFIGURATIONS as TEST_ONE_CONFIGURATIONS
from experiment_two_configurations import CONFIGURATIONS as TEST_TWO_CONFIGURATIONS
from experiment_three_configurations import FLYING_CONFIGURATIONS, WALKING_CONFIGURATIONS
import numpy as np

PROGRAM = "./cmake-build-release/clion_vulkan"  # or "./your_script.sh", etc.
WORKING_DIRECTORY = "/home/roeliefantje/Documents/git/uni/vulkan-svo"
CSV_KEYS = ["elapsed_seconds"] + sorted(["fps", "mspf", "steps", "far_values_memory", "octree_memory", "staging_memory", "grid_memory"])

def main():
    # run_experiment_one()
    create_graphs_experiment_one()
    run_experiment_two()
    create_graphs_experiment_two()
    # run_experiment_three()
    # create_graphs_experiment_three()
    # run_comparison_experiments()
    # create_graphs_comparison()

def run_experiment_one():
    for test in [0, 1]:
        for config in TEST_ONE_CONFIGURATIONS:
            grid_size, grid_height, resolution = config[1], config[2], config[0]
            args = ["--test", str(test), "--res", str(resolution),
                    "--grid", str(grid_size), "--gridheight", str(grid_height)]
            print(f"Running program, Grid Size: {grid_size}, Chunk Resolution: {resolution}, test: {test}")
            run_program(PROGRAM, WORKING_DIRECTORY, args, f"./exp1_results/location_{test + 1}_{resolution}_{grid_size}.csv", duration=20)

def run_experiment_two():
    for test in [2, 3]:
        for config in TEST_TWO_CONFIGURATIONS:
            grid_size, grid_height, resolution = config[1], config[2], config[0]
            args = ["--test", str(test), "--res", str(resolution),
                    "--grid", str(grid_size), "--gridheight", str(grid_height)]
            print(f"Running program, Grid Size: {grid_size}, Chunk Resolution: {resolution}, test: {test}")
            run_program(PROGRAM, WORKING_DIRECTORY, args, f"./exp2_results/path_{test - 1}_{resolution}_{grid_size}.csv")

def run_experiment_three():
    for test in [4, 5]:
        configurations = FLYING_CONFIGURATIONS if test == 5 else WALKING_CONFIGURATIONS
        for config in configurations:
            grid_size, resolution = config[1], config[0]
            args = ["--test", str(test), "--res", str(resolution),
                    "--grid", str(grid_size)]
            print(f"Running program, Grid Size: {grid_size}, Chunk Resolution: {resolution}, test: {test}")
            name = "walking" if test == 4 else "flying"
            run_program(PROGRAM, WORKING_DIRECTORY, args, f"./exp3_results/path_{name}_{resolution}_{grid_size}.csv")

def run_comparison_experiments():
    comparison_configs = [
        [512, 1, 5],
        [2048, 1, 5],
        [2048, 3, 5],
    ]
    for config in comparison_configs:
        grid_size, grid_height, resolution = config[1], config[2], config[0]
        args = ["--test", str(0), "--res", str(resolution),
                "--grid", str(grid_size), "--gridheight", str(grid_height)]
        print(f"Running program, Grid Size: {grid_size}, Chunk Resolution: {resolution}, test: {0}")
        run_program(PROGRAM, WORKING_DIRECTORY, args, f"./compare_results/{resolution}_{grid_size}.csv", duration=10)


def create_graphs_experiment_one():
    print("Creating experiment one graphs")
    csv_files = glob.glob(os.path.join("./exp1_results/", "*.csv"))
    averages = {
        "Location 1": {},
        "Location 2": {},
    }

    file_pattern = re.compile(
        r"location_(\d+)_(\d+)_(\d+)\.csv"
    )

    for csv_file in csv_files:
        match = file_pattern.search(csv_file)
        location = f"Location {match.group(1)}"
        chunk_resolution = int(match.group(2))
        total_resolution = chunk_resolution * ((int(match.group(3)) + 1) / 2)
        averages[location][chunk_resolution] = averages[location].get(chunk_resolution, []) + [{
            "x": total_resolution,
            **get_average_from_file(csv_file)
        }]

    for loc, groups in averages.items():
        group_memory = {}
        group_steps = {}
        group_mspf = {}
        for group, values in groups.items():
            for value in values:
                total_memory = value["far_values_memory"] + value["octree_memory"] + value["staging_memory"] + value["grid_memory"]
                group_memory.setdefault(f"Chunk Res: {group}", []).append({
                    "y": total_memory,
                    "x": value["x"]
                })

                group_steps.setdefault(f"Chunk Res: {group}", []).append({
                    "y": value["steps"],
                    "x": value["x"]
                })

                group_mspf.setdefault(f"Chunk Res: {group}", []).append({
                    "y": value["mspf"],
                    "x": value["x"]
                })

        plot_values(group_memory, f"Average GPU Memory Consumption for {loc}", "Total voxel resolution","Average Memory used (MB)", f"./exp1_figures/{loc.replace(' ', '_')}_memory.png", marker=None)
        plot_values(group_steps, f"Average steps per ray for {loc}", "Total voxel resolution","Average steps per ray", f"./exp1_figures/{loc.replace(' ', '_')}_steps.png", marker=None)
        plot_values(group_mspf, f"Average milliseconds per frame for {loc}", "Total voxel resolution","Average milliseconds per frame", f"./exp1_figures/{loc.replace(' ', '_')}_mspf.png", marker=None)

def create_graphs_experiment_two():
    print("Creating experiment two graphs")
    csv_files = glob.glob(os.path.join("./exp2_results/", "*.csv"))
    path_values = {
        "Path 1": {},
        "Path 2": {},
    }

    file_pattern = re.compile(
        r"path_(\d+)_(\d+)_(\d+)\.csv"
    )

    for csv_file in csv_files:
        match = file_pattern.search(csv_file)
        location = f"Path {match.group(1)}"
        chunk_resolution = int(match.group(2))
        total_resolution = chunk_resolution * ((int(match.group(3)) + 1) / 2)
        values = get_csv_file_rows(csv_file)
        group = f"Scene res: {int(total_resolution)}"
        path_values[location][group] = values

    for path, groups in path_values.items():
        memory_over_time = {}
        steps_over_time = {}
        mspf_over_time = {}
        for group, values in groups.items():
            for value in values:
                if value.get("far_values_memory") is not None and value.get("octree_memory") is not None and value.get("staging_memory") is not None and value.get("grid_memory") is not None:
                    memory_over_time.setdefault(group, []).append({
                        "y": float(value["far_values_memory"]) + float(value["octree_memory"]) + float(value["staging_memory"]) + float(value["grid_memory"]),
                        "x": float(value["elapsed_seconds"])
                    })
                if value.get("steps"):
                    steps_over_time.setdefault(group, []).append({
                        "y": float(value["steps"]),
                        "x": float(value["elapsed_seconds"])
                    })
                if value.get("mspf"):
                    mspf_over_time.setdefault(group, []).append({
                        "y": float(value["mspf"]),
                        "x": float(value["elapsed_seconds"])
                    })

        plot_values(memory_over_time, f"Total GPU Memory Consumption for {path}", "Time in seconds","Total GPU Memory used (MB)", f"./exp2_figures/{path.replace(' ', '_')}_memory.png", marker=None)
        plot_values(steps_over_time, f"Average steps per ray for {path}", "Time in seconds","Average steps per ray", f"./exp2_figures/{path.replace(' ', '_')}_steps.png", marker=None)
        plot_values(mspf_over_time, f"Average milliseconds per frame for {path}", "Time in seconds","Average milliseconds per frame", f"./exp2_figures/{path.replace(' ', '_')}_mspf.png", marker=None)


def create_graphs_experiment_three():
    print("Creating experiment three graphs")
    csv_files = glob.glob(os.path.join("./exp3_results/", "*.csv"))
    path_values = {
        "flying configuration": {},
        "walking configuration": {},
    }

    file_pattern = re.compile(
        r"path_(flying|walking)_(\d+)_(\d+)\.csv"
    )

    for csv_file in csv_files:
        match = file_pattern.search(csv_file)
        location = f"{match.group(1)} configuration"
        chunk_resolution = int(match.group(2))
        total_resolution = chunk_resolution * ((int(match.group(3)) + 1) / 2)
        values = get_csv_file_rows(csv_file)
        group = f"Scene res: {total_resolution}"
        path_values[location][group] = values

    for path, groups in path_values.items():
        memory_over_time = {}
        steps_over_time = {}
        mspf_over_time = {}
        for group, values in groups.items():
            for value in values:
                if value.get("far_values_memory") is not None and value.get("octree_memory") is not None and value.get("staging_memory") is not None and value.get("grid_memory") is not None:
                    memory_over_time.setdefault(group, []).append({
                        "y": float(value["far_values_memory"]) + float(value["octree_memory"]) + float(value["staging_memory"]) + float(value["grid_memory"]),
                        "x": float(value["elapsed_seconds"])
                    })
                if value.get("steps"):
                    steps_over_time.setdefault(group, []).append({
                        "y": float(value["steps"]),
                        "x": float(value["elapsed_seconds"])
                    })
                if value.get("mspf"):
                    mspf_over_time.setdefault(group, []).append({
                        "y": float(value["mspf"]),
                        "x": float(value["elapsed_seconds"])
                    })

        plot_values(memory_over_time, f"Total GPU Memory Consumption for the {path}", "Time in seconds","Total GPU Memory used (MB)", f"./exp3_figures/{path.replace(' ', '_')}_memory.png" , marker=None)
        plot_values(steps_over_time, f"Average steps per ray for the {path}", "Time in seconds","Average steps per ray", f"./exp3_figures/{path.replace(' ', '_')}_steps.png", marker=None)
        plot_values(mspf_over_time, f"Average milliseconds per frame for the {path}", "Time in seconds","Average milliseconds per frame", f"./exp3_figures/{path.replace(' ', '_')}_mspf.png", marker=None)

def create_graphs_comparison():
    print("Creating comparison graphs")
    csv_files_ours = glob.glob(os.path.join("./compare_results/", "*.csv"))
    files_theres = glob.glob(os.path.join("./results_illinois-voxel-sandbox/", "*.txt"))
    averages = {
        "ours": {},
        "Arbore et al.": {}
    }

    file_pattern_ours = re.compile(
        r"(\d+)_(\d+)\.csv"
    )
    file_pattern_arbore = re.compile(
        r"(\d+)\.txt"
    )

    for csv_file in csv_files_ours:
        match = file_pattern_ours.search(csv_file)
        chunk_resolution = int(match.group(1))
        total_resolution = chunk_resolution * ((int(match.group(2)) + 1) / 2)
        averages["ours"][total_resolution] = get_average_from_file(csv_file)

    for txt_file in files_theres:
        match = file_pattern_arbore.search(txt_file)
        total_resolution = int(match.group(1))
        averages["Arbore et al."][total_resolution] = get_average_from_arbore(txt_file)

    avg_memory = {}
    avg_mspf = {}
    for group, values in averages.items():
        for resolution, value in values.items():
            if value.get("far_values_memory") is not None and value.get("octree_memory") is not None and value.get("staging_memory") is not None and value.get("grid_memory") is not None:
                avg_memory.setdefault(group, []).append({
                    "y": float(value["far_values_memory"]) + float(value["octree_memory"]) + float(value["staging_memory"]) + float(value["grid_memory"]),
                    "x": int(resolution)
                })
            if value.get("total_memory"):
                avg_memory.setdefault(group, []).append({
                    "y": float(value["total_memory"]),
                    "x": int(resolution)
                })
            if value.get("mspf"):
                avg_mspf.setdefault(group, []).append({
                    "y": float(value["mspf"]),
                    "x": int(resolution)
                })

    plot_values(avg_memory, f"Average GPU Memory Consumption comparison", "Total voxel resolution","Average Memory used (MB)", f"./compare_figures/memory.png")
    plot_values(avg_mspf, f"Average GPU Memory Consumption comparison", "Total voxel resolution","Average milliseconds per frame", f"./compare_figures/mspf.png")

def plot_values(groups: dict, title, xlabel, ylabel, output_file, marker: str|None="o"):
    plt.figure()
    for group_name, values in groups.items():
        values = sorted(values, key=lambda v: v["x"])
        x = [v["x"] for v in values]
        y = [v["y"] for v in values]
        if marker:
            plt.plot(x, y, marker=marker, label=group_name)
        else:
            plt.plot(x, y, label=group_name)
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.grid(True)
    if len(groups.items()) > 1:
        plt.legend()
    plt.tight_layout()
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    plt.savefig(output_file)
    plt.close()

def get_csv_file_rows(file_path):
    rows = []
    with open(file_path, newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            non_empty_row = {}
            for key, value in row.items():
                if value:  # non-empty
                    non_empty_row[key] = value
            rows.append(non_empty_row)
    return rows

def get_average_from_file(file_path):
    sums = defaultdict(float)
    counts = defaultdict(int)

    with open(file_path, newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            try:
                elapsed = float(row["elapsed_seconds"])
            except (ValueError, KeyError):
                continue  # skip invalid rows

            if elapsed < 2:  # skip first 5 seconds
                continue

            for key, value in row.items():
                if key == "elapsed_seconds":
                    continue  # skip timestamp
                if value:  # non-empty
                    try:
                        num = float(value)
                        sums[key] += num
                        counts[key] += 1
                    except ValueError:
                        pass  # ignore non-numeric values

    # Compute averages
    averages = {key: (sums[key] / counts[key] if counts[key] > 0 else 0)
                for key in sums.keys()}
    return averages

def get_average_from_arbore(file_path):
    fps_values = []
    mem_values = []

    with open(file_path, "r") as f:
        for line in f:
            m = re.search(r"INFO:\s*([0-9]+)\s*FPS", line)
            if m:
                fps_values.append(float(m.group(1)))

            m2 = re.search(r"\s([0-9]+)\s*BYTES", line)
            if m2:
                mem_values.append(int(m2.group(1)) / (1024 * 1024))  # bytes → MB

    # Convert FPS to ms per frame
    ms_values = [1000.0 / fps for fps in fps_values]

    return {
        "mspf": np.mean(ms_values),
        "total_memory": np.mean(mem_values)
    }

def run_program(program_path, working_dir, args, output_file, duration=65):
    working_dir = Path(working_dir)
    if not working_dir.exists():
        raise FileNotFoundError(f"Working directory not found: {working_dir}")

    chunkgen_process = subprocess.run(
        [program_path, "--chunkgen", *args],
        cwd=working_dir
    )
    if chunkgen_process.returncode != 0:
        print(f"Error while generating the chunks!:\n{chunkgen_process.stderr}")
        return

    grid_height_pattern = re.compile(
        r"Grid height:\s*(\d+)",
        re.IGNORECASE
    )
    grid_size_pattern = re.compile(
        r"Grid size:\s*(\d+)",
        re.IGNORECASE
    )


    fps_pattern = re.compile(r"\[(\d{2}:\d{2}:\d{2}\.\d+)].*?FPS:\s*([\d.]+),\s*msPf:\s*([\d.]+)")
    steps_pattern = re.compile(r"\[(\d{2}:\d{2}:\d{2}\.\d+)].*?Average Steps per ray:\s*([\d.]+)")
    far_values_memory_pattern = re.compile(r"\[(\d{2}:\d{2}:\d{2}\.\d+)].*?Far Values Buffer Memory used:\s*([\d.]+)\s*MB", re.IGNORECASE)
    octree_memory_pattern = re.compile(r"\[(\d{2}:\d{2}:\d{2}\.\d+)].*?Octree Nodes Buffer Memory used:\s*([\d.]+)\s*MB", re.IGNORECASE)
    staging_buffer_memory_pattern = re.compile(r"\[(\d{2}:\d{2}:\d{2}\.\d+)].*?Staging Buffer Memory used:\s*([\d.]+)\s*MB", re.IGNORECASE)

    fps_values, mspf_values, steps_values, memory_usages = [], [], [], []

    process = subprocess.Popen(
        [program_path, *args],
        cwd=working_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        bufsize=1,
    )

    print(f"Running {program_path} in {working_dir} for 60 seconds...\n")
    start_time = time.time()

    output_lines = defaultdict(dict)
    start_ts = None
    grid_height = 0
    grid_size = 0

    try:
        for line in process.stdout:
            line = line.strip()

            if match := grid_height_pattern.search(line):
                grid_height = int(match.group(1))

            elif match := grid_size_pattern.search(line):
                grid_size = int(match.group(1))

            elif match := fps_pattern.search(line):
                ts = datetime.strptime(match.group(1), "%H:%M:%S.%f")
                start_ts = ts if not start_ts else start_ts
                delta_seconds = round((ts - start_ts).total_seconds(), 1)
                output_lines[delta_seconds]["fps"] = float(match.group(2))
                output_lines[delta_seconds]["mspf"] = float(match.group(3))

            elif match := steps_pattern.search(line):
                ts = datetime.strptime(match.group(1), "%H:%M:%S.%f")
                start_ts = ts if not start_ts else start_ts
                delta_seconds = round((ts - start_ts).total_seconds(), 1)
                output_lines[delta_seconds]["steps"] = float(match.group(2))

            elif match := far_values_memory_pattern.search(line):
                ts = datetime.strptime(match.group(1), "%H:%M:%S.%f")
                start_ts = ts if not start_ts else start_ts
                delta_seconds = round((ts - start_ts).total_seconds(), 1)
                output_lines[delta_seconds]["far_values_memory"] = float(match.group(2))

            elif match := octree_memory_pattern.search(line):
                ts = datetime.strptime(match.group(1), "%H:%M:%S.%f")
                start_ts = ts if not start_ts else start_ts
                delta_seconds = round((ts - start_ts).total_seconds(), 1)
                output_lines[delta_seconds]["octree_memory"] = float(match.group(2))

            elif match := staging_buffer_memory_pattern.search(line):
                ts = datetime.strptime(match.group(1), "%H:%M:%S.%f")
                start_ts = ts if not start_ts else start_ts
                delta_seconds = round((ts - start_ts).total_seconds(), 1)
                output_lines[delta_seconds]["staging_memory"] = float(match.group(2))

            if time.time() - start_time > duration:
                process.terminate()
                break

        process.wait(timeout=5)

    except KeyboardInterrupt:
        process.terminate()
    except subprocess.TimeoutExpired:
        process.kill()

    for inner in output_lines.values():
        #8 bytes on the GPU for each grid position,
        inner["grid_memory"] = (grid_height * grid_size * grid_size * 8) / (1024.0 * 1024.0)

    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    # Step 2: Write to CSV
    with open(output_file, "w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_KEYS)
        writer.writeheader()

        for elapsed_seconds, inner in sorted(output_lines.items()):
            # Make a row dict
            row = {"elapsed_seconds": elapsed_seconds}
            row.update(inner)  # add inner dict values
            writer.writerow(row)


if __name__ == "__main__":
    main()
