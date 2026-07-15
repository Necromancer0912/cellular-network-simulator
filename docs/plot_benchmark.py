import csv
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))

threads, speedups, single_ms, multi_ms = [], [], [], []
with open(os.path.join(HERE, "benchmark_results.csv")) as f:
    for row in csv.DictReader(f):
        threads.append(int(row["threads"]))
        speedups.append(float(row["speedup"]))
        single_ms.append(float(row["single_ms"]))
        multi_ms.append(float(row["multi_ms"]))

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))

ax1.plot(threads, speedups, marker="o", color="#2563eb", linewidth=2)
ax1.axhline(1.0, color="#999", linestyle="--", linewidth=1, label="no speedup (1.0x)")
ax1.set_xlabel("Worker threads")
ax1.set_ylabel("Speedup (single_ms / multi_ms)")
ax1.set_title("Speedup vs. thread count\n(100,000 devices, 8 towers)")
ax1.grid(alpha=0.3)
ax1.legend()

ax2.plot(threads, single_ms, marker="s", color="#dc2626", label="single-threaded", linewidth=2)
ax2.plot(threads, multi_ms, marker="o", color="#16a34a", label="multi-threaded (ThreadPool)", linewidth=2)
ax2.set_xlabel("Worker threads")
ax2.set_ylabel("Wall-clock time (ms)")
ax2.set_title("Absolute connection time\n(100,000 devices, 8 towers)")
ax2.grid(alpha=0.3)
ax2.legend()

fig.suptitle("Threading benchmark: connecting 100,000 devices across 8 independent 5G towers", fontsize=10, y=1.02)
fig.tight_layout()
fig.savefig(os.path.join(HERE, "benchmark_speedup.png"), dpi=150, bbox_inches="tight")
print("saved")
