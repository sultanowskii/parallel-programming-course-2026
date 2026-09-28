import csv
from statistics import median
import subprocess


def run_stress(impl, threads, snapshots):
    command = [
        "./bench", "stress", "--impl", impl,
        "--threads", str(threads), "--snapshots", str(snapshots),
    ]
    print("\n" + " ".join(command), flush=True)

    process = subprocess.Popen(command, stdout=subprocess.PIPE, text=True)
    result = {}

    for line in process.stdout:
        print(line, end="", flush=True)

        if ": " in line:
            name, value = line.strip().split(": ", 1)
            result[name] = value

    return_code = process.wait()
    if return_code != 0 and not (impl == "double-buffer-unsafe" and return_code == 1):
        raise RuntimeError(f"stress test failed: {impl}, threads={threads}")

    less = int(result["Bucket sum < count"])
    greater = int(result["Bucket sum > count"])
    percentage = 100 * (less + greater) / int(result["Snapshots"])
    operations = int(result["Recorded operations"])
    final_count = int(result["Final snapshot count"])
    difference = int(result["Diff"])

    if return_code == 1 and difference == 0:
        raise RuntimeError(f"unexpected failure: {impl}, threads={threads}")

    return percentage, less, greater, operations, final_count, difference


def main():
    implementations = ["striped", "tls", "double-buffer", "double-buffer-unsafe"]

    with open("results/stress.csv", "w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow([
            "impl", "threads", "snapshots", "inconsistent_percent",
            "less", "greater", "operations", "final_count", "difference",
        ])

        for impl in implementations:
            threads = 4
            snapshots = 10000
            repeats = 10 if impl == "double-buffer-unsafe" else 1
            results = []

            for _ in range(repeats):
                results.append(run_stress(impl, threads, snapshots))

            percentage, less, greater, operations, final_count, difference = [
                median(values) for values in zip(*results)
            ]

            writer.writerow([
                impl, threads, snapshots, f"{percentage:.2f}",
                less, greater, operations, final_count, difference,
            ])
            output.flush()


if __name__ == "__main__":
    main()
