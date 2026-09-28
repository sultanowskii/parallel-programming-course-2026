import csv
import subprocess


def run_benchmark(impl, threads):
    command = ["./bench", "benchmark", "--impl", impl, "--threads", str(threads)]
    print("\n" + " ".join(command), flush=True)

    process = subprocess.Popen(command, stdout=subprocess.PIPE, text=True)
    median = None

    for line in process.stdout:
        print(line, end="", flush=True)

        if line.startswith("Median:"):
            median = float(line.split()[1])

    if process.wait() != 0 or median is None:
        raise RuntimeError(f"benchmark failed: {impl}, threads={threads}")

    return median


def main():
    points = [("plain", 1)]
    for impl in ["mutex", "empty_mutex", "striped", "tls", "double-buffer"]:
        for threads in [1, 2, 4, 8]:
            points.append((impl, threads))

    with open("results/benchmark.csv", "w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow(["impl", "threads", "median"])

        for impl, threads in points:
            median = run_benchmark(impl, threads)
            writer.writerow([impl, threads, f"{median:.2f}"])
            output.flush()


if __name__ == "__main__":
    main()
