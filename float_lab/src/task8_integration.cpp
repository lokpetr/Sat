// ---------------------------------------------------------------------------
// Пункт 8. Интегрировал, интегрировал, да не выинтегрировал.
//
// Метод прямоугольников (левые / середины) на [0, 1], сравнение с аналитикой,
// float против double, и график ошибки по числу прямоугольников.
//
// Весь смысл: ошибка состоит из ДВУХ частей.
//   * методическая (аппроксимация)  ~ C / N^p   -- уменьшается с ростом N
//   * вычислительная (round-off)    ~ K * eps * sqrt(N)  (в худшем случае ~ N*eps)
// Поэтому график зависимости площади от N -- это буква U в лог-лог осях,
// и "больше прямоугольников" НЕ равно "точнее". У float дно этой буквы
// наступает на 3-4 порядка раньше, чем у double.
//
// Программа пишет CSV (для графиков) и печатает человекочитаемые таблицы.
// ---------------------------------------------------------------------------
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "decimal.h"
#include "floatbits.h"

using u64 = unsigned long long int;

// ---------------------------------------------------------------------------
// Тестируемые функции. Для каждой известна первообразная (F(0) = 0).
// operator() шаблонный -- один и тот же текст кода компилируется и в float,
// и в double, поэтому сравнение типов честное: формулы идентичны.
// ---------------------------------------------------------------------------
struct FuncLine {  // y = x,  integral = 1/2
    template <typename T>
    T operator()(T x) const { return x; }
    template <typename T>
    T antiderivative(T x) const { return x * x / static_cast<T>(2.0); }
    template <typename T>
    T exact() const { return static_cast<T>(0.5); }
    static constexpr const char *name = "y = x (метод средних точен аналитически!)";
    static constexpr const char *slug = "line";
    static constexpr const char *formula = "F(x) = x^2 / 2";
};

struct FuncCubic {  // y = x^3 + x, integral = 1/4 + 1/2 = 0.75
    template <typename T>
    T operator()(T x) const { return x * x * x + x; }
    template <typename T>
    T antiderivative(T x) const { return x * x * x * x / static_cast<T>(4.0) + x * x / static_cast<T>(2.0); }
    template <typename T>
    T exact() const { return static_cast<T>(0.75); }
    static constexpr const char *name = "y = x^3 + x";
    static constexpr const char *slug = "cubic";
    static constexpr const char *formula = "F(x) = x^4/4 + x^2/2";
};

struct FuncQuarterCircle {  // y = sqrt(1-x^2), integral = pi/4
    template <typename T>
    T operator()(T x) const {
        using std::sqrt;
        return sqrt(static_cast<T>(1.0) - x * x);
    }
    template <typename T>
    T antiderivative(T x) const {
        using std::asin;
        using std::sqrt;
        return (x * sqrt(static_cast<T>(1.0) - x * x) + asin(x)) / static_cast<T>(2.0);
    }
    template <typename T>
    T exact() const { return static_cast<T>(0.78539816339744830962); }
    static constexpr const char *name = "y = sqrt(1 - x^2)  (четверть круга)";
    static constexpr const char *slug = "circle";
    static constexpr const char *formula = "F(x) = (x*sqrt(1-x^2) + asin x) / 2";
};

struct FuncPi {  // y = 4/(1+x^2), integral = pi
    template <typename T>
    T operator()(T x) const { return static_cast<T>(4.0) / (static_cast<T>(1.0) + x * x); }
    template <typename T>
    T antiderivative(T x) const {
        using std::atan;
        return static_cast<T>(4.0) * atan(x);
    }
    template <typename T>
    T exact() const { return static_cast<T>(3.14159265358979323846); }
    static constexpr const char *name = "y = 4 / (1 + x^2)   (интеграл = pi)";
    static constexpr const char *slug = "pi";
    static constexpr const char *formula = "F(x) = 4*atan(x)";
};

// ---------------------------------------------------------------------------
// Аккумуляторы: обычный и компенсированный (Kahan).
// ---------------------------------------------------------------------------
template <typename T>
struct NaiveAcc {
    using value_type = T;
    T s = 0;
    void add(T x) { s += x; }
    T value() const { return s; }
};

template <typename T>
struct KahanAcc {
    using value_type = T;
    T s = 0, c = 0;
    void add(T x) {
        const T y = x - c;
        const T t = s + y;
        c = (t - s) - y;
        s = t;
    }
    T value() const { return s; }
};

// ---------------------------------------------------------------------------
// Метод прямоугольников. rule: 0 = левые, 1 = середины, 2 = правые.
//   x_i = a + (i + offset) * h,   h = (b - a) / n,   S += f(x_i) * h
// Аккумулятор отдельный шаблонный параметр -- чтобы одним кодом померить
// эффект Kahan-суммирования на ровно той же формуле.
// ---------------------------------------------------------------------------
enum Rule { LEFT = 0, MID = 1, RIGHT = 2 };

template <typename Acc, typename Fn>
typename Acc::value_type rectangles(const Fn &fn, u64 n, Rule rule, Acc &acc) {
    using T = typename Acc::value_type;
    const T h = static_cast<T>(1.0) / static_cast<T>(n);
    const T off = (rule == LEFT) ? T(0) : (rule == MID ? h / static_cast<T>(2) : h);
    for (u64 i = 0; i < n; ++i) {
        const T x = static_cast<T>(i) * h + off;  // узел; i*h чтобы не накапливать x += h
        acc.add(fn(x) * h);
    }
    return acc.value();
}

template <typename T, typename Fn>
T rectangles(const Fn &fn, u64 n, Rule rule, bool kahan) {
    if (kahan) {
        KahanAcc<T> a;
        return rectangles(fn, n, rule, a);
    }
    NaiveAcc<T> a;
    return rectangles(fn, n, rule, a);
}

// ---------------------------------------------------------------------------
// Вспомогательный вывод
// ---------------------------------------------------------------------------
template <typename T>
static std::string d(T v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", static_cast<double>(v));
    return buf;
}

// ---------------------------------------------------------------------------
// Один "прогон" по всем N для одной функции: пишет два CSV и печатает таблицы.
// ---------------------------------------------------------------------------
template <typename Fn>
void run_function(const Fn &fn, int nmax_pow2, const std::string &out_dir) {
    std::cout << "\n\n######## " << fn.name << " ########\n";
    std::cout << "  аналитически:  integral_0^1 f dx = " << std::setprecision(17) << fn.template exact<double>()
              << "     (" << fn.formula << ", F(1) - F(0))\n";

    // ---- 8.1 таблица "значение площади vs N", float и double, левые/средние ----
    std::ofstream conv(out_dir + std::string("conv_") + fn.slug + ".csv");
    conv << "n,float_left,float_mid,float_right,float_mid_kahan,"
            "double_left,double_mid,double_right,double_mid_kahan,exact\n";

    std::cout << "\n  --- зависимость результата от числа прямоугольников (метод средних) ---\n";
    std::cout << "        N |     float S |      abs.ошибка |  в ulp  |       double S |      abs.ошибка"
                 " |   в ulp\n";
    std::cout << "  " << std::string(100, '-') << "\n";

    u64 best_n_f = 0, best_n_d = 0, bad_n_f = 0, bad_n_d = 0;
    double best_err_f = 1e300, best_err_d = 1e300;
    double end_f = 0, end_d = 0;  // ошибка на самой крупной проверенной сетке

    for (int p = 0; p <= nmax_pow2; ++p) {
        const u64 n = 1ull << static_cast<u64>(p);

        const float fl = rectangles<float>(fn, n, LEFT, false);
        const float fm = rectangles<float>(fn, n, MID, false);
        const float fr = rectangles<float>(fn, n, RIGHT, false);
        const float fmk = rectangles<float>(fn, n, MID, true);
        const double dl = rectangles<double>(fn, n, LEFT, false);
        const double dm = rectangles<double>(fn, n, MID, false);
        const double dr = rectangles<double>(fn, n, RIGHT, false);
        const double dmk = rectangles<double>(fn, n, MID, true);

        const double ex = fn.template exact<double>();
        const double ef = std::fabs(static_cast<double>(fm) - ex);
        const double ed = std::fabs(dm - ex);
        // оптимум = НАИМЕНЬШЕЕ N с минимальной ошибкой (для y = x метод средних точен
        // при любом N, и "победителей" много -- берём самого мелкого), плюс отдельно
        // заводим "с какого N начинает портиться" -- это и есть практический предел
        if (ef < best_err_f) { best_err_f = ef; best_n_f = n; }
        if (ed < best_err_d) { best_err_d = ed; best_n_d = n; }
        if (!bad_n_f && ef > best_err_f) bad_n_f = n;
        if (!bad_n_d && ed > best_err_d) bad_n_d = n;
        end_f = ef;
        end_d = ed;

        const double ef_ulp = ef / (ulp32(fm) > 0 ? ulp32(fm) : 1);
        const double ed_ulp = ed / (ulp64(dm) > 0 ? ulp64(dm) : 1);

        std::cout << "  " << std::setw(9) << n << " |" << std::setw(13) << std::setprecision(9)
                  << std::fixed << fm << " |" << std::setw(16) << std::scientific << ef << " |"
                  << std::setw(9) << fmt_g(static_cast<float>(ef_ulp)) << " |" << std::setw(15)
                  << std::setprecision(12) << std::fixed << dm << " |" << std::setw(16) << std::scientific
                  << ed << " |" << std::setw(9) << fmt_g(ed_ulp) << "\n";

        conv << n << "," << d(fl) << "," << d(fm) << "," << d(fr) << "," << d(fmk) << "," << d(dl) << ","
             << d(dm) << "," << d(dr) << "," << d(dmk) << "," << d(ex) << "\n";
    }
    conv.close();

    const std::string nmax_s = std::to_string(1ull << (u64)nmax_pow2);
    auto bad_txt = [](u64 b) { return b ? std::to_string(b) : std::string("нет (в пределе теста)"); };
    std::cout << "\n  ОПТИМУМ float : N = " << best_n_f << " (ошибка " << std::scientific
              << std::setprecision(3) << best_err_f << "), портиться начинает с N = " << bad_txt(bad_n_f)
              << "; на N = " << nmax_s << " ошибка уже " << end_f << "\n";
    std::cout << "  ОПТИМУМ double: N = " << best_n_d << " (ошибка " << best_err_d
              << "), портиться начинает с N = " << bad_txt(bad_n_d) << "; на N = " << nmax_s << " ошибка "
              << end_d << "\n"
              << "  (левые прямоугольники, кстати, сходятся как 1/N -- см. CSV и график: там"
                 " видно\n   и методическую ошибку, и 'пол' из round-off; левый/средний/правый"
                 " варианты лежат в CSV)\n";

    // ---- 8.2 кривые f(x) и F(x) + заштрихованные прямоугольники ----
    {
        std::ofstream out(out_dir + std::string("curve_") + fn.slug + ".csv");
        out << "i,x,float_f,float_F,double_f,double_F\n";
        const int m = 1000;
        for (int i = 0; i <= m; ++i) {
            const double x = static_cast<double>(i) / m;
            const double ff = fn(x);
            const double dF = fn.antiderivative(x);
            out << i << "," << x << "," << d(static_cast<float>(fn(static_cast<float>(x)))) << ","
                << d(static_cast<float>(fn.antiderivative(static_cast<float>(x)))) << "," << d(ff) << ","
                << d(dF) << "\n";
        }
    }

    // ---- 8.3 демонстрация "x_i = i*h" против накопления "x += h" ----
    std::cout << "\n  --- 8.3 тот же интеграл, но узел считается с накоплением: x += h ---\n"
              << "      (классический рецепт из лекций: узел нельзя накапливать, узел надо пересчитывать из i)\n";
    std::cout << "          N |  h = 1/N точно? |    x=i*h    |     x+=h     |      разница |  ош. пересчёт |  ош. накопл. |  x после цикла\n";
    const u64 n3[] = {1024ull, 4096ull, 65536ull, 1000ull, 10000ull, 100000ull, 1048576ull, 16777216ull};
    const double ex = fn.template exact<double>();
    for (u64 n : n3) {
        if (n > (1ull << static_cast<u64>(nmax_pow2))) continue;
        const float h = 1.0f / static_cast<float>(n);

        NaiveAcc<float> a1;
        for (u64 i = 0; i < n; ++i) a1.add(fn(static_cast<float>(i) * h + h / 2.0f) * h);

        float x = 0.0f;  // накопительный шаг -- как в лекции
        NaiveAcc<float> a2;
        for (u64 i = 0; i < n; ++i) {
            a2.add(fn(x + h / 2.0f) * h);
            x += h;
        }
        // h представима точно только когда n -- степень двойки; тогда x += h не округляется вовсе
        const bool h_exact = (static_cast<double>(h) * static_cast<double>(n)) == 1.0;
        std::cout << "  " << std::setw(9) << n << " |" << std::setw(18)
                  << (h_exact ? "да (n = 2^k)" : "нет") << " |"
                  << std::setw(13) << std::setprecision(9) << std::fixed << a1.value() << " |"
                  << std::setw(13) << a2.value() << " |" << std::setw(15) << std::setprecision(6)
                  << std::scientific << std::fabs(static_cast<double>(a1.value()) - a2.value())
                  << " |" << std::setw(15) << std::fabs(static_cast<double>(a1.value()) - ex) << " |"
                  << std::setw(14) << std::fabs(static_cast<double>(a2.value()) - ex) << " |"
                  << std::setw(13) << std::setprecision(7) << std::fixed << x
                  << (x > 1.0f ? "   <-- x ВЫШЕЛ ЗА b!" : "") << "\n";
    }
    std::cout << "      вывод: пока n -- степень двойки, h = 2^-k точна, и накопление x += h не округляется\n"
              << "      вообще: разница нулевая, и рецепт выглядит пустой перестраховкой. Он перестаёт быть\n"
              << "      декоративным, стоит взять h неточной (n кратно 10) или длинный отрезок: дрейф узла\n"
              << "      растёт линейно по i, портит ответ на 2-3 порядка сильнее и в конце концов выносит x\n"
              << "      за границу интеграла -- там sqrt(1-x*x) становится sqrt(отрицательного), и вся сумма\n"
              << "      обращается в NaN (строки со «ВЫШЕЛ ЗА b»). Тот же самый приём, кстати, запрещает и\n"
              << "      параллельную сумму по i: накопленный узел невозможно вычислить для k-го блока\n"
              << "      независимо, поэтому в SIMD/GPU-коде узел всегда пересчитывают от индекса.\n";

}

// ---------------------------------------------------------------------------
// Бонус: сравнение с "эталонным" ответом, посчитанным целыми числами (для y=x
// это возможно ровно) -- чтобы показать, что метод вообще-то точен, а виноват float.
// ---------------------------------------------------------------------------
static void exact_linear_demo() {
    std::cout << "\n\n--- для y = x левые прямоугольники дают ровно (n-1)/(2n) в целых числах ---\n";
    std::cout << "        N |   точное (n-1)/(2n)  |  float-ответ  |  double-ответ  |  float ошибается?\n";
    for (int p = 0; p <= 24; p += 2) {
        const u64 n = 1ull << static_cast<u64>(p);
        const double exact = (static_cast<double>(n) - 1.0) / (2.0 * static_cast<double>(n));
        const float fsum = rectangles<float>(FuncLine{}, n, LEFT, false);
        const double dsum = rectangles<double>(FuncLine{}, n, LEFT, false);
        std::cout << "  " << std::setw(9) << n << " |" << std::setw(21) << std::setprecision(17)
                  << std::fixed << exact << " |" << std::setw(16) << std::setprecision(9) << fsum
                  << " |" << std::setw(16) << std::setprecision(15) << dsum << " |" << std::setw(18)
                  << std::scientific << (static_cast<double>(fsum) - exact) << "\n";
    }
}

int main(int argc, char **argv) {
    std::string out_dir = "results/";
    int nmax_pow2 = 27;  // 2^27 прямоугольников: за глаза хватает, чтобы увидеть дно U-кривой и у double
    if (argc > 1) out_dir = argv[1];
    if (argc > 2) nmax_pow2 = std::atoi(argv[2]);
    if (out_dir.back() != '/') out_dir += '/';

    std::cout << "=== 8. Интегрирование методом прямоугольников: float vs double ===\n";
    std::cout << "  CSV пишут в " << out_dir << ", максимум N = 2^" << nmax_pow2 << " = "
              << (1ull << static_cast<u64>(nmax_pow2)) << "\n";

    run_function(FuncLine{}, nmax_pow2, out_dir);
    run_function(FuncCubic{}, nmax_pow2, out_dir);
    run_function(FuncQuarterCircle{}, nmax_pow2, out_dir);
    run_function(FuncPi{}, nmax_pow2, out_dir);
    exact_linear_demo();

    std::cout << "\n--- главный вывод пункта 8 ---\n"
              << "  у float оптимум достигается на N на несколько порядков раньше, чем у double,\n"
              << "  а дальше уточнение сетки УХУДШАЕТ ответ: прибавляемое слагаемое становится\n"
              << "  меньше шага сетки float и тонет в шуме округления (см. пункт 5 про ulp).\n"
              << "  Лечится не 'большим N', а типом поточнее / Kahan / композицией (см. Makefile: kahan-колонки).\n";
    return 0;
}
