// ---------------------------------------------------------------------------
// Пункт 7. Неассоциативность сложения: (a+b)+c != a+(b+c).
// Тут есть два сюжета:
//   7.1 -- найти конкретный контрпример (перебором, чтобы не угадывать);
//   7.2 -- показать, что ОТ ПОРЯДКА ОПЕРАЦИЙ зависит результат целого массива,
//          включая "параллельные" схемы (GPU, OpenMP reduction) -- то есть
//          один и тот же код на разном числе потоков даёт разные биты.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include "decimal.h"
#include "floatbits.h"

using u64 = unsigned long long int;

// ------------------------------------------------------------------
// Суммы в разном порядке. Все -- одна и та же формула, разные скобки.
// ------------------------------------------------------------------
template <typename T>
static T sum_forward(const std::vector<T> &a) {
    T s = 0;
    for (T x : a) s += x;
    return s;
}

template <typename T>
static T sum_backward(const std::vector<T> &a) {
    T s = 0;
    for (auto it = a.rbegin(); it != a.rend(); ++it) s += *it;
    return s;
}

template <typename T>
static T sum_pairwise(const std::vector<T> &a, size_t l, size_t r) {
    if (r - l <= 1) return l < r ? a[l] : T(0);
    const size_t m = l + (r - l) / 2;
    return sum_pairwise(a, l, m) + sum_pairwise(a, m, r);
}

// "Как на GPU / в OpenMP reduction": K независимых аккумуляторных дорожек,
// потом их pairwise-сложение. При K=1 это обычный forward, при K=n -- tree.
template <typename T>
static T sum_lanes(const std::vector<T> &a, int lanes) {
    std::vector<T> acc(lanes, T(0));
    for (size_t i = 0; i < a.size(); ++i) acc[i % static_cast<size_t>(lanes)] += a[i];
    while (acc.size() > 1) {
        std::vector<T> next;
        next.reserve((acc.size() + 1) / 2);
        for (size_t i = 0; i < acc.size(); i += 2)
            next.push_back(i + 1 < acc.size() ? acc[i] + acc[i + 1] : acc[i]);
        acc.swap(next);
    }
    return acc[0];
}

template <typename T>
static T sum_kahan(const std::vector<T> &a) {
    T s = 0, c = 0;
    for (T x : a) {
        const T y = x - c;
        const T t = s + y;
        c = (t - s) - y;
        s = t;
    }
    return s;
}

// ------------------------------------------------------------------
// 7.1 Перебор: ищем тройку, у которой скобки дают РАЗНЫЙ результат.
// ------------------------------------------------------------------
struct Triple {
    float a, b, c;
    float left, right;
};

int main() {
    std::cout << "=== 7. Неассоциативность: (a+b)+c != a+(b+c) ===\n";

    // 7.0 -- знаменитый пример «0.1 + 0.2 != 0.3»: проверим побитово, а не по слухам
    {
        const float l = 0.1f + 0.2f, r = 0.3f;
        std::cout << "\n--- 7.0. 0.1f + 0.2f против 0.3f -- побитово ---\n";
        std::cout << "  0.1f + 0.2f = " << std::setprecision(9) << std::fixed << l << "   "
                  << hex_of(l) << "  " << bits32_pretty(bits_of(l)) << "\n"
                  << "            точно в памяти: " << dec::exact_of(l) << "\n";
        std::cout << "  0.3f        = " << r << "   " << hex_of(r) << "  "
                  << bits32_pretty(bits_of(r)) << "\n"
                  << "            точно в памяти: " << dec::exact_of(r) << "\n";
        std::cout << "  равны? " << (l == r ? "ДА" : "НЕТ") << "; разница младших разрядов: "
                  << static_cast<int>(bits_of(l)) - static_cast<int>(bits_of(r)) << "\n";
        const double dl = 0.1 + 0.2, dr = 0.3;
        std::cout << "  то же в double: 0.1 + 0.2 = " << std::setprecision(17) << std::fixed << dl
                  << "  " << hex_of(dl) << " " << bits_pretty(dl) << "\n"
                  << "                   0.3       = " << dr << "  " << hex_of(dr) << " "
                  << bits_pretty(dr) << "\n"
                  << "            точно в памяти: " << dec::exact_of(dl) << "\n"
                  << "                            " << dec::exact_of(dr) << "\n"
                  << "  равны? " << (dl == dr ? "ДА" : "НЕТ")
                  << "; разница младших разрядов: "
                  << static_cast< long long>(bits_of(dl)) - static_cast<long long>(bits_of(dr)) << "\n";
        std::cout << "  >>> вывод: байка «0.1 + 0.2 != 0.3» -- про double; в binary32 (float) обе\n"
                     "      величины приходят к ОДНОМУ представимому числу, и равенство выполняется.\n"
                     "      Т.е. 'проверенный' рецепт из блогов надо применять с указанием формата.\n";
    }

    // Берём "человеческие" слагаемые -- тогда контрпример можно показать пальцем.
    const float pool[] = {0.1f,  0.2f,  0.3f,  1.0f,  3.0f,   1e3f,  1e4f,   1e7f,  1e8f,
                          1e-4f, 1e-7f, 1e-8f, -0.1f, -1.0f,  -1e7f, -1e8f,  0.5f};
    const int npool = static_cast<int>(sizeof(pool) / sizeof(pool[0]));

    std::vector<Triple> found;
    long long checked = 0, broken = 0;
    for (int i = 0; i < npool; ++i)
        for (int j = 0; j < npool; ++j)
            for (int k = 0; k < npool; ++k) {
                const float a = pool[i], b = pool[j], c = pool[k];
                const float left = (a + b) + c;   // сначала левые скобки
                const float right = a + (b + c);  // сначала правые
                ++checked;
                if (!(left == right)) {
                    ++broken;
                    if (found.size() < 4) found.push_back({a, b, c, left, right});
                }
            }

    std::cout << "\n--- 7.1. перебрали ВСЕ " << checked << " троек из набора \"круглых\" чисел;\n"
              << "           скобки дают РАЗНЫЙ ответ в " << broken << " из них (" << std::setprecision(3)
              << std::fixed << (100.0 * broken / checked) << "%). Примеры:\n";
    std::cout << "          a          b          c   |   (a+b)+c    (float)      |   a+(b+c)"
                 "    (float)      | биты (left)   биты (right)\n";
    for (const Triple &t : found) {
        std::cout << "  " << std::right << std::setw(11) << fmt_g(t.a) << std::setw(11) << fmt_g(t.b)
                  << std::setw(11) << fmt_g(t.c) << " | " << std::setw(26) << dec::exact_of(t.left) << " | "
                  << std::setw(26) << dec::exact_of(t.right) << " | " << bits32_pretty(bits_of(t.left))
                  << "\n  " << std::string(33, ' ') << " | " << std::setw(26) << hex_of(t.left) << " | "
                  << std::setw(26) << hex_of(t.right) << " | " << bits32_pretty(bits_of(t.right)) << "\n";
    }
    if (found.empty()) std::cout << "  (в этом наборе все тройки ассоциативны)\n";

    // ------------------------------------------------------------------
    // 7.2 Массив с большим разбросом порядков: один и тот же набор чисел,
    //     разный порядок операций.
    // ------------------------------------------------------------------
    std::cout << "\n--- 7.2. сумма массива из 1 000 000 элементов, разный порядок ---\n";
    std::mt19937_64 rng(2024);  // фиксированный seed -> воспроизводимость
    const size_t N = 1000000;
    std::vector<float> v;
    v.reserve(N);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    for (size_t i = 0; i < N; ++i) {
        // log-uniform по модулю: от 1e-6 до 1e6, со случайным знаком
        const float e = u(rng) * 12.0f - 6.0f;
        const float sgn = (u(rng) < 0.5f) ? -1.0f : 1.0f;
        v.push_back(sgn * std::pow(10.0f, e) * (0.5f + 0.5f * u(rng)));
    }

    // Эталон: та же сумма в long double (на x86 -- 64-битная мантисса) и в double+Kahan.
    std::vector<double> vd(v.begin(), v.end());
    const double ref = sum_kahan<double>(vd);

    auto report = [&](const char *name, float got) {
        const double diff = static_cast<double>(got) - ref;
        std::cout << "  " << ljust_u(name, 30) << " " << std::setw(20) << std::fixed
                  << std::setprecision(9) << got << "  |" << std::setw(16) << std::scientific
                  << std::setprecision(6) << diff << "  |  " << hex_of(got) << "  "
                  << bits32_pretty(bits_of(got)) << "\n";
    };

    std::cout << "  " << ljust_u("метод суммирования", 30) << "     ответ float (точное"
                 " десятичное значение)" << std::string(4, ' ') << " дельта       биты\n";
    std::cout << "  " << std::string(120, '-') << "\n";
    report("left-to-right", sum_forward(v));
    report("right-to-left", sum_backward(v));
    report("pairwise (деление пополам)", sum_pairwise(v, 0, v.size()));
    report("4 дорожки (как reduction)", sum_lanes(v, 4));
    report("8 дорожек", sum_lanes(v, 8));
    report("1024 дорожки", sum_lanes(v, 1024));
    report("Kahan", sum_kahan(v));
    {
        std::vector<float> s = v;
        std::sort(s.begin(), s.end());  // по возрастанию: сначала все отрицательные
        report("сортировка по возрастанию", sum_forward(s));
    }
    {
        std::vector<float> s = v;
        std::sort(s.begin(), s.end(), [](float a, float b) { return std::fabs(a) < std::fabs(b); });
        report("по |x|: сначала мелкие", sum_forward(s));
    }
    {
        std::vector<float> s = v;
        std::shuffle(s.begin(), s.end(), rng);  // "тот же массив, другой порядок"
        report("перемешали (тот же набор!)", sum_forward(s));
    }

    float mn = std::numeric_limits<float>::max(), mx = -std::numeric_limits<float>::max();
    for (const auto &p : {sum_forward(v), sum_backward(v), sum_pairwise(v, 0, v.size()), sum_lanes(v, 8),
                         sum_kahan(v)}) {
        mn = std::min(mn, p);
        mx = std::max(mx, p);
    }
    std::cout << "\n  разброс между \"тем же самым, но другими скобками\": " << std::setprecision(9) << std::fixed
              << (mx - mn) << "  (относительно ~" << std::setprecision(3) << std::scientific
              << (mx - mn) / std::fabs(static_cast<double>(ref)) * 100.0 << "%)\n";
    std::cout << "  double-эталон (Kahan в double) = " << std::setprecision(9) << std::fixed << ref
              << "\n";
    std::cout << "  >>> вывод: 'правильного' результата среди них нет -- есть только разный порядок\n"
                 "      округлений. Отсюда и рецепт: для воспроизводимости фиксировать порядок\n"
                 "      суммирования (или Kahan / целочисленный аккумулятор), а не 'просто сменить компилятор'.\n";
    return 0;
}
