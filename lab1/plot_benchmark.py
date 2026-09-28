import csv

import matplotlib.pyplot as plt
from matplotlib.ticker import NullFormatter


def main():
    rows = []
    with open("results/benchmark.csv", newline="") as source:
        for row in csv.DictReader(source):
            rows.append((row["impl"], int(row["threads"]), float(row["median"])))

    figure, axis = plt.subplots(figsize=(8, 4.5))

    for name in dict.fromkeys(row[0] for row in rows):
        points = sorted([row for row in rows if row[0] == name], key=lambda row: row[1])
        axis.plot(
            [row[1] for row in points],
            [row[2] / 1_000_000 for row in points],
            marker="o",
            linestyle="-",
            label=name,
        )

    axis.set_xticks(sorted({row[1] for row in rows}))
    axis.set_xlabel("Threads")
    axis.set_ylabel("Operations (M) / second (log scale)")
    axis.set_yscale("log")
    y_ticks = [20, 30, 50, 100, 200, 300, 500, 1000, 2000]
    axis.set_yticks(y_ticks, labels=[str(value) for value in y_ticks])
    axis.yaxis.set_minor_formatter(NullFormatter())
    axis.grid(True, which="both", alpha=0.25)
    axis.legend()
    axis.set_title("Collector throughput")
    figure.tight_layout()
    figure.savefig("results/benchmark.png", dpi=160)


if __name__ == "__main__":
    main()
