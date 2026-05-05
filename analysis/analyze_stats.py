#!/usr/bin/env python3
import csv
import os
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from collections import defaultdict
from statistics import mean, stdev

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


def coefficient_of_variation(values):
    """
    Coefficient of Variation = std_dev / mean
    Measures relative variability. Lower = more fair.
    Values close to 0: very fair. Values > 1: highly unfair.
    """
    if not values or len(values) < 2:
        return 0.0
    m = mean(values)
    if m == 0:
        return 0.0
    return stdev(values) / m


def jain_fairness_index(values):
    """
    Jain's Fairness Index = (Σx)² / (n × Σx²)
    Range: 0 to 1. Higher = more fair.
    1.0 = perfectly fair. 1/n = completely unfair.
    """
    if not values:
        return 0.0
    n = len(values)
    if n == 1:
        return 1.0
    sum_x = sum(values)
    sum_x2 = sum(x**2 for x in values)
    if sum_x2 == 0:
        return 0.0
    return (sum_x**2) / (n * sum_x2)


def max_min_ratio(values):
    """
    Max/Min ratio of execution times.
    How many times longer is the longest compared to shortest.
    Lower = more fair (closer to 1).
    """
    if not values or len(values) == 0:
        return 1.0
    min_v = min(values)
    max_v = max(values)
    if min_v == 0:
        return float('inf') if max_v > 0 else 1.0
    return max_v / min_v


def calculate_slowdown(row):
    """
    Slowdown (Normalized Turnaround Time) = Turnaround / Service_Time
    Measures how much a command was delayed by scheduling.
    - Slowdown = 1.0: Command executed immediately without waiting
    - Slowdown = 5.0: Command took 5x longer due to waiting
    
    Professional interpretation:
    - < 2.0: Good (at most doubled the execution time)
    - 2.0-5.0: Acceptable
    - > 5.0: Poor (command delayed significantly)
    """
    duration = row["duration_ms"]
    turnaround = row["turnaround_ms"]
    if duration == 0:
        return 1.0
    return turnaround / duration


def load_data(csv_path):
    with open(csv_path, newline="", encoding="utf-8") as f:
        """ O reader representa um conjunto de linhas do ficheiro csv, onde cada linha é um dicionário com as colunas como chaves """
        reader = csv.DictReader(f)

        """ Dá erro se tiver colunas a faltar no ficheiro gerado no script """
        missing = [col for col in REQUIRED_COLUMNS if col not in (reader.fieldnames or [])]
        if missing:
            raise ValueError(f"Missing required columns: {', '.join(missing)}")

        """ Converte os valores das colunas, passando o que for preciso para inteiro e armazena num novo dicionário """
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
                "p99_wait_ms",
                "cv_wait",
                "jain_wait",
                "mean_turnaround_ms",
                "p95_turnaround_ms",
                "p99_turnaround_ms",
                "cv_turnaround",
                "jain_turnaround",
                "mean_slowdown",
                "p95_slowdown",
                "p99_slowdown",
                "cv_slowdown",
                "jain_slowdown",
                "max_min_wait_ratio",
                "max_min_turnaround_ratio",
                "mean_exec_ms",
                "mean_throughput_cmd_s",
            ]
        )

        for policy, parallelism in sorted(by_cfg.keys(), key=lambda x: (x[0], x[1])):
            group = by_cfg[(policy, parallelism)]
            waits = [r["waiting_ms"] for r in group]
            turns = [r["turnaround_ms"] for r in group]
            slowdowns = [calculate_slowdown(r) for r in group]
            execs = [r["duration_ms"] for r in group]
            throughputs = throughput_by_cfg.get((policy, parallelism), [0.0])

            writer.writerow(
                [
                    policy,
                    parallelism,
                    round(mean(waits), 2),
                    round(percentile(waits, 95), 2),
                    round(percentile(waits, 99), 2),
                    round(coefficient_of_variation(waits), 4),
                    round(jain_fairness_index(waits), 4),
                    round(mean(turns), 2),
                    round(percentile(turns, 95), 2),
                    round(percentile(turns, 99), 2),
                    round(coefficient_of_variation(turns), 4),
                    round(jain_fairness_index(turns), 4),
                    round(mean(slowdowns), 4),
                    round(percentile(slowdowns, 95), 4),
                    round(percentile(slowdowns, 99), 4),
                    round(coefficient_of_variation(slowdowns), 4),
                    round(jain_fairness_index(slowdowns), 4),
                    round(max_min_ratio(waits), 2),
                    round(max_min_ratio(turns), 2),
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


def plot_fairness_metrics(rows, output_dir):
    """Plot fairness metrics (CV and Jain's Index) for all policies."""
    if plt is None:
        return
    
    by_cfg = defaultdict(list)
    for row in rows:
        by_cfg[(row["policy"], row["parallelism"])].append(row)
    
    # Group by policy only for fairness comparison
    by_policy = defaultdict(list)
    for (policy, parallelism), group in by_cfg.items():
        waits = [r["waiting_ms"] for r in group]
        turns = [r["turnaround_ms"] for r in group]
        by_policy[policy].append({
            "parallelism": parallelism,
            "cv_wait": coefficient_of_variation(waits),
            "jain_wait": jain_fairness_index(waits),
            "cv_turn": coefficient_of_variation(turns),
            "jain_turn": jain_fairness_index(turns),
        })
    
    policies = sorted(by_policy.keys())
    
    # Plot 1: Coefficient of Variation for Waiting Time
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["cv_wait"] for d in data]
        plt.plot(x, y, marker="o", label=policy)
    plt.title("Fairness: Coefficient of Variation (Waiting Time)\nLower = More Fair")
    plt.xlabel("Parallelism")
    plt.ylabel("CV (std_dev/mean)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "fairness_cv_waiting.png"), dpi=150)
    plt.close()
    
    # Plot 2: Jain's Fairness Index for Waiting Time
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["jain_wait"] for d in data]
        plt.plot(x, y, marker="s", label=policy)
    plt.title("Fairness: Jain's Index (Waiting Time)\nHigher = More Fair (1.0 = Perfect)")
    plt.xlabel("Parallelism")
    plt.ylabel("Jain's Index (0 to 1)")
    plt.ylim([0, 1.05])
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "fairness_jain_waiting.png"), dpi=150)
    plt.close()
    
    # Plot 3: Coefficient of Variation for Turnaround Time
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["cv_turn"] for d in data]
        plt.plot(x, y, marker="^", label=policy)
    plt.title("Fairness: Coefficient of Variation (Turnaround Time)\nLower = More Fair")
    plt.xlabel("Parallelism")
    plt.ylabel("CV (std_dev/mean)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "fairness_cv_turnaround.png"), dpi=150)
    plt.close()
    
    # Plot 4: Jain's Fairness Index for Turnaround Time
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["jain_turn"] for d in data]
        plt.plot(x, y, marker="d", label=policy)
    plt.title("Fairness: Jain's Index (Turnaround Time)\nHigher = More Fair (1.0 = Perfect)")
    plt.xlabel("Parallelism")
    plt.ylabel("Jain's Index (0 to 1)")
    plt.ylim([0, 1.05])
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "fairness_jain_turnaround.png"), dpi=150)
    plt.close()


def plot_per_user_fairness(rows, output_dir):
    """Plot average turnaround time per user to show user-level fairness."""
    if plt is None:
        return
    
    by_policy_user = defaultdict(lambda: defaultdict(list))
    for row in rows:
        by_policy_user[row["policy"]][row["user_id"]].append(row["turnaround_ms"])
    
    for policy in sorted(by_policy_user.keys()):
        by_user = by_policy_user[policy]
        users = sorted(by_user.keys())
        
        if not users:
            continue
        
        user_means = [mean(by_user[u]) for u in users]
        
        plt.figure(figsize=(10, 5))
        bars = plt.bar([str(u) for u in users], user_means, color="skyblue", edgecolor="navy")
        
        # Add average line
        overall_mean = mean(user_means)
        plt.axhline(y=overall_mean, color="r", linestyle="--", label=f"Mean: {overall_mean:.0f}ms")
        
        plt.title(f"User-Level Fairness: Average Turnaround Time ({policy})")
        plt.xlabel("User ID")
        plt.ylabel("Average Turnaround Time (ms)")
        plt.legend()
        plt.tight_layout()
        
        safe_policy = "".join(ch.lower() if ch.isalnum() else "_" for ch in policy).strip("_")
        if not safe_policy:
            safe_policy = "unknown"
        
        plt.savefig(os.path.join(output_dir, f"user_fairness_{safe_policy}.png"), dpi=150)
        plt.close()


def plot_slowdown_metrics(rows, output_dir):
    """Plot Slowdown (Normalized Turnaround Time) metrics."""
    if plt is None:
        return
    
    by_cfg = defaultdict(list)
    for row in rows:
        slowdown = calculate_slowdown(row)
        by_cfg[(row["policy"], row["parallelism"])].append(slowdown)
    
    # Group by policy for comparison
    by_policy = defaultdict(list)
    for (policy, parallelism), slowdowns in by_cfg.items():
        by_policy[policy].append({
            "parallelism": parallelism,
            "mean_slowdown": mean(slowdowns),
            "p95_slowdown": percentile(slowdowns, 95),
            "p99_slowdown": percentile(slowdowns, 99),
            "cv_slowdown": coefficient_of_variation(slowdowns),
            "jain_slowdown": jain_fairness_index(slowdowns),
        })
    
    policies = sorted(by_policy.keys())
    
    # Plot 1: Mean Slowdown vs Parallelism
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["mean_slowdown"] for d in data]
        plt.plot(x, y, marker="o", label=policy)
    plt.title("Slowdown: Average Normalized Turnaround Time\nLower = Better (1.0 = No Delay)")
    plt.xlabel("Parallelism")
    plt.ylabel("Mean Slowdown (Turnaround/Duration)")
    plt.axhline(y=1.0, color="green", linestyle=":", alpha=0.5, label="Ideal (1.0)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_mean.png"), dpi=150)
    plt.close()
    
    # Plot 2: P95 Slowdown
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["p95_slowdown"] for d in data]
        plt.plot(x, y, marker="s", label=policy)
    plt.title("Slowdown: 95th Percentile\nWorst 5% of Commands - Lower = Better")
    plt.xlabel("Parallelism")
    plt.ylabel("P95 Slowdown")
    plt.axhline(y=1.0, color="green", linestyle=":", alpha=0.5, label="Ideal (1.0)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_p95.png"), dpi=150)
    plt.close()
    
    # Plot 3: P99 Slowdown
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["p99_slowdown"] for d in data]
        plt.plot(x, y, marker="^", label=policy)
    plt.title("Slowdown: 99th Percentile\nWorst 1% of Commands - Lower = Better")
    plt.xlabel("Parallelism")
    plt.ylabel("P99 Slowdown")
    plt.axhline(y=1.0, color="green", linestyle=":", alpha=0.5, label="Ideal (1.0)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_p99.png"), dpi=150)
    plt.close()
    
    # Plot 4: Slowdown CV (fairness)
    plt.figure(figsize=(10, 5))
    for policy in policies:
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["cv_slowdown"] for d in data]
        plt.plot(x, y, marker="d", label=policy)
    plt.title("Slowdown Fairness: Coefficient of Variation\nLower = More Fair (Equal Slowdown)")
    plt.xlabel("Parallelism")
    plt.ylabel("CV (std_dev/mean)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_cv.png"), dpi=150)
    plt.close()


def plot_slowdown_distribution(rows, output_dir):
    """Plot distribution of slowdown values by policy."""
    if plt is None:
        return
    
    by_policy = defaultdict(list)
    for row in rows:
        slowdown = calculate_slowdown(row)
        by_policy[row["policy"]].append(slowdown)
    
    policies = sorted(by_policy.keys())
    if not policies:
        return
    
    # Box plot of slowdown distribution
    plt.figure(figsize=(10, 6))
    data = [by_policy[policy] for policy in policies]
    bp = plt.boxplot(data, labels=policies, showfliers=True)
    plt.title("Slowdown Distribution by Policy\nBox: 25-75%, Line: Median, Dots: Outliers")
    plt.ylabel("Slowdown (Turnaround/Duration)")
    plt.axhline(y=1.0, color="green", linestyle=":", alpha=0.5, label="Ideal (1.0)")
    plt.legend()
    plt.grid(True, alpha=0.3, axis="y")
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_distribution.png"), dpi=150)
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
    plot_fairness_metrics(rows, output_dir)
    plot_per_user_fairness(rows, output_dir)
    plot_slowdown_metrics(rows, output_dir)
    plot_slowdown_distribution(rows, output_dir)

    if plt is None:
        print("matplotlib not installed: generated summary_metrics.csv only.")

    print(f"Analysis complete. Output directory: {output_dir}")


if __name__ == "__main__":
    main()
