// ---------------------------------------------------------------------------
// Пункт 6. Накопление ошибки при суммировании.
// Суммируем массив вещественных чисел так, чтобы ответ отличался от ожидаемого.
// ---------------------------------------------------------------------------
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

#include "decimal.h"
#include "floatbits.h"

// Суммирование "в лоб", аккумулятор типа Acc.
template <typename Acc, typename T>
static Acc naive_sum(const std::vector<T> &a, Acc start = Acc(0)) {
    Acc s = start;
    for (T x : a) s += x;
    return s;
}

// Компенсированное суммирование Канаана -- "рецепт" борьбы с накоплением ошибки.
template <typename Acc, typename T>
static Acc kahan_sum(const std::vector<T> &a) {
    Acc sum = Acc(0), c = Acc(0);  // c -- накопленная "потерянная" часть
    for (T x : a) {
        const Acc y = static_cast<Acc>(x) - c;
        const Acc t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum;
}

int main() {
    std::cout << "=== 6. Накопление ошибки при суммировании ===\n";

    // ------------------------------------------------------------------
    // 6.1. Классика: десять слагаемых по 0.1.
    // ------------------------------------------------------------------
    std::cout << "\n--- 6.1. десять раз по 0.1 ---\n";
    {
        float sf = 0.0f;
        double sd = 0.0;
        for (int i = 0; i < 10; ++i) {
            sf += 0.1f;
            sd += 0.1;
        }
        std::cout << "  float : сумма = " << std::setprecision(9) << std::fixed << sf
                  << "  (ожидалось 1.000000000)\n";
        std::cout << "          ровно в памяти: " << dec::exact_of(sf) << "\n";
        std::cout << "  double: сумма = " << std::setprecision(17) << sd
                  << "  (ожидалось 1.00000000000000000)\n";
        std::cout << "          ровно в памяти: " << dec::exact_of(sd) << "\n";
        std::cout << "  и то, и другое НЕ равно 1.0, то есть `сумма == 1.0` уже ложно:  float -> "
                  << (sf == 1.0f) << ",  double -> " << (sd == 1.0) << "\n";
        std::cout << "  причина видна из битов: 0.1f = " << dec::exact_of(0.1f) << "\n"
                  << "    в двоичной системе 0.1 = 0.0001100110011...(период), мантисса обрывается,\n"
                  << "    и ошибка 1.49e-9 на слагаемое превращается в ошибку 1.49e-8 на десятке.\n";
    }

    // ------------------------------------------------------------------
    // 6.2. Массив из 10 000 000 элементов по 0.1f.
    // Разделяем два разных эффекта: ошибку ПРЕДСТАВЛЕНИЯ (её не убрать ничем)
    // и ошибку НАКОПЛЕНИЯ (вот её как раз и убирают Kahan/параллельные схемы).
    // ------------------------------------------------------------------
    std::cout << "\n--- 6.2. 10 000 000 штук по 0.1f (N = 1e7) ---\n";
    const uint64_t N = 10000000ull;
    const std::vector<float> a(N, 0.1f);

    // 0.1f = 13421773 * 2^-27 (ровно!), поэтому ТОЧНАЯ сумма N таких слагаемых
    // -- это целое 13421773*N, поделённое на 2^27. Ни капли погрешности в эталоне.
    const double exact_stored = std::ldexp(static_cast<double>(13421773ull * N), -27);
    const double expected_decimal = 0.1 * static_cast<double>(N);  // "ожидаемое" из десятичных

    const float naive = naive_sum<float>(a);
    const float kahan = kahan_sum<float>(a);
    const double in_double = naive_sum<double>(a);

    std::cout << std::fixed << std::setprecision(9);
    std::cout << "  'ожидалось' (0.1 * N)          : " << std::setprecision(17) << expected_decimal << "\n";
    std::cout << "  точная сумма тех же float'ов   : " << exact_stored << "   <- наш эталон\n";
    std::cout << "  наивная сумма, float            : " << naive << "   ошибка " << std::setprecision(6)
              << std::scientific << (static_cast<double>(naive) - exact_stored) << "\n";
    std::cout << "  та же наивная сумма, double     : " << std::fixed << std::setprecision(9) << in_double
              << "   ошибка " << std::scientific << (in_double - exact_stored) << "\n";
    std::cout << "  сумма Канаана, float            : " << std::fixed << std::setprecision(9) << kahan
              << "   ошибка " << std::scientific << (static_cast<double>(kahan) - exact_stored) << "\n";

    // Когда именно наивный float-аккумулятор перестаёт видеть слагаемое?
    uint64_t absorb_at = 0;
    for (int k = 0; k < 40; ++k) {
        const float v = std::ldexp(1.0f, k);
        if (v + 0.1f == v) {
            absorb_at = 1ull << k;
            break;
        }
    }
    std::cout << "  >>> после того как сумма превышает " << absorb_at << " = 2^"
              << static_cast<int>(std::log2(static_cast<double>(absorb_at)))
              << ", шаг сетки float уже " << ulp32(static_cast<float>(static_cast<double>(absorb_at)))
              << " >= 2*0.1, и прибавление 0.1 начинает округляться до целого шага сетки:\n"
              << "      наивный ответ больше точного на " << std::setprecision(6)
              << (static_cast<double>(naive) - exact_stored) << ", это ~" << std::setprecision(1)
              << std::fixed << ((static_cast<double>(naive) - exact_stored) / 0.1)
              << " слагаемых, которых там не было -- сетка float просто округляла\n"
              << "      каждое прибавление 0.1 до ближайшего узла (0 или 0.25 или 0.5).\n";

    // ------------------------------------------------------------------
    // 6.3. "Абсорбция": одно большое число + много маленьких.
    // ------------------------------------------------------------------
    std::cout << "\n--- 6.3. 1e7 плюс 10 000 000 штук по 0.1f ---\n";
    {
        const float big = 1.0e7f;
        float s = big;
        for (uint64_t i = 0; i < N; ++i) s += 0.1f;
        std::cout << std::fixed << std::setprecision(9);
        std::cout << "  получилось                     : " << s << "\n";
        std::cout << "  ожидалось (1e7 + 1e7*0.1)       : " << (static_cast<double>(big) + expected_decimal)
                  << "\n";
        std::cout << "  ulp(1e7f) = " << ulp32(big)
                  << " -- шаг сетки больше, чем всё, что мы прибавляем\n";
        std::cout << "  >>> добавленная часть потеряна на " << std::setprecision(3)
                  << (1.0 - (static_cast<double>(s) - big) / expected_decimal) * 100.0 << "%\n";
    }

    // ------------------------------------------------------------------
    // 6.4. Catastrophic cancellation: ошибка не в накоплении, а в вычитании
    // двух почти равных чисел. Классика -- квадратное уравнение.
    // ------------------------------------------------------------------
    std::cout << "\n--- 6.4. вычитание близких чисел: x^2 - 1e8*x + 1 = 0 ---\n";
    {
        const float b = 1.0e8f, c = 1.0f;
        const float disc = std::sqrt(b * b - 4.0f * c);
        const float x1_naive = (b - disc) / 2.0f;              // "малый" корень в лоб
        const float x1_stable = 2.0f * c / (b + disc);         // домножили на сопряжённое
        std::cout << std::setprecision(9);
        std::cout << "  b*b - 4c            = " << std::scientific << (b * b - 4.0f * c)
                  << "   <- b*b и 4c различаются на 8 порядков, четвёрки просто не видно\n";
        std::cout << "  (b - sqrt(D))/2     = " << x1_naive
                  << "   <- НОЛЬ: все значащие цифры съедены вычитанием\n";
        std::cout << "  2c / (b + sqrt(D))  = " << x1_stable
                  << "   <- верный корень (аналитически 1.0000000e-08)\n";
        const float a = 1.0e8f;
        std::cout << "  sqrt(a+1) - sqrt(a) = " << (std::sqrt(a + 1.0f) - std::sqrt(a))
                  << "   vs   1/(sqrt(a+1)+sqrt(a)) = " << (1.0f / (std::sqrt(a + 1.0f) + std::sqrt(a)))
                  << "  (аналитически 5.0000000e-05)\n";
        std::cout << "  >>> вычитание близких чисел превращает ошибку последнего разряда в ошибку"
                     "\n      ПЕРВОГО значащего: это самый дорогой из всех показанных эффектов.\n";
    }
    return 0;
}
