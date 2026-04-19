#!/usr/bin/env python3
import csv
import os
import sys
from collections import defaultdict
from statistics import mean

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except Exception:
    plt = None


REQUIRED_COLUMNS = [
    "run_id",
    "policy",
    "parallelism",
    "user_id",
    "command_id",
    "submit_time_ms",
    "start_time_ms",
    "end_time_ms",
    "duration_ms",
    "turnaround_ms",
    "waiting_ms",
]


def parse_int(row, key):
    return int(float(row[key]))


def percentile(values, p):
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = int(round((p / 100.0) * (len(ordered) - 1)))
    return float(ordered[idx])


def load_data(csv_path):
    with open(csv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        missing = [col for col in REQUIRED_COLUMNS if col not in (reader.fieldnames or [])]
        if missing:
            raise ValueError(f"Missing required columns: {', '.join(missing)}")

        rows = []
        for row in reader:
            parsed = {
                "run_id": parse_int(row, "run_id"),
                "policy": row["policy"],
                "parallelism": parse_int(row, "parallelism"),
                "user_id": parse_int(row, "user_id"),
                "command_id": parse_int(row, "command_id"),
                "submit_time_ms": parse_int(row, "submit_time_ms"),
                "start_time_ms": parse_int(row, "start_time_ms"),
                "end_time_ms": parse_int(row, "end_time_ms"),
                "duration_ms": parse_int(row, "duration_ms"),
                "turnaround_ms": parse_int(row, "turnaround_ms"),
                "waiting_ms": parse_int(row, "waiting_ms"),
            }
            rows.append(parsed)
    return rows


def build_throughput(rows):
    by_run = defaultdict(list)
    for row in rows:
        key = (row["run_id"], row["policy"], row["parallelism"])
        by_run[key].append(row)

    throughput_rows = []
    for (run_id, policy, parallelism), group in by_run.items():
        first_submit = min(r["submit_time_ms"] for r in group)
        last_end = max(r["end_time_ms"] for r in group)
        duration_s = max((last_end - first_submit) / 1000.0, 1e-6)
        throughput_rows.append(
            {
                "run_id": run_id,
                "policy": policy,
                "parallelism": parallelism,
                "throughput_cmd_s": len(group) / duration_s,
            }
        )
    return throughput_rows


def export_summary(rows, throughput_rows, output_dir):
    by_cfg = defaultdict(list)
    for row in rows:
        by_cfg[(row["policy"], row["parallelism"])].append(row)

    throughput_by_cfg = defaultdict(list)
    for row in throughput_rows:
        throughput_by_cfg[(row["policy"], row["parallelism"])].append(row["throughput_cmd_s"])

    out_path = os.path.join(output_dir, "summary_metrics.csv")
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "policy",
                "parallelism",
                "mean_wait_ms",
                "p95_wait_ms",
                "mean_turnaround_ms",
                "p95_turnaround_ms",
                "mean_exec_ms",
                "mean_throughput_cmd_s",
            ]
        )

        for policy, parallelism in sorted(by_cfg.keys(), key=lambda x: (x[0], x[1])):
            group = by_cfg[(policy, parallelism)]
            waits = [r["waiting_ms"] for r in group]
            turns = [r["turnaround_ms"] for r in group]
            execs = [r["duration_ms"] for r in group]
            throughputs = throughput_by_cfg.get((policy, parallelism), [0.0])

            writer.writerow(
                [
                    policy,
                    parallelism,
                    round(mean(waits), 2),
                    round(percentile(waits, 95), 2),
                    round(mean(turns), 2),
                    round(percentile(turns, 95), 2),
                    round(mean(execs), 2),
                    round(mean(throughputs), 4),
                ]
            )


def plot_policy_wait(rows, output_dir):
    if plt is None:
        return
    by_policy = defaultdict(list)
    for row in rows:
        by_policy[row["policy"]].append(row["waiting_ms"])

    policies = sorted(by_policy.keys())
    values = [mean(by_policy[p]) for p in policies]

    plt.figure(figsize=(8, 5))
    plt.bar(policies, values)
    plt.title("Average Waiting Time by Policy")
    plt.xlabel("Policy")
    plt.ylabel("Waiting Time (ms)")
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "policy_vs_wait.png"), dpi=150)
    plt.close()


def plot_parallelism_impact(rows, throughput_rows, output_dir):
    if plt is None:
        return

    turns_cfg = defaultdict(list)
    for row in rows:
        turns_cfg[(row["policy"], row["parallelism"])].append(row["turnaround_ms"])

    th_cfg = defaultdict(list)
    for row in throughput_rows:
        th_cfg[(row["policy"], row["parallelism"])].append(row["throughput_cmd_s"])

    policies = sorted({row["policy"] for row in rows})

    plt.figure(figsize=(9, 5))
    for policy in policies:
        cfg = sorted([k for k in turns_cfg.keys() if k[0] == policy], key=lambda x: x[1])
        x = [parallelism for _, parallelism in cfg]
        y = [mean(turns_cfg[key]) for key in cfg]
        plt.plot(x, y, marker="o", label=policy)
    plt.title("Parallelism Impact on Turnaround")
    plt.xlabel("Parallelism")
    plt.ylabel("Average Turnaround (ms)")
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "parallelism_vs_turnaround.png"), dpi=150)
    plt.close()

    plt.figure(figsize=(9, 5))
    for policy in policies:
        cfg = sorted([k for k in th_cfg.keys() if k[0] == policy], key=lambda x: x[1])
        x = [parallelism for _, parallelism in cfg]
        y = [mean(th_cfg[key]) for key in cfg]
        plt.plot(x, y, marker="o", label=policy)
    plt.title("Parallelism Impact on Throughput")
    plt.xlabel("Parallelism")
    plt.ylabel("Throughput (commands/s)")
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "parallelism_vs_throughput.png"), dpi=150)
    plt.close()


def plot_user_distribution(rows, output_dir):
    if plt is None:
        return
    by_policy_user = defaultdict(lambda: defaultdict(list))
    for row in rows:
        by_policy_user[row["policy"]][row["user_id"]].append(row["waiting_ms"])

    for policy in sorted(by_policy_user.keys()):
        by_user = by_policy_user[policy]
        users = sorted(by_user.keys())
        data = [by_user[user] for user in users]
        if not data:
            continue

        safe_policy = "".join(ch.lower() if ch.isalnum() else "_" for ch in policy).strip("_")
        if not safe_policy:
            safe_policy = "unknown"

        plt.figure(figsize=(10, 5))
        plt.boxplot(data, tick_labels=[str(u) for u in users], showfliers=False)
        plt.title(f"Waiting Time Distribution by User ({policy})")
        plt.xlabel("User ID")
        plt.ylabel("Waiting Time (ms)")
        plt.tight_layout()
        plt.savefig(
            os.path.join(output_dir, f"user_wait_distribution_{safe_policy}.png"),
            dpi=150,
        )
        plt.close()


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "tmp/stats.csv"
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "analysis/output"

    os.makedirs(output_dir, exist_ok=True)

    rows = load_data(csv_path)
    throughput_rows = build_throughput(rows)

    export_summary(rows, throughput_rows, output_dir)
    plot_policy_wait(rows, output_dir)
    plot_parallelism_impact(rows, throughput_rows, output_dir)
    plot_user_distribution(rows, output_dir)

    if plt is None:
        print("matplotlib not installed: generated summary_metrics.csv only.")

    print(f"Analysis complete. Output directory: {output_dir}")


if __name__ == "__main__":
    main()
