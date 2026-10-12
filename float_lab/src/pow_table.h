// ---------------------------------------------------------------------------
// pow_table.h -- общий генератор таблиц для пунктов 0 и 3.
//
// Один и тот же код печатает и "три столбца" (пункт 0), и "четыре столбца"
// (пункт 3): достаточно включить флаг show_bits. Тип (float/double) задаётся
// шаблонным параметром, поэтому для double ничего отдельно писать не нужно.
// ---------------------------------------------------------------------------
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#include "decimal.h"
#include "floatbits.h"

using u64 = unsigned long long int;

// ---------------------------------------------------------------------------
// Свой my_pow: "быстрое возведение в степень" (бинарный алгоритм), O(log n)
// умножений вместо O(n). Заодно честно детектируем переполнение u64, чтобы не
// выдавать wrap-around за правильный ответ.
// ---------------------------------------------------------------------------
inline bool mul_overflow(u64 a, u64 b, u64 &out) {
    const bool overflow = (a != 0ull && b > std::numeric_limits<u64>::max() / a);
    out = a * b;  // при переполнении -- значение по модулю 2^64 (wrap-around)
    return overflow;
}

inline u64 my_pow(u64 x, u64 n, bool &overflowed) {
    u64 result = 1ull;
    overflowed = false;
    while (n > 0ull) {
        if (n & 1ull) overflowed = mul_overflow(result, x, result) || overflowed;
        n >>= 1;
        if (n > 0ull) overflowed = mul_overflow(x, x, x) || overflowed;
    }
    return result;
}

inline u64 my_pow(u64 x, u64 n) {
    bool dummy = false;
    return my_pow(x, n, dummy);
}

// ---------------------------------------------------------------------------
// Traits: чем float-вариант таблицы отличается от double-варианта.
// ---------------------------------------------------------------------------
template <typename T>
struct FloatTraits;

template <>
struct FloatTraits<float> {
    static constexpr const char *name = "float";
    static constexpr int kBits = 32;
    static constexpr int kValueWidth = 26;
    static float call(u64 b, u64 n) { return std::pow(static_cast<float>(b), static_cast<float>(n)); }
    static std::string bits(float v) { return bits32_pretty(bits_of(v)); }
    static std::string hex(float v) {
        std::ostringstream os;
        os << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << bits_of(v)
           << std::dec;
        return os.str();
    }
    static std::string value(float v) { return fmt_auto_f(v, 9, kValueWidth); }
    static std::string exact(float v) { return dec::exact_of(v); }
    static double ulp(float v) { return ulp32(v); }
    static float rounded_exact(const std::string &s) { return std::strtof(s.c_str(), nullptr); }
    static int max_exponent10() { return std::numeric_limits<float>::max_exponent10; }
    static int mantissa_bits() { return 24; }  // 23 явных + 1 скрытая единица
};

template <>
struct FloatTraits<double> {
    static constexpr const char *name = "double";
    static constexpr int kBits = 64;
    static constexpr int kValueWidth = 30;
    static double call(u64 b, u64 n) { return std::pow(static_cast<double>(b), static_cast<double>(n)); }
    static std::string bits(double v) { return bits64_pretty(bits_of(v)); }
    static std::string hex(double v) {
        std::ostringstream os;
        os << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << bits_of(v)
           << std::dec;
        return os.str();
    }
    static std::string value(double v) { return fmt_auto_d(v, 6, kValueWidth); }
    static std::string exact(double v) { return dec::exact_of(v); }
    static double ulp(double v) { return ulp64(v); }
    static double rounded_exact(const std::string &s) { return std::strtod(s.c_str(), nullptr); }
    static int max_exponent10() { return std::numeric_limits<double>::max_exponent10; }
    static int mantissa_bits() { return 53; }
};

// ---------------------------------------------------------------------------
// Таблица.  show_bits == false -> пункт 0 (3 столбца),
//           show_bits == true  -> пункт 3 (4 столбца, + биты в памяти).
// ---------------------------------------------------------------------------
template <typename T>
void print_pow_table(u64 base, int nmax, bool show_bits, std::ostream &os = std::cout) {
    using TR = FloatTraits<T>;

    os << "\n### основание " << base << ", " << TR::name << " (" << TR::kBits
       << " бита, значащих разрядов " << TR::mantissa_bits() << ")\n";
    os << std::right;
    const std::string bits_head = TR::bits(T(1.5));
    os << "    n |" << std::setw(24) << "my_pow(base, n) |" << std::setw(TR::kValueWidth + 3)
       << "std::pow(" << TR::name << ")";
    if (show_bits) os << " |" << std::setw(static_cast<int>(bits_head.size()) + 2) << "биты в памяти";
    os << " |" << std::setw(14) << "ulp(шаг)"
       << " |" << std::setw(10) << "бит потерь"
       << " | статус\n";
    os << std::string(show_bits ? int(bits_head.size()) + 118 : 118, '-') << "\n";

    int first_inexact = -1;  // ответ уже НЕ равен точному целому
    int first_inf = -1;      // переполнение диапазона float/double
    int first_wrap = -1;     // my_pow не влезло в u64
    int first_wrong = -1;    // std::pow даже не корректно округлилась

    for (int n = 0; n <= nmax; ++n) {
        bool overflowed = false;
        const u64 exact_int = my_pow(base, static_cast<u64>(n), overflowed);
        const std::string exact_dec = dec::pow_exact(base, static_cast<u64>(n));  // истинное 3^n
        const T got = TR::call(base, static_cast<u64>(n));

        const std::string got_dec = TR::exact(got);          // что реально лежит в памяти
        const bool exact_match = (got_dec == exact_dec);     // совпало ли ДОЛЬШЕ некуда
        const bool correct_rounding = (got == TR::rounded_exact(exact_dec));

        if (overflowed && first_wrap < 0) first_wrap = n;
        if (!exact_match && first_inexact < 0) first_inexact = n;
        if (!correct_rounding && first_wrong < 0) first_wrong = n;
        if (std::isinf(got) && first_inf < 0) first_inf = n;

        os << std::setw(5) << n << " |" << std::setw(24);
        if (overflowed)
            os << "[u64 overflow, " << exact_dec.size() << " цифр]";
        else
            os << exact_int;

        os << " |" << TR::value(got);
        if (show_bits) os << " |" << std::setw(static_cast<int>(bits_head.size()) + 2) << TR::bits(got);
        os << " |";

        const double u = TR::ulp(got);
        char ulp_buf[32];
        if (std::isfinite(u))
            std::snprintf(ulp_buf, sizeof(ulp_buf), "%13.3g", u);
        else
            std::snprintf(ulp_buf, sizeof(ulp_buf), "%13s", "--");
        os << ulp_buf;

        // "бит потерь" k: шаг сетки равен 2^k, значит k МЛАДШИХ двоичных разрядов целого
        // числа в этом float/double физически нечем записать -- вот они и теряются
        int lost_bits = 0;
        if (std::isfinite(u) && u >= 1.0) {
            lost_bits = static_cast<int>(std::lround(std::log2(u)));
            if (lost_bits < 0) lost_bits = 0;
        }
        os << " |" << std::setw(9) << (std::isinf(got) ? std::string("inf") : std::to_string(lost_bits));

        os << " | " << (exact_match ? "точно" : (correct_rounding ? "округлено до ближайшего" : "ОШИБКА pow"));
        os << "\n";
    }

    auto num = [](int i) { return i >= 0 ? std::to_string(i) : std::string("вне диапазона теста"); };
    os << "\n  * my_pow перестаёт влезать в unsigned long long начиная с n = " << num(first_wrap) << "\n";
    os << "  * std::pow впервые ОТВЕЧАЕТ НЕТОЧНО (уже не то самое целое) начиная с n = "
       << num(first_inexact) << "\n";
    os << "  * результат впервые ОКРУГЛЁН НЕВЕРНО (не ближайший представимый) -- n = "
       << num(first_wrong) << "\n";
    os << "  * std::pow впервые ВЫХОДИТ ЗА ДИАПАЗОН (=> inf) начиная с n = " << num(first_inf)
       << "   (макс. порядок 1e" << TR::max_exponent10() << ")\n";
}

// Подробный разбор одной строки таблицы: что за число на самом деле хранится.
template <typename T>
std::string explain_row(u64 base, u64 n, std::ostream &os = std::cout) {
    using TR = FloatTraits<T>;
    const std::string exact_dec = dec::pow_exact(base, n);
    const T got = TR::call(base, n);
    os << "\n  разбор n = " << n << " для " << TR::name << ":\n"
       << "    точное значение  base^n = " << exact_dec << "   (" << exact_dec.size() << " цифр)\n"
       << "    в памяти лежит  " << TR::hex(got) << "  " << TR::bits(got) << "\n"
       << "    это ровно      " << TR::exact(got) << "\n"
       << "    std::pow вернула (как её печатает cout) " << TR::value(got) << "\n";
    return TR::exact(got);
}
