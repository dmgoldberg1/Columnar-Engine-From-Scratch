from pathlib import Path
import csv

import matplotlib.pyplot as plt

INPUT = Path("string_compression_report_table.csv")
OUTPUT_COMBINED = Path("string_compression_comparison.svg")
OUTPUT_SCATTER = Path("string_compression_tradeoff.svg")

COLORS = {
    "dictionary": "#1f77b4",
    "adaptive": "#2ca02c",
    "delta_length": "#ff7f0e",
}

LABELS = {
    "dictionary": "Словарь",
    "adaptive": "Адаптивный",
    "delta_length": "Delta-length",
}


def load_rows():
    with INPUT.open() as f:
        rows = list(csv.DictReader(f))
    if not rows:
        raise RuntimeError(f"No rows found in {INPUT}")
    for row in rows:
        row["estimated_full_time_min"] = float(row["estimated_full_time_min"])
        row["estimated_output_gb"] = float(row["estimated_output_gb"])
        row["estimated_compression_ratio"] = float(row["estimated_compression_ratio"])
        row["steady_state_speed_mb_s"] = float(row["steady_state_speed_mb_s"])
        row["time_vs_adaptive"] = float(row["time_vs_adaptive"])
        row["size_vs_adaptive"] = float(row["size_vs_adaptive"])
    return rows


def build_combined(rows):
    methods = [row["method"] for row in rows]
    labels = [LABELS[m] for m in methods]
    colors = [COLORS[m] for m in methods]
    times = [row["estimated_full_time_min"] for row in rows]
    sizes = [row["estimated_output_gb"] for row in rows]

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.5), constrained_layout=True)

    ax = axes[0]
    bars = ax.bar(labels, times, color=colors)
    ax.set_title("Время перекладки")
    ax.set_ylabel("Оценка полного времени, мин")
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    for bar, value in zip(bars, times):
        ax.text(bar.get_x() + bar.get_width() / 2.0, value, f"{value:.2f}", ha="center", va="bottom", fontsize=10)

    ax = axes[1]
    bars = ax.bar(labels, sizes, color=colors)
    ax.set_title("Размер выходного файла")
    ax.set_ylabel("Оценка размера файла, GiB")
    ax.grid(axis="y", linestyle="--", alpha=0.35)
    for bar, value in zip(bars, sizes):
        ax.text(bar.get_x() + bar.get_width() / 2.0, value, f"{value:.2f}", ha="center", va="bottom", fontsize=10)

    fig.suptitle("Сравнение режимов строкового сжатия", fontsize=14)
    fig.savefig(OUTPUT_COMBINED, bbox_inches="tight")
    plt.close(fig)


def build_scatter(rows):
    fig, ax = plt.subplots(figsize=(7.5, 5.5), constrained_layout=True)
    for row in rows:
        method = row["method"]
        x = row["estimated_output_gb"]
        y = row["estimated_full_time_min"]
        ax.scatter(x, y, s=140, color=COLORS[method], label=LABELS[method])
        offset = (-70, 6) if method == "delta_length" else (8, 6)
        ax.annotate(
            LABELS[method],
            (x, y),
            textcoords="offset points",
            xytext=offset,
            fontsize=10,
        )

    ax.set_title("Компромисс между временем перекладки и размером файла")
    ax.set_xlabel("Оценка размера файла, GiB")
    ax.set_ylabel("Оценка полного времени, мин")
    ax.grid(linestyle="--", alpha=0.35)
    fig.savefig(OUTPUT_SCATTER, bbox_inches="tight")
    plt.close(fig)


def main():
    rows = load_rows()
    build_combined(rows)
    build_scatter(rows)
    print(f"Saved {OUTPUT_COMBINED}")
    print(f"Saved {OUTPUT_SCATTER}")


if __name__ == "__main__":
    main()
