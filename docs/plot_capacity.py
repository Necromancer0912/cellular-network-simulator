import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))

# These values are Protocol::calculate_max_users(10.0) / calculate_required_cores(...)
# output at a 10 MHz reference bandwidth - see gen_capacity_table.cpp in this
# directory to regenerate them straight from the current code rather than
# trusting these hardcoded numbers to stay in sync by hand.
gens = ["2G", "3G", "4G", "5G", "6G", "7G"]
max_users = [800, 1600, 120000, 484800, 6720000, 268800000]
cores = [20, 19, 1320, 5236, 37632, 830592]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.5))

colors = ["#dc2626", "#ea580c", "#ca8a04", "#16a34a", "#0891b2", "#7c3aed"]
ax1.bar(gens, max_users, color=colors)
ax1.set_yscale("log")
ax1.set_ylabel("Max users (log scale)")
ax1.set_title("Modeled capacity per generation\n(10 MHz bandwidth)")
ax1.grid(axis="y", alpha=0.3)

ax2.bar(gens, cores, color=colors)
ax2.set_yscale("log")
ax2.set_ylabel("Required cores (log scale)")
ax2.set_title("Modeled cores needed to serve\nthat many users")
ax2.grid(axis="y", alpha=0.3)

fig.suptitle("Protocol.calculate_max_users() / calculate_required_cores() output, 2G-7G", fontsize=10, y=1.02)
fig.tight_layout()
fig.savefig(os.path.join(HERE, "capacity_comparison.png"), dpi=150, bbox_inches="tight")
print("saved")
