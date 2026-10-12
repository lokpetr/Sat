// ---------------------------------------------------------------------------
// Пункт 5. Бесконечный цикл из-за дискретного шага float.
// Считаем counter'ом float с шагом 1.0f: в какой-то момент 1.0f становится
// меньше, чем расстояние до следующего представимого float, и переменная
// перестаёт меняться вообще.
//
// Программа НЕ зависает насовсем: она сама замечает, что x перестал расти,
// печатает "я застрял на ..." и выходит. Уберите детектор -- и получите
// честно висящий процесс (именно это и надо показать на защите).
// ---------------------------------------------------------------------------
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

#include "floatbits.h"
#include "pow_table.h"  // переиспользуем my_pow: 2^24 = 16777216

using clock_ = std::chrono::steady_clock;

int main(int argc, char **argv) {
    // Режим "честно зависнуть" для демонстрации на защите: ./build/task5_infinite_loop --hang
    const bool really_hang = (argc > 1 && std::string(argv[1]) == "--hang");
    std::cout << "=== 5. Бесконечный цикл на float-счётчике ===\n";
    if (really_hang)
        std::cout << "   (режим --hang: детектор выключен, процесс будет висеть вечно, Ctrl-C)\n";
    std::cout << "\n";

    // ------------------------------------------------------------------
    // А. Сначала предскажем точку аварии по шагу сетки (ulp), ничего не считая.
    // ------------------------------------------------------------------
    std::cout << "расстояние между соседними float (ulp) вокруг степеней двойки:\n";
    std::cout << "     k        float 2^k          ulp = шаг сетки      1.0f < ulp ?\n";
    for (int k = 18; k <= 26; ++k) {
        const float v = std::ldexp(1.0f, k);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.6g", static_cast<double>(ulp32(v)));
        std::cout << "   2^" << std::setw(2) << std::left << k << std::right << "  " << std::setw(18)
                  << std::fixed << std::setprecision(1) << v << "          " << std::setw(8) << buf
                  << "            " << (1.0f < ulp32(v) ? "ДА -> шаг теряется" : "нет") << "\n";
    }
    std::cout << "\n  при x >= 2^24 = " << my_pow(2, 24)
              << " шаг сетки уже 2, то есть +1.0f физически некуда применить:\n"
              << "  x + 1.0f округляется обратно к x. Значит цикл вида\n"
              << "      for (float x = 0; x < 1e30f; x += 1.0f) { ... }\n"
              << "  застрянет насовсем (1e30 -- это сильно больше 2^24, предела не дождёмся).\n\n";

    // ------------------------------------------------------------------
    // А2. То же самое для double и long double. long double на x86-64 -- это ровно
    //     80-битный x87-формат (1 + 15 + 64), т.е. то, на чём по умолчанию считают
    //     32-битные сборки: "граница застревания" уезжает на 2^64.
    // ------------------------------------------------------------------
    {
        auto probe = [](auto tag) {
            using TT = decltype(tag);
            const TT one = TT(1);
            int k_stall = -1;
            for (int k = 1; k <= std::numeric_limits<TT>::digits + 2; ++k) {
                const TT v = std::ldexp(one, k);
                const TT nxt = std::nextafter(v, std::numeric_limits<TT>::infinity());
                if (nxt - v > one) {            // шаг 1 уже не помещается в сетку
                    k_stall = k;
                    break;
                }
            }
            return k_stall;
        };
        using F_ = float;
        using D_ = double;
        using LD_ = long double;
        const int kf = probe(F_{});
        const int kd = probe(D_{});
        const int kl = probe(LD_{});
        std::cout << "\n  где происходит то же самое, только с другим форматом (критерий тот же:\n"
                     "  шаг 1.0 перестаёт помещаться между соседними числами):\n"
                  << "  формат        значащих бит   1.0 теряется с   max<T>          comment\n"
                  << "  float          " << std::setw(6) << std::numeric_limits<float>::digits
                  << "         2^" << kf << "       " << std::scientific << std::setprecision(3)
                  << std::numeric_limits<float>::max() << "   SSE2, -O2 по умолчанию\n"
                  << "  double         " << std::setw(6) << std::numeric_limits<double>::digits
                  << "         2^" << kd << "       " << std::numeric_limits<double>::max()
                  << "   SSE2\n"
                  << "  long double    " << std::setw(6) << std::numeric_limits<long double>::digits
                  << "         2^" << kl << "       " << std::numeric_limits<long double>::max()
                  << "  80-битный x87 = то, на чём считают\n"
                  << "                                             без -msse2 (32-битные\n"
                  << "                                             сборки, -m32)\n"
                  << "  (до 2^64 досчитывать, конечно, никто не стал: это ~360 лет на этой машине при\n"
                  << "   836 млн итераций/с; здесь границу даёт то же сравнение ulp, что и выше)\n";
    }

    // ------------------------------------------------------------------
    // Б. Эксперимент: реально идём по кругу и ловим момент, когда застряли.
    // ------------------------------------------------------------------
    const float LIMIT = 1e30f;  // "очень большое число" из условия лабы
    float x = 0.0f;
    uint64_t it = 0;
    uint64_t stall = 0;
    const auto t0 = clock_::now();
    auto tPrev = t0;
    uint64_t itPrev = 0;

    std::cout << "  #iter        x          x+1.0f (то же?)     скорость, млн ит/с   время\n";
    while (true) {
        const float before = x;
        x += 1.0f;
        ++it;

        if (x == before && !really_hang) {  // вот она, потеря шага в погрешности
            if (++stall > (1u << 22)) break;  // "зависли" -- убеждаемся, что это навсегда
        } else {
            stall = 0;
        }

        if ((it & ((1u << 22) - 1)) == 0) {
            const auto now = clock_::now();
            const double dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - tPrev).count();
            const double rate = (it - itPrev) / dt / 1e6;
            const double total = std::chrono::duration_cast<std::chrono::duration<double>>(now - t0).count();
            std::cout << std::setw(11) << it << "  " << std::setw(18) << std::fixed
                      << std::setprecision(1) << x << "   " << (x + 1.0f == x ? "ДА, не меняется!" : "нет")
                      << std::setw(16) << std::setprecision(1) << rate << std::setw(14)
                      << std::setprecision(2) << total << " s\n";
            tPrev = now;
            itPrev = it;
        }
    }

    const double total = std::chrono::duration_cast<std::chrono::duration<double>>(clock_::now() - t0).count();
    std::cout << "\n  >>> ЦИКЛ ЗАСТРЯЛ на x = " << std::setprecision(9) << std::fixed << x << "  (= 2^"
              << static_cast<int>(std::log2(static_cast<double>(x))) << ")  после " << it
              << " итераций за " << std::setprecision(4) << total << " с\n";
    std::cout << "      следующий float после x : " << float_from_bits(bits_of(x) + 1u)
              << "  (расстояние " << ulp32(x) << ", наш шаг 1.0f в 2 раза меньше)\n";
    std::cout << "      x + 1.0f = " << std::setprecision(9) << std::fixed << x + 1.0f
              << "  -- то же самое число, поэтому условие\n      x < " << std::scientific << LIMIT
              << " никогда не станет ложным.\n";

    // Экстраполяция для double -- чтобы было понятно, почему на double "как бы работает".
    const double rate = static_cast<double>(it) / total;
    const double need_iters = std::ldexp(1.0, 53);  // 2^53 для double
    std::cout << "\n  то же самое на double застряло бы на 2^53 = " << std::fixed << std::setprecision(0)
              << need_iters << ",\n      при измеренной скорости " << std::setprecision(1) << rate / 1e6
              << " млн итераций/с это ~" << std::setprecision(1) << need_iters / rate / 86400.0
              << " суток непрерывной работы\n      (и это БЕЗ учёта тела цикла). Вывод: 'перейти на"
                 " double' -- не исправление бага,\n      а просто отсрочка аварии на месяц-другой."
                 "\n      На 80-битных x87-регистрах (старые 32-битные сборки) граница ещё дальше:\n"
              << "      2^64, то есть цикл 'работает' -- и это как раз пример рецепта, который"
                 "\n      перестаёт работать, когда меняешь компилятор/архитектуру.\n";

    std::cout << "\n  как чинят: счётчик целочисленный (for (uint64_t i = 0; i < n; ++i) { x = ... }),\n"
                 "  либо шаг, заведомо больший ulp, либо сравнение не с пределом, а по числу шагов.\n";
    return 0;
}
