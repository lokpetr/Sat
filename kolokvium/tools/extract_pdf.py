#!/usr/bin/env python3
"""
Извлекает текст из PDF учебника (Дымарский, "Лекции по математическому анализу")
постранично + строит индекс определений/теорем/лемм/следствий/замечаний/примеров
с номерами страниц и номеров в книге.

Использование:
    python3 extract_pdf.py <учебник.pdf> [--outdir build]

Результат в outdir:
    pages/page-0001.txt ...   — текст каждой страницы отдельно
    full.txt                  — весь текст подряд со маркерами страниц
    index.json                — индекс утверждений: {page, line, kind, num, title}
    index.md                  — человекочитаемый индекс
"""
import argparse
import json
import os
import re
import sys

try:
    import pymupdf  # noqa
except ImportError:  # старые сборки
    import fitz as pymupdf  # noqa


# Русские ключевые слова, с которых начинаются формулировки.
KINDS = [
    ("Теорема", "теорема"),
    ("Предложение", "предложение"),
    ("Лемма", "лемма"),
    ("Следствие", "следствие"),
    ("Утверждение", "утверждение"),
    ("Определение", "определение"),
    ("Замечание", "замечание"),
    ("Пример", "пример"),
    ("Обозначение", "обозначение"),
    ("Соглашение", "соглашение"),
    ("Доказательство", "доказательство"),
    ("Аксиома", "аксиома"),
]

# "Теорема 3.2." / "Теорема 12 (Вейерштрасса)." / "Лемма 5.1" / "Определение 7."
HEAD_RE = re.compile(
    r"^\s*(?P<kind>" + "|".join(k for k, _ in KINDS) + r")\s*"
    r"(?P<num>\d+(?:\.\d+)*)?\s*[.:)]?\s*(?P<rest>.*)$",
    re.IGNORECASE,
)


def extract(pdf_path: str, outdir: str) -> None:
    pages_dir = os.path.join(outdir, "pages")
    os.makedirs(pages_dir, exist_ok=True)

    doc = pymupdf.open(pdf_path)
    n = doc.page_count
    print(f"PDF: {pdf_path}")
    print(f"Страниц: {n}")
    meta = doc.metadata or {}
    if meta.get("title"):
        print(f"Title: {meta['title']}")
    print("-" * 60)

    full_parts = []
    index = []
    digits_only_pages = 0

    for i in range(n):
        page = doc.load_page(i)
        text = page.get_text("text")
        # Нормализуем переносы: склеиваем дефисные переносы слов
        text = re.sub(r"(\w)-\n(\w)", r"\1\2", text)
        # Одиночный перевод строки внутри абзаца -> пробел (грубо, но читаемо)
        lines = [ln.rstrip() for ln in text.split("\n")]

        with open(os.path.join(pages_dir, f"page-{i + 1:04d}.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(lines))

        full_parts.append(f"\n\n<<< PAGE {i + 1} >>>\n")
        full_parts.append("\n".join(lines))

        if len("".join(lines).strip()) < 15:
            digits_only_pages += 1

        for ln_no, ln in enumerate(lines, start=1):
            s = ln.strip()
            if not s or len(s) > 200:
                continue
            m = HEAD_RE.match(s)
            if not m:
                continue
            kind = next(v for k, v in KINDS if k.lower() == m.group("kind").lower())
            index.append(
                {
                    "page": i + 1,
                    "line": ln_no,
                    "kind": kind,
                    "num": (m.group("num") or "").strip(),
                    "text": s,
                }
            )

    with open(os.path.join(outdir, "full.txt"), "w", encoding="utf-8") as f:
        f.write("".join(full_parts))

    with open(os.path.join(outdir, "index.json"), "w", encoding="utf-8") as f:
        json.dump(index, f, ensure_ascii=False, indent=1)

    with open(os.path.join(outdir, "index.md"), "w", encoding="utf-8") as f:
        f.write("# Индекс утверждений\n\n")
        for it in index:
            f.write(f"- с. {it['page']:>3} | **{it['kind']} {it['num']}** — {it['text'][:160]}\n")

    print(f"Страниц без текстового слоя (похоже на сканы): {digits_only_pages} из {n}")
    if digits_only_pages > n * 0.5:
        print("!! PDF, похоже, СКАН без текстового слоя -> потребуется OCR (tesseract/paddleocr).")
    print(f"Найдено утверждений в индексе: {len(index)}")
    print(f"Результаты: {outdir}/pages/, {outdir}/full.txt, {outdir}/index.md")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pdf")
    ap.add_argument("--outdir", default="build")
    args = ap.parse_args()
    extract(args.pdf, args.outdir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
