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
                "history_mode": row.get("history_mode", "unknown"),
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
        key = (row["run_id"], row["history_mode"], row["policy"], row["parallelism"])
        by_run[key].append(row)

    throughput_rows = []
    for (run_id, history_mode, policy, parallelism), group in by_run.items():
        first_submit = min(r["submit_time_ms"] for r in group)
        last_end = max(r["end_time_ms"] for r in group)
        duration_s = max((last_end - first_submit) / 1000.0, 1e-6)
        throughput_rows.append(
            {
                "run_id": run_id,
                "history_mode": history_mode,
                "policy": policy,
                "parallelism": parallelism,
                "throughput_cmd_s": len(group) / duration_s,
            }
        )
    return throughput_rows


def export_summary(rows, throughput_rows, output_dir):
    by_cfg = defaultdict(list)
    for row in rows:
        by_cfg[(row["history_mode"], row["policy"], row["parallelism"])].append(row)

    throughput_by_cfg = defaultdict(list)
    for row in throughput_rows:
        throughput_by_cfg[(row["history_mode"], row["policy"], row["parallelism"])].append(row["throughput_cmd_s"])

    out_path = os.path.join(output_dir, "summary_metrics.csv")
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "history_mode",
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

        for history_mode, policy, parallelism in sorted(by_cfg.keys(), key=lambda x: (x[0], x[1], x[2])):
            group = by_cfg[(history_mode, policy, parallelism)]
            waits = [r["waiting_ms"] for r in group]
            turns = [r["turnaround_ms"] for r in group]
            slowdowns = [calculate_slowdown(r) for r in group]
            execs = [r["duration_ms"] for r in group]
            throughputs = throughput_by_cfg.get((history_mode, policy, parallelism), [0.0])

            writer.writerow(
                [
                    history_mode,
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


def plot_policy_wait(rows, throughput_rows, output_dir):
    if plt is None:
        return

    by_policy = defaultdict(list)
    for row in rows:
        by_policy[row["policy"]].append(row["waiting_ms"])

    throughput_by_policy = defaultdict(list)
    for row in throughput_rows:
        throughput_by_policy[row["policy"]].append(row["throughput_cmd_s"])

    policies = sorted(by_policy.keys())
    wait_values = [mean(by_policy[p]) for p in policies]
    throughput_values = [mean(throughput_by_policy[p]) if throughput_by_policy[p] else 0.0 for p in policies]

    plt.figure(figsize=(10, 5))
    x_pos = list(range(len(policies)))
    bar_width = 0.35
    plt.bar([x - bar_width / 2 for x in x_pos], wait_values, width=bar_width, color='steelblue', edgecolor='navy', alpha=0.7, label="Waiting Time (ms)")
    plt.bar([x + bar_width / 2 for x in x_pos], throughput_values, width=bar_width, color='lightgreen', edgecolor='darkgreen', alpha=0.7, label="Throughput (commands/s)")
    plt.xticks(x_pos, policies, fontsize=11)
    plt.title("Average Waiting Time and Throughput by Policy", fontsize=12, fontweight='bold')
    plt.xlabel("Policy", fontsize=11)
    plt.ylabel("Metric Value", fontsize=11)
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "policy_vs_wait.png"), dpi=150)
    plt.close()


def plot_policy_comparison_by_parallelism(rows, throughput_rows, output_dir):
    if plt is None:
        return

    by_parallelism = defaultdict(lambda: defaultdict(list))
    for row in rows:
        by_parallelism[row["parallelism"]][row["policy"]].append(row)

    throughput_by_parallelism = defaultdict(lambda: defaultdict(list))
    for row in throughput_rows:
        throughput_by_parallelism[row["parallelism"]][row["policy"]].append(
            row["throughput_cmd_s"]
        )

    for parallelism in sorted(by_parallelism.keys()):
        policy_groups = by_parallelism[parallelism]
        policies = sorted(policy_groups.keys())
        if not policies:
            continue

        wait_values = [mean([r["waiting_ms"] for r in policy_groups[policy]]) for policy in policies]
        turnaround_values = [mean([r["turnaround_ms"] for r in policy_groups[policy]]) for policy in policies]
        throughput_values = [
            mean(throughput_by_parallelism[parallelism].get(policy, [0.0]))
            for policy in policies
        ]

        x_positions = list(range(len(policies)))
        bar_width = 0.22

        fig, ax_left = plt.subplots(figsize=(13, 5))
        ax_right = ax_left.twinx()

        wait_bars = ax_left.bar(
            [x - bar_width for x in x_positions],
            wait_values,
            width=bar_width,
            label="Waiting",
            color='skyblue',
            edgecolor='black',
            linewidth=0.5,
        )
        turnaround_bars = ax_left.bar(
            x_positions,
            turnaround_values,
            width=bar_width,
            label="Turnaround",
            color='orange',
            edgecolor='black',
            linewidth=0.5,
        )
        throughput_bars = ax_right.bar(
            [x + bar_width for x in x_positions],
            throughput_values,
            width=bar_width,
            label="Throughput",
            color='lightgreen',
            edgecolor='black',
            linewidth=0.5,
        )

        ax_left.set_xticks(x_positions)
        ax_left.set_xticklabels(policies, fontsize=11)
        ax_left.set_title(f"Policy Comparison at Parallelism {parallelism}", fontsize=12, fontweight='bold')
        ax_left.set_xlabel("Policy", fontsize=11)
        ax_left.set_ylabel("Waiting / Turnaround (ms)", fontsize=11)
        ax_right.set_ylabel("Throughput (commands/s)", fontsize=11)
        ax_left.grid(True, alpha=0.3, axis='y')

        handles = [wait_bars, turnaround_bars, throughput_bars]
        labels = [handle.get_label() for handle in handles]
        ax_left.legend(handles, labels, fontsize=10, loc="upper left")
        fig.tight_layout()
        plt.savefig(
            os.path.join(output_dir, f"policy_comparison_parallelism_{parallelism}.png"),
            dpi=150,
        )
        plt.close(fig)


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

    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']
    markers = ['o', 's', '^', 'D']
    plt.figure(figsize=(10, 5))
    for idx, policy in enumerate(policies):
        cfg = sorted([k for k in turns_cfg.keys() if k[0] == policy], key=lambda x: x[1])
        x = [parallelism for _, parallelism in cfg]
        y = [mean(turns_cfg[key]) for key in cfg]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Parallelism Impact on Turnaround", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("Average Turnaround (ms)", fontsize=11)
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "parallelism_vs_turnaround.png"), dpi=150)
    plt.close()

    plt.figure(figsize=(10, 5))
    for idx, policy in enumerate(policies):
        cfg = sorted([k for k in th_cfg.keys() if k[0] == policy], key=lambda x: x[1])
        x = [parallelism for _, parallelism in cfg]
        y = [mean(th_cfg[key]) for key in cfg]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Parallelism Impact on Throughput", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("Throughput (commands/s)", fontsize=11)
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
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
    
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']
    markers = ['o', 's', '^', 'D']
    
    # Plot 1: Coefficient of Variation for Waiting Time
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["cv_wait"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Fairness: Coefficient of Variation (Waiting Time)\nLower = More Fair", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("CV (std_dev/mean)", fontsize=11)
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "fairness_cv_waiting.png"), dpi=150)
    plt.close()
    
    # Plot 2: Jain's Fairness Index for Waiting Time
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["jain_wait"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Fairness: Jain's Index (Waiting Time)\nHigher = More Fair (1.0 = Perfect)", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("Jain's Index (0 to 1)", fontsize=11)
    plt.ylim([0, 1.05])
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "fairness_jain_waiting.png"), dpi=150)
    plt.close()
    
    # Plot 3: Coefficient of Variation for Turnaround Time
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["cv_turn"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Fairness: Coefficient of Variation (Turnaround Time)\nLower = More Fair", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("CV (std_dev/mean)", fontsize=11)
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "fairness_cv_turnaround.png"), dpi=150)
    plt.close()
    
    # Plot 4: Jain's Fairness Index for Turnaround Time
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["jain_turn"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Fairness: Jain's Index (Turnaround Time)\nHigher = More Fair (1.0 = Perfect)", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("Jain's Index (0 to 1)", fontsize=11)
    plt.ylim([0, 1.05])
    plt.legend(fontsize=10)
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
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']
    markers = ['o', 's', '^', 'D']
    
    # Plot 1: Mean Slowdown vs Parallelism
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["mean_slowdown"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Slowdown: Average Normalized Turnaround Time\nLower = Better (1.0 = No Delay)", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("Mean Slowdown (Turnaround/Duration)", fontsize=11)
    plt.axhline(y=1.0, color="green", linestyle=":", linewidth=2, alpha=0.7, label="Ideal (1.0)")
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_mean.png"), dpi=150)
    plt.close()
    
    # Plot 2: P95 Slowdown
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["p95_slowdown"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Slowdown: 95th Percentile\nWorst 5% of Commands - Lower = Better", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("P95 Slowdown", fontsize=11)
    plt.axhline(y=1.0, color="green", linestyle=":", linewidth=2, alpha=0.7, label="Ideal (1.0)")
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_p95.png"), dpi=150)
    plt.close()
    
    # Plot 3: P99 Slowdown
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["p99_slowdown"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Slowdown: 99th Percentile\nWorst 1% of Commands - Lower = Better", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("P99 Slowdown", fontsize=11)
    plt.axhline(y=1.0, color="green", linestyle=":", linewidth=2, alpha=0.7, label="Ideal (1.0)")
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_p99.png"), dpi=150)
    plt.close()
    
    # Plot 4: Slowdown CV (fairness)
    plt.figure(figsize=(11, 5))
    for idx, policy in enumerate(policies):
        data = sorted(by_policy[policy], key=lambda x: x["parallelism"])
        x = [d["parallelism"] for d in data]
        y = [d["cv_slowdown"] for d in data]
        plt.plot(x, y, marker=markers[idx], markersize=8, linewidth=2.5, label=policy, color=colors[idx])
    plt.title("Slowdown Fairness: Coefficient of Variation\nLower = More Fair (Equal Slowdown)", fontsize=12, fontweight='bold')
    plt.xlabel("Parallelism", fontsize=11)
    plt.ylabel("CV (std_dev/mean)", fontsize=11)
    plt.legend(fontsize=10)
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
    plt.figure(figsize=(12, 6))
    data = [by_policy[policy] for policy in policies]
    bp = plt.boxplot(data, tick_labels=policies, showfliers=True, widths=0.6)
    plt.title("Slowdown Distribution by Policy\nBox: 25-75%, Line: Median, Dots: Outliers", fontsize=12, fontweight='bold')
    plt.ylabel("Slowdown (Turnaround/Duration)", fontsize=11)
    plt.xlabel("Policy", fontsize=11)
    plt.axhline(y=1.0, color="green", linestyle=":", alpha=0.5, linewidth=2, label="Ideal (1.0)")
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3, axis="y")
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, "slowdown_distribution.png"), dpi=150)
    plt.close()


def plot_history_mode_comparison(rows, throughput_rows, output_dir):
    """Compare cold and warm runs to show the effect of persisted history."""
    if plt is None:
        return

    history_modes = sorted({row["history_mode"] for row in rows})
    if len(history_modes) < 2:
        return

    by_policy_history = defaultdict(list)
    for row in rows:
        by_policy_history[(row["policy"], row["history_mode"])].append(row)

    throughput_by_policy_history = defaultdict(list)
    for row in throughput_rows:
        throughput_by_policy_history[(row["policy"], row["history_mode"])].append(
            row["throughput_cmd_s"]
        )

    policies = sorted({row["policy"] for row in rows})
    metrics = [
        ("waiting_ms", "Average Waiting Time by History Mode", "Waiting Time (ms)", "history_waiting.png"),
        ("turnaround_ms", "Average Turnaround by History Mode", "Turnaround (ms)", "history_turnaround.png"),
        ("slowdown", "Average Slowdown by History Mode", "Slowdown (turnaround/duration)", "history_slowdown.png"),
        ("throughput", "Throughput by History Mode", "Commands/s", "history_throughput.png"),
    ]

    colors = {
        "cold": "#4c78a8",
        "warm": "#f58518",
        "unknown": "#777777",
    }

    for metric_key, title, ylabel, filename in metrics:
        x_positions = list(range(len(policies)))
        bar_width = min(0.8 / len(history_modes), 0.35)

        plt.figure(figsize=(12, 5))
        for idx, history_mode in enumerate(history_modes):
            offset = (idx - (len(history_modes) - 1) / 2) * bar_width
            values = []
            for policy in policies:
                if metric_key == "throughput":
                    samples = throughput_by_policy_history.get((policy, history_mode), [])
                    values.append(mean(samples) if samples else 0.0)
                    continue

                group = by_policy_history.get((policy, history_mode), [])
                if metric_key == "slowdown":
                    samples = [calculate_slowdown(row) for row in group]
                else:
                    samples = [row[metric_key] for row in group]
                values.append(mean(samples) if samples else 0.0)

            plt.bar(
                [x + offset for x in x_positions],
                values,
                width=bar_width,
                label=history_mode,
                color=colors.get(history_mode, None),
                edgecolor="black",
                linewidth=0.5,
            )

        plt.xticks(x_positions, policies)
        plt.title(title, fontsize=12, fontweight="bold")
        plt.xlabel("Policy")
        plt.ylabel(ylabel)
        plt.legend(title="History")
        plt.grid(True, alpha=0.3, axis="y")
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, filename), dpi=150)
        plt.close()


def plot_history_impact_by_parallelism(rows, output_dir):
    """For each policy, compare cold vs warm as parallelism changes."""
    if plt is None:
        return

    history_modes = sorted({row["history_mode"] for row in rows})
    if len(history_modes) < 2:
        return

    by_cfg = defaultdict(list)
    for row in rows:
        by_cfg[(row["policy"], row["history_mode"], row["parallelism"])].append(row)

    policies = sorted({row["policy"] for row in rows})
    colors = {
        "cold": "#4c78a8",
        "warm": "#f58518",
        "unknown": "#777777",
    }
    markers = {
        "cold": "o",
        "warm": "s",
        "unknown": "D",
    }

    for policy in policies:
        safe_policy = "".join(ch.lower() if ch.isalnum() else "_" for ch in policy).strip("_")
        if not safe_policy:
            safe_policy = "unknown"

        for metric_key, title_suffix, ylabel, filename_suffix in [
            ("turnaround_ms", "Turnaround", "Average Turnaround (ms)", "turnaround"),
            ("slowdown", "Slowdown", "Average Slowdown", "slowdown"),
        ]:
            plt.figure(figsize=(10, 5))

            for history_mode in history_modes:
                cfgs = sorted(
                    [key for key in by_cfg.keys() if key[0] == policy and key[1] == history_mode],
                    key=lambda x: x[2],
                )
                x_values = [parallelism for _, _, parallelism in cfgs]
                y_values = []
                for key in cfgs:
                    group = by_cfg[key]
                    if metric_key == "slowdown":
                        y_values.append(mean(calculate_slowdown(row) for row in group))
                    else:
                        y_values.append(mean(row[metric_key] for row in group))

                if not x_values:
                    continue

                plt.plot(
                    x_values,
                    y_values,
                    marker=markers.get(history_mode, "o"),
                    markersize=8,
                    linewidth=2.5,
                    label=history_mode,
                    color=colors.get(history_mode, None),
                )

            plt.title(f"{policy}: {title_suffix} with Cold vs Warm History", fontsize=12, fontweight="bold")
            plt.xlabel("Parallelism")
            plt.ylabel(ylabel)
            plt.legend(title="History")
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.savefig(
                os.path.join(output_dir, f"history_{filename_suffix}_{safe_policy}.png"),
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
    plot_policy_wait(rows, throughput_rows, output_dir)
    plot_policy_comparison_by_parallelism(rows, throughput_rows, output_dir)
    plot_parallelism_impact(rows, throughput_rows, output_dir)
    plot_user_distribution(rows, output_dir)
    plot_fairness_metrics(rows, output_dir)
    plot_per_user_fairness(rows, output_dir)
    plot_slowdown_metrics(rows, output_dir)
    plot_slowdown_distribution(rows, output_dir)
    plot_history_mode_comparison(rows, throughput_rows, output_dir)
    plot_history_impact_by_parallelism(rows, output_dir)

    if plt is None:
        print("matplotlib not installed: generated summary_metrics.csv only.")

    print(f"Analysis complete. Output directory: {output_dir}")


if __name__ == "__main__":
    main()
