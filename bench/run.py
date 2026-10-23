"""Прогоняет бенчмарк на всех размерах поля и уровнях бота.

Пример:
    python bench/run.py --build build-release --label v0

Результаты в bench/results/<label>/:
    moves.csv   - время каждого хода (медиана по повторам)
    summary.csv - среднее время хода для каждого размера и уровня + контрольная сумма ходов
"""
import argparse
import csv
import re
import subprocess
from pathlib import Path

# уровень -> (число партий, имя уровня); у лёгкого бота партии длинные, их меньше
LEVELS = {3: (20, "hard"), 2: (20, "medium"), 1: (3, "easy")}

SUMMARY_RE = re.compile(r"moves=(\d+) total=([\d.]+) ms avg=([\d.]+) us/move.*checksum=([0-9a-f]+)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build-release", help="папка сборки с bench_*.exe")
    ap.add_argument("--label", required=True, help="метка версии, например v0 или v1")
    ap.add_argument("--reps", type=int, default=5, help="повторов каждой серии партий")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--levels", default="3,2,1", help="уровни через запятую")
    args = ap.parse_args()

    root = Path(__file__).resolve().parent
    build = Path(args.build).resolve()
    out = root / "results" / args.label
    out.mkdir(parents=True, exist_ok=True)

    exes = sorted(build.glob("bench_*.exe"), key=lambda p: int(p.stem.split("_")[1]))
    if not exes:
        raise SystemExit(f"В {build} нет bench_*.exe, сначала соберите проект")

    levels = [int(x) for x in args.levels.split(",")]
    header = None
    with open(out / "moves.csv", "w", newline="") as fm, open(out / "summary.csv", "w", newline="") as fs:
        moves_w = csv.writer(fm)
        summ_w = csv.writer(fs)
        summ_w.writerow(["label", "size", "level", "games", "moves", "total_ms", "avg_us", "checksum"])

        for exe in exes:
            size = int(exe.stem.split("_")[1])
            for level in levels:
                games, name = LEVELS[level]
                cmd = [str(exe), str(games), str(level), str(level), str(args.seed), str(args.reps)]
                res = subprocess.run(cmd, capture_output=True, text=True, check=True)

                lines = res.stdout.strip().splitlines()
                if header is None:
                    header = ["label"] + lines[0].split(",")
                    moves_w.writerow(header)
                for line in lines[1:]:
                    moves_w.writerow([args.label] + line.split(","))

                m = SUMMARY_RE.search(res.stderr)
                n_moves, total_ms, avg_us, checksum = m.group(1), m.group(2), m.group(3), m.group(4)
                summ_w.writerow([args.label, size, level, games, n_moves, total_ms, avg_us, checksum])
                fs.flush()
                print(f"{args.label}  SIZE={size:<4} {name:<6} moves={n_moves:<6} "
                      f"avg={float(avg_us):>10.2f} us/move  checksum={checksum}")


if __name__ == "__main__":
    main()
