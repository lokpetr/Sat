# ---------------------------------------------------------------------------
# Лаба по флоатам: собираем все пункты одной командой.
#
#   make            -- собрать всё в build/
#   make run        -- собрать и прогнать все пункты, сложив вывод в results/
#   make probe      -- мини-пробник -ffast-math (results/fastmath_probe.txt)
#   make plots      -- построить графики по CSV из results/ (нужен python3+matplotlib)
#   make fastmath   -- собрать тот же код с -ffast-math (см. разбор в README)
#   make clean
# ---------------------------------------------------------------------------
SHELL    := /bin/bash

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-result
LDLIBS   ?= -lm
PYTHON   ?= python3
NMAX     ?= 27          # степень двойки: сколько прямоугольников максимум считаем в пункте 8

SRCS := $(sort $(wildcard src/task*.cpp))
BINS := $(patsubst src/%.cpp,build/%,$(SRCS))

TASKS := $(patsubst build/%,%,$(BINS))

.PHONY: all run plots fastmath fastmath-diff fma-diff probe clean

all: $(BINS)

build:
	@mkdir -p build results

build/%: src/%.cpp src/floatbits.h src/decimal.h src/pow_table.h | build
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)

# Сравнение "тот же код, другой флаг": обычный -O2 против -ffast-math.
# Результат -- results/fastmath_vs_O2.txt, разбор в README (пункт "рецепты устаревают").
.PHONY: fastmath-diff
fastmath-diff: all fastmath
	@mkdir -p results
	@{ echo "# diff выводов, собранных с -O2 и с -O2 -ffast-math (тот же самый код!)"; \
	  for t in $(TASKS); do \
	    echo; echo "#########  $$t  #########"; \
	    if [ "$$t" = "task8_integration" ]; then mkdir -p results results-fm; a="results 27"; b="results-fm 27"; \
	      else a=""; b=""; fi; \
	    diff <(./build/$$t $$a 2>&1) <(./build-fm/$$t $$b 2>&1) | head -40; \
	  done; } > results/fastmath_vs_O2.txt
	@rm -rf results-fm
	@echo "готово: results/fastmath_vs_O2.txt ($$(wc -l < results/fastmath_vs_O2.txt) строк)"

# -ffast-math намеренно "ломает" IEEE 754-ради скорости: наглядная проверка
# того, что рецепты зависят от флагов компиляции.
fastmath:
	@mkdir -p build-fm
	@for s in $(SRCS); do \
	  b=build-fm/$$(basename $$s .cpp); \
	  echo "  CXX   $$b (-ffast-math)"; \
	  $(CXX) -std=c++17 -O2 -ffast-math -fno-math-errno $$s -o $$b $(LDLIBS) || exit 1; \
	done

# Тот же код, но с -march=native: GCC по умолчанию включает -ffp-contract=fast,
# т.е. склеивает a*b+c в одну FMA-инструкцию без промежуточного округления.
# Ответ меняется в последних битах -- вот вам и "бит-в-бит" воспроизводимость,
# которая ломается БЕЗ всякого -ffast-math, одним только -march.
.PHONY: fma-diff
fma-diff: all
	@mkdir -p build-fma
	@for f in fast off; do \
	  echo "  CXX   build-fma/task8_$$f (-march=native -ffp-contract=$$f)"; \
	  $(CXX) -std=c++17 -O2 -march=native -ffp-contract=$$f src/task8_integration.cpp \
	    -o build-fma/task8_$$f $(LDLIBS) || exit 1; \
	done
	@mkdir -p build-fma/a build-fma/b
	@./build-fma/task8_fast build-fma/a 20 > build-fma/fast.txt 2>&1
	@./build-fma/task8_off  build-fma/b 20 > build-fma/off.txt  2>&1
	@{ echo "# один и тот же код, -O2 -march=native, разница только в -ffp-contract"; \
	   diff build-fma/fast.txt build-fma/off.txt || true; } > results/fma_vs_nofma.txt
	@echo "готово: results/fma_vs_nofma.txt"

# run: каждый пункт пишет вывод в results/taskN.txt и печатает первые строки.
# Пункт 8 дополнительно сыплет CSV, которые нужны для графиков.
run: all
	@mkdir -p results
	@for t in $(TASKS); do \
	  echo "===================  $$t  ==================="; \
	  if [ "$$t" = "task8_integration" ]; then args="results $(NMAX)"; else args=""; fi; \
	  ./build/$$t $$args > results/$$t.txt 2>&1; \
	  head -n 60 results/$$t.txt; \
	  echo "   ... (полный вывод: results/$$t.txt)"; \
	  echo; \
	done
	@echo "вывод всех пунктов лежит в results/*.txt"

plots: all
	@mkdir -p results
	./build/task8_integration results $(NMAX) > results/task8_integration.txt
	$(PYTHON) plots.py results plots
	@echo "графики: plots/*.png"

# Мини-пробник "что именно ломает -ffast-math": один и тот же src/fastmath_probe.cpp
# собирается дважды, выводы сравниваются.  ->  results/fastmath_probe.txt
.PHONY: probe
probe:
	@mkdir -p build build-fm results
	$(CXX) $(CXXFLAGS) src/fastmath_probe.cpp -o build/fastmath_probe $(LDLIBS)
	$(CXX) -std=c++17 -O2 -ffast-math -fno-math-errno src/fastmath_probe.cpp -o build-fm/fastmath_probe $(LDLIBS)
	@{ echo "# один и тот же код, -O2 (слева) против -O2 -ffast-math (справа)"; \
	  echo "#   0 -- это 'не бесконечность' / 'не NaN' / 'не число', т.е. проверка соврала"; \
	  diff <(./build/fastmath_probe 2>&1) <(./build-fm/fastmath_probe 2>&1) | grep -v "^[0-9]" ; } > results/fastmath_probe.txt
	@echo "готово: results/fastmath_probe.txt"

clean:
	rm -rf build build-fm build-fma results plots
