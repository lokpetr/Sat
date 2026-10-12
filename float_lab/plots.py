#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Графики для лабы по флоатам (пункт 8).

Читает CSV, которые насчитал ./build/task8_integration, и рисует:
  fig1_functions.png   -- сама функция и её первообразная (заштрихованная площадь)
  fig2_area_vs_n.png   -- зависимость значения площади от числа прямоугольников
  fig3_error_vs_n.png  -- лог-лог график |ошибка| vs N, с отмеченным оптимумом

Использование:  python3 plots.py [каталог_с_CSV] [куда_класть_PNG]
Зависимости: matplotlib (+ numpy). Если его нет -- см. README, там же есть
текстовая версия графика (results/ascii_*.txt), которую рисует сам этот скрипт.
"""
from __future__ import annotations

import csv
import os
import sys

FUNCS = [
    ("line", "y = x"),
    ("cubic", "y = x^3 + x"),
    ("circle", "y = sqrt(1 - x^2)"),
    ("pi", "y = 4/(1 + x^2)"),
]

EPS_F = 1.1920928955078125e-07   # float epsilon
EPS_D = 2.220446049250313e-16    # double epsilon


def read_csv(path):
    rows = []
    with open(path, newline="") as fh:
        rdr = csv.reader(fh)
        header = next(rdr)
        for r in rdr:
            if not r:
                continue
            rows.append([float(x) for x in r])
    return header, rows


def ascii_plot(xs, ys, width=64, height=14, xlog=True, ylog=True, label=""):
    """Запасной вариант на случай, если matplotlib недоступен: тот же график в тексте."""
    import math

    def tr(v, log):
        return math.log10(v) if (log and v > 0) else v

    pts = [(tr(x, xlog), tr(y, ylog)) for x, y in zip(xs, ys) if x > 0 and y > 0]
    if not pts:
        return ""
    x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
    y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    if x1 == x0:
        x1 = x0 + 1
    if y1 == y0:
        y1 = y0 + 1
    grid = [[" "] * width for _ in range(height)]
    for x, y in pts:
        c = int((x - x0) / (x1 - x0) * (width - 1))
        r = height - 1 - int((y - y0) / (y1 - y0) * (height - 1))
        grid[r][c] = "*"
    lines = []
    for i, row in enumerate(grid):
        val = 10 ** (y1 - (y1 - y0) * i / (height - 1)) if ylog else (y1 - (y1 - y0) * i / (height - 1))
        lines.append(f"{val:>11.3g} |" + "".join(row))
    lo_x = 10 ** x0 if xlog else x0
    hi_x = 10 ** x1 if xlog else x1
    lines.append(" " * 12 + "+" + "-" * width)
    lines.append(f"{'':12} {lo_x:<{width - 8}.4g} {'N ->':^{8}} {hi_x:>8.4g}")
    if label:
        lines.append(f"{'':12} {label}")
    return "\n".join(lines)


def main():
    res = sys.argv[1] if len(sys.argv) > 1 else "results"
    out = sys.argv[2] if len(sys.argv) > 2 else "plots"
    os.makedirs(out, exist_ok=True)

    conv = {}
    curve = {}
    for slug, _ in FUNCS:
        cp = os.path.join(res, f"conv_{slug}.csv")
        kp = os.path.join(res, f"curve_{slug}.csv")
        if os.path.exists(cp):
            conv[slug] = read_csv(cp)[1]
        if os.path.exists(kp):
            curve[slug] = read_csv(kp)[1]

    if not conv:
        print(f"нет CSV в {res}/ -- сначала: make run (или ./build/task8_integration {res} 26)")
        return 1

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib не найден -- делаю текстовые версии графиков (ascii_*.txt)")
        for slug, title in FUNCS:
            if slug not in conv:
                continue
            rows = conv[slug]
            n = [r[0] for r in rows]
            with open(os.path.join(out, f"ascii_err_{slug}.txt"), "w") as fh:
                fh.write(title + " | |ошибка float| и |ошибка double| vs N (log-log)\n")
                fh.write(ascii_plot(n, [abs(r[2] - r[9]) for r in rows], label="float") + "\n")
                fh.write(ascii_plot(n, [abs(r[6] - r[9]) for r in rows], label="double") + "\n")
        print("готово: " + out + "/ascii_err_*.txt")
        return 0

    plt.rcParams.update({"figure.dpi": 110, "font.size": 9, "axes.grid": True,
                         "grid.alpha": .3, "axes.titlesize": 10})

    # ------------------------------------------------------------------ fig 1
    fig, axes = plt.subplots(2, 2, figsize=(9.5, 6.4))
    for ax, (slug, title) in zip(axes.ravel(), FUNCS):
        if slug not in curve:
            ax.axis("off")
            continue
        rows = curve[slug]
        x = [r[1] for r in rows]
        f = [r[4] for r in rows]
        F = [r[5] for r in rows]
        ax.plot(x, f, lw=2, label=f"f(x) = {title}")
        ax.plot(x, F, lw=2, ls="--", color="tab:orange", label="F(x) — первообразная, F(0)=0")
        ax.fill_between(x, 0, f, alpha=.18, label="площадь под f (то, что считаем)")
        ax.axhline(F[-1], color="k", lw=.6, alpha=.5)
        ax.set_title(f"{title},  ∫₀¹ = {F[-1]:.6f}")
        ax.set_xlabel("x")
        ax.legend(fontsize=7, loc="upper left")
    fig.suptitle("Пункт 8: функция, её первообразная (F(0)=0) и площадь под графиком", y=0.995)
    fig.tight_layout()
    fig.savefig(os.path.join(out, "fig1_functions.png"))
    plt.close(fig)

    # ------------------------------------------------------------------ fig 2
    fig, axes = plt.subplots(2, 2, figsize=(10.5, 6.6))
    for ax, (slug, title) in zip(axes.ravel(), FUNCS):
        if slug not in conv:
            ax.axis("off")
            continue
        rows = conv[slug]
        n = [r[0] for r in rows]
        fm = [r[2] for r in rows]
        fl = [r[1] for r in rows]
        dm = [r[6] for r in rows]
        ex = rows[0][9]
        ax.plot(n, fm, "o-", ms=3, lw=1.3, label="float, средние прямоугольники")
        ax.plot(n, fl, "x-", ms=3, lw=1, alpha=.7, label="float, левые прямоугольники")
        ax.plot(n, dm, "s-", ms=3, lw=1.3, mfc="none", color="tab:green",
                label="double, средние (на этом масштабе -- прямая)")
        ax.axhline(ex, color="k", ls=":", lw=1.4, label=f"точное значение = {ex:.6g}")
        ax.set_xscale("log", base=2)
        ax.set_xlim(min(n) * 0.7, max(n) * 1.5)
        ax.set_xlabel("число прямоугольников N")
        ax.set_ylabel("значение интеграла")
        ax.set_title(title, fontsize=9.5)
    fig.suptitle("Пункт 8: зависимость значения площади от числа прямоугольников. "
                 "float со временем УХОДИТ В ШУМ, double стоит на месте", y=0.995)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=4, fontsize=7.5, frameon=True)
    fig.tight_layout(rect=(0, 0.045, 1, 0.965))
    fig.savefig(os.path.join(out, "fig2_area_vs_n.png"))
    plt.close(fig)

    # ------------------------------------------------------------------ fig 3
    fig, axes = plt.subplots(2, 2, figsize=(12.6, 8.4))
    for ax, (slug, title) in zip(axes.ravel(), FUNCS):
        if slug not in conv:
            ax.axis("off")
            continue
        rows = conv[slug]
        n = [r[0] for r in rows]
        FLOOR = 1e-17  # нулевую ошибку на лог-шкале не нарисовать, поднимаем до "пола"

        def errs(col):
            raw = [abs(r[col] - r[9]) for r in rows]
            return raw, [max(v, FLOOR) for v in raw]

        ef_raw, ef = errs(2)
        ed_raw, ed = errs(6)
        efk_raw, efk = errs(4)

        ax.plot(n, ef, "o-", ms=3.5, lw=1.3, color="tab:blue", label="float, наивная сумма")
        ax.plot(n, efk, "^--", ms=4, lw=1, color="tab:red", alpha=.8, label="float + Kahan")
        ax.plot(n, ed, "s-", ms=3.5, lw=1.3, color="tab:green", label="double, наивная сумма")
        if ef[0] > 0:  # для y=x метод средних точен аналитически -- рисовать наклон нечего
            ax.plot(n, [ef[0] * (n[0] / nn) ** 2 for nn in n], color="gray", ls="--", lw=.9,
                    label="наклон O(1/N²): 'методическая' часть ошибки")
        ax.axhline(EPS_F, color="tab:blue", ls=":", lw=1, label="eps_float")
        ax.axhline(EPS_D, color="tab:green", ls=":", lw=1, label="eps_double")

        def opt(arr):
            # ровно та же логика, что в C++-программе: оптимум = НАИМЕНЬШЕЕ N,
            # достигшее минимума (строгий <), а "портиться начинает" = первое N,
            # где ошибка превзошла ТЕКУЩИЙ (на тот момент) минимум
            best, bi, bad = float("inf"), -1, None
            for k, v in enumerate(arr):
                if v < best:
                    best, bi = v, k
                elif v > best and bad is None:
                    bad = n[k]
            return n[bi], best, bad

        nf, ef_min, nf_bad = opt(ef_raw)
        nd, ed_min, nd_bad = opt(ed_raw)
        ax.plot(nf, max(ef_min, FLOOR), "o", ms=13, mfc="none", mew=1.8, color="tab:blue")
        ax.plot(nd, max(ed_min, FLOOR), "s", ms=13, mfc="none", mew=1.8, color="tab:green")
        fmt = lambda e: "0" if e == 0 else f"{e:.3e}"
        bad_f = f"N={int(nf_bad)}" if nf_bad else "нет (в пределе теста)"
        bad_d = f"N={int(nd_bad)}" if nd_bad else "нет (в пределе теста)"
        ax.set_title(
            f"{title}\nfloat : оптимум N={int(nf)}, err={fmt(ef_min)}, портиться нач. с {bad_f}\n"
            f"double: оптимум N={int(nd)}, err={fmt(ed_min)}, портиться нач. с {bad_d}",
            fontsize=7.8, linespacing=1.35)
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlim(min(n) * 0.7, max(n) * 1.5)
        ax.set_ylim(min(EPS_D * 0.03, FLOOR * 0.3), max(max(ef), max(ed), max(efk)) * 3)
        ax.set_xlabel("число прямоугольников N (log)")
        ax.set_ylabel("|ошибка| (log)")
    fig.suptitle("Пункт 8: |ошибка| vs N. У float -- 'буква U', дно на N~1e3..1e6 (дальше только хуже);\n"
                 "у double дно на 3-4 порядка позже. Кружок/квадрат -- точка оптимума.", y=0.998)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center", ncol=6, fontsize=7.5, frameon=True)
    fig.tight_layout(rect=(0, 0.04, 1, 0.955))
    fig.savefig(os.path.join(out, "fig3_error_vs_n.png"))
    plt.close(fig)

    print("графики лежат в", out + ":")
    for f in sorted(os.listdir(out)):
        print("  ", os.path.join(out, f))
    return 0


if __name__ == "__main__":
    sys.exit(main())
