import csv
import os
import sys
from collections import defaultdict

LOCAL_LIBS = os.path.join(os.path.dirname(__file__), ".python-libs")
if os.path.isdir(LOCAL_LIBS):
    sys.path.insert(0, LOCAL_LIBS)

import matplotlib.pyplot as plt


COLORS = {
    "quick_sort": "#d62728",
    "merge_sort": "#1f77b4",
    "ternary_string_quick_sort": "#2ca02c",
    "string_merge_sort_lcp": "#9467bd",
    "msd_radix_sort": "#ff7f0e",
    "msd_radix_sort_with_cutoff": "#17becf",
}

ALGORITHM_LABELS = {
    "quick_sort": "QuickSort",
    "merge_sort": "MergeSort",
    "ternary_string_quick_sort": "3-way String QuickSort",
    "string_merge_sort_lcp": "String MergeSort + LCP",
    "msd_radix_sort": "MSD Radix Sort",
    "msd_radix_sort_with_cutoff": "MSD Radix Sort + cutoff",
}

DATASET_LABELS = {
    "random": "Random strings",
    "reversed": "Reverse sorted strings",
    "nearly_sorted": "Nearly sorted strings",
    "common_prefix": "Strings with common prefixes",
}

METRICS = {
    "avg_microseconds": ("Average runtime", "microseconds", "time"),
    "avg_char_comparisons": ("Character comparisons / inspections", "count", "char_comparisons"),
}


def read_rows(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def group_points(rows, dataset, metric):
    result = defaultdict(list)
    for row in rows:
        if row["dataset"] != dataset:
            continue
        result[row["algorithm"]].append((int(row["n"]), float(row[metric])))

    for algorithm in result:
        result[algorithm].sort()
    return result


def save_plot(rows, dataset, metric, output_path):
    points = group_points(rows, dataset, metric)
    metric_title, y_label, _ = METRICS[metric]

    plt.figure(figsize=(12, 7))
    for algorithm in sorted(points):
        xs = [x for x, _ in points[algorithm]]
        ys = [y for _, y in points[algorithm]]
        plt.plot(
            xs,
            ys,
            marker="o",
            markersize=3,
            linewidth=2,
            color=COLORS.get(algorithm),
            label=ALGORITHM_LABELS.get(algorithm, algorithm),
        )

    plt.title(f"{DATASET_LABELS.get(dataset, dataset)}: {metric_title}", fontsize=15, pad=14)
    plt.xlabel("Array size, n")
    plt.ylabel(y_label)
    plt.grid(True, linestyle="--", linewidth=0.6, alpha=0.45)
    plt.legend(loc="best", fontsize=9)
    plt.tight_layout()
    plt.savefig(output_path, dpi=180)
    plt.close()


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "string_sort_results.csv"
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "plots"
    extension = sys.argv[3] if len(sys.argv) > 3 else "png"

    if extension not in {"png", "svg", "pdf"}:
        raise ValueError("Output extension must be one of: png, svg, pdf")

    os.makedirs(output_dir, exist_ok=True)
    rows = read_rows(csv_path)
    datasets = sorted({row["dataset"] for row in rows})

    for dataset in datasets:
        for metric, (_, _, metric_suffix) in METRICS.items():
            output_path = os.path.join(output_dir, f"{dataset}_{metric_suffix}.{extension}")
            save_plot(rows, dataset, metric, output_path)

    print(f"Matplotlib plots written to {output_dir}")


if __name__ == "__main__":
    main()
