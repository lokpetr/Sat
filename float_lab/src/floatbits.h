// ---------------------------------------------------------------------------
// floatbits.h -- "кирпичики" для всей лабы по флоатам.
//
// Идея: один раз написать код, который умеет смотреть на память как на биты,
// а потом переиспользовать его во всех пунктах (1, 2, 3, 4, 5, 8).
//
//   * to_binary_bits(x)      -- любое беззнаковое целое -> строка из 0 и 1
//                               (32 бита для unsigned int, 64 для unsigned long long)
//   * bits_of(float/double)  -- reinterpret cast через union (как просили в лабе)
//   * bits32_pretty()/bits64_pretty()
//                            -- та же строка, но разбитая на S | E | M группами по 4
//   * decode32()/decode64()  -- ручная расшифровка IEEE 754-2008 + проверка, что
//                               собранные из битов число совпадает с исходным
//   * ulp32()/ulp64()        -- "цена последнего разряда": расстояние до соседа
//
// Компиляция: -std=c++17 -O2 (см. Makefile). Работает на x86-64, ARM, RISC-V --
// везде, где float/double это IEEE 754 binary32/binary64 (то есть почти везде).
// ---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

// ---------------------------------------------------------------------------
// Пункт 1. unsigned int -> двоичная строка. Все 32 бита, со старшими нулями.
// Шаблон, чтобы тем же кодом печатать и 64-битные слова (нужно для double).
// ---------------------------------------------------------------------------
template <typename UIntT>
std::string to_binary_bits(UIntT x) {
    static_assert(std::numeric_limits<UIntT>::is_integer && !std::numeric_limits<UIntT>::is_signed,
                  "нужен беззнаковый целочисленный тип");
    const int kBits = static_cast<int>(sizeof(UIntT) * 8);
    std::string s(kBits, '0');
    // Идём СО СТАРШИХ битов (вариант "слева направо" из подсказки): тогда цифры
    // сразу оказываются в правильном порядке и никакой рекурсии/массива не надо.
    for (int i = 0; i < kBits; ++i) {
        const UIntT bit = (x >> (kBits - 1 - i)) & UIntT(1);  // битовая маска + сдвиг
        s[i] = bit ? '1' : '0';
    }
    return s;
}

// Тот же результат "на минималках": делим на степень двойки сдвигом вправо,
// остаток снимаем побитовым И. Цифры получаются задом наперёд, разворачиваем.
template <typename UIntT>
std::string to_binary_bits_shift_right(UIntT x) {
    const int kBits = static_cast<int>(sizeof(UIntT) * 8);
    std::string s;
    s.reserve(kBits);
    for (int i = 0; i < kBits; ++i) {
        s.push_back((x & UIntT(1)) ? '1' : '0');  // "x & 1" = младший бит
        x >>= 1;                                   // "откусили" обработанный бит
    }
    std::reverse(s.begin(), s.end());
    return s;
}

// ---------------------------------------------------------------------------
// Пункт 2. посмотреть на float как на последовательность битов.
// Присваивание float -> unsigned int не годится (округлит до целой части),
// поэтому переиспользуем память: union, у которого поля лежат в одном месте.
// ---------------------------------------------------------------------------
inline uint32_t bits_of(float f) {
    union {
        float f;
        uint32_t u;
    } view{};
    view.f = f;
    return view.u;
}

inline uint64_t bits_of(double d) {
    union {
        double d;
        uint64_t u;
    } view{};
    view.d = d;
    return view.u;
}

// "Честный" вариант (C++20) -- без UB, но пока прилично срабатывает и union:
//   uint32_t bits_of(float f) { return std::bit_cast<uint32_t>(f); }

inline float float_from_bits(uint32_t u) {
    union {
        uint32_t u;
        float f;
    } view{};
    view.u = u;
    return view.f;
}

inline double double_from_bits(uint64_t u) {
    union {
        uint64_t u;
        double d;
    } view{};
    view.u = u;
    return view.d;
}

// ---------------------------------------------------------------------------
// Выравнивание по КОЛИЧЕСТВУ СИМВОЛОВ, а не байт: иначе русские подписи в
// таблицах разъезжаются (setw считает байты, а в UTF-8 кириллица -- 2 байта).
// ---------------------------------------------------------------------------
inline size_t utf8_width(const std::string &s) {
    size_t n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++n;  // не "хвост" многобайтовой последовательности
    }
    return n;
}

inline std::string ljust_u(const std::string &s, size_t w) {
    std::string r = s;
    while (utf8_width(r) < w) r += ' ';
    return r;
}

// ---------------------------------------------------------------------------
// Красивая печать: группы по 4 бита, поля разделены '|'.
// ---------------------------------------------------------------------------
inline std::string group_by_4(const std::string &bits) {
    std::string out;
    const int n = static_cast<int>(bits.size());
    for (int i = 0; i < n; ++i) {
        if (i && (n - i) % 4 == 0) out += ' ';
        out += bits[i];
    }
    return out;
}

inline std::string bits32_pretty(uint32_t raw) {
    const std::string b = to_binary_bits(raw);
    const std::string s = b.substr(0, 1);
    const std::string e = b.substr(1, 8);
    const std::string m = b.substr(9, 23);
    return s + "|" + group_by_4(e) + "|" + group_by_4(m);
}

inline std::string bits64_pretty(uint64_t raw) {
    const std::string b = to_binary_bits(raw);
    const std::string s = b.substr(0, 1);
    const std::string e = b.substr(1, 11);
    const std::string m = b.substr(12, 52);
    return s + "|" + group_by_4(e) + "|" + group_by_4(m);
}

// Подпись полей строго под строкой bits32_pretty()/bits64_pretty():
//   0|0111 1111|000 0000 ...
//   S|EEEE EEEE|MMM MMMM ...
inline std::string marks32() {
    return "S|" + group_by_4(std::string(8, 'E')) + "|" + group_by_4(std::string(23, 'M'));
}
inline std::string marks64() {
    return "S|" + group_by_4(std::string(11, 'E')) + "|" + group_by_4(std::string(52, 'M'));
}

// Универсальная обёртка "значение любого типа -> его биты с подписанными полями"
template <typename T>
std::string bits_pretty(T v);
template <>
inline std::string bits_pretty<float>(float v) { return bits32_pretty(bits_of(v)); }
template <>
inline std::string bits_pretty<double>(double v) { return bits64_pretty(bits_of(v)); }
template <typename T>
std::string hex_of(T v);
template <>
inline std::string hex_of<float>(float v) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << bits_of(v)
       << std::dec << std::setfill(' ');
    return os.str();
}
template <>
inline std::string hex_of<double>(double v) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << bits_of(v)
       << std::dec << std::setfill(' ');
    return os.str();
}

// ---------------------------------------------------------------------------
// Ручная расшифровка IEEE 754-2008 и проверка себя.
//   value = (-1)^S * 1,M * 2^(E - bias)          -- нормальные
//   value = (-1)^S * 0,M * 2^(1 - bias)          -- субнормальные (E == 0)
//   E == all 1s:  M == 0 -> +-inf, M != 0 -> NaN
// ---------------------------------------------------------------------------
struct F32Fields {
    uint32_t raw = 0;
    uint32_t sign = 0, exp_field = 0, mant_field = 0;
    int real_exp = 0;
    bool subnormal = false, special = false;
};

inline F32Fields split32(uint32_t raw) {
    F32Fields r;
    r.raw = raw;
    r.sign = (raw >> 31) & 0x1u;
    r.exp_field = (raw >> 23) & 0xFFu;
    r.mant_field = raw & 0x7FFFFFu;
    r.subnormal = (r.exp_field == 0u);
    r.special = (r.exp_field == 0xFFu);
    r.real_exp = r.subnormal ? (1 - 127) : (static_cast<int>(r.exp_field) - 127);
    return r;
}

inline double recover32(const F32Fields &r) {
    if (r.special) return std::nan("");
    double mant = 1.0;
    if (r.subnormal) mant = 0.0;
    for (int i = 0; i < 23; ++i) {
        if (r.mant_field & (1u << (22 - i))) mant += std::ldexp(1.0, -(i + 1));
    }
    double v = mant * std::ldexp(1.0, r.real_exp);
    return r.sign ? -v : v;
}

inline std::string decode32(uint32_t raw) {
    const F32Fields r = split32(raw);
    std::ostringstream os;
    os << "S=" << r.sign << "  E=" << std::setw(3) << std::setfill('0') << r.exp_field << " ("
       << std::setfill(' ') << (static_cast<int>(r.exp_field) - 127) << ")  M=" << std::hex
       << std::setw(6) << std::setfill('0') << r.mant_field << std::dec << std::setfill(' ');
    if (r.special) {
        os << (r.mant_field ? "  => NaN" : (r.sign ? "  => -inf" : "  => +inf"));
    } else if (r.subnormal) {
        os << "  => субнормаль (скрытая единица = 0), степень 2^" << r.real_exp;
    } else {
        os << "  => 1.M * 2^" << r.real_exp;
    }
    return os.str();
}

struct F64Fields {
    uint64_t raw = 0;
    uint64_t sign = 0, exp_field = 0, mant_field = 0;
    int real_exp = 0;
    bool subnormal = false, special = false;
};

inline F64Fields split64(uint64_t raw) {
    F64Fields r;
    r.raw = raw;
    r.sign = (raw >> 63) & 0x1ull;
    r.exp_field = (raw >> 52) & 0x7FFull;
    r.mant_field = raw & 0xFFFFFFFFFFFFFull;
    r.subnormal = (r.exp_field == 0ull);
    r.special = (r.exp_field == 0x7FFull);
    r.real_exp = r.subnormal ? (1 - 1023) : (static_cast<int>(r.exp_field) - 1023);
    return r;
}

inline std::string decode64(uint64_t raw) {
    const F64Fields r = split64(raw);
    std::ostringstream os;
    os << "S=" << r.sign << "  E=" << std::setw(4) << std::setfill('0') << r.exp_field << " ("
       << std::setfill(' ') << (static_cast<int>(r.exp_field) - 1023) << ")  M=0x" << std::hex
       << std::setw(13) << std::setfill('0') << r.mant_field << std::dec << std::setfill(' ');
    if (r.special) {
        os << (r.mant_field ? "  => NaN" : (r.sign ? "  => -inf" : "  => +inf"));
    } else if (r.subnormal) {
        os << "  => субнормаль, степень 2^" << r.real_exp;
    } else {
        os << "  => 1.M * 2^" << r.real_exp;
    }
    return os.str();
}

// ---------------------------------------------------------------------------
// "Цена шага" -- расстояние до следующего представимого числа. Именно она
// объясняет и пункт 0 (потеря точности), и пункт 5 (бесконечный цикл).
// ---------------------------------------------------------------------------
inline float ulp32(float v) {
    if (!std::isfinite(v)) return std::numeric_limits<float>::quiet_NaN();
    const float nxt = std::nextafterf(v, std::numeric_limits<float>::infinity());
    return nxt - v;
}

inline double ulp64(double v) {
    if (!std::isfinite(v)) return std::numeric_limits<double>::quiet_NaN();
    const double nxt = std::nextafter(v, std::numeric_limits<double>::infinity());
    return nxt - v;
}

// ---------------------------------------------------------------------------
// Мелкие помощники для таблиц: печать с фиксированной шириной и "полным"
// количеством знаков, чтобы было видно именно то, что лежит в памяти.
// ---------------------------------------------------------------------------
inline std::string fmt_f(float v, int prec = 9, int width = 0) {
    if (std::isinf(v)) return v < 0 ? "-inf" : "+inf";
    if (std::isnan(v)) return "nan";
    std::ostringstream os;
    if (width) os << std::setw(width);
    os << std::fixed << std::setprecision(prec) << v;
    return os.str();
}

inline std::string fmt_d(double v, int prec = 17, int width = 0) {
    if (std::isinf(v)) return v < 0 ? "-inf" : "+inf";
    if (std::isnan(v)) return "nan";
    std::ostringstream os;
    if (width) os << std::setw(width);
    os << std::fixed << std::setprecision(prec) << v;
    return os.str();
}

// "Аккуратный столбик" как просили в лабе, но без разъезжающейся таблицы:
// фиксированная точка для нормальных величин, scientific для очень больших
// (иначе 3^90 в fixed -- это 40 символов, и таблица превращается в кашу).
inline std::string fmt_auto_f(float v, int prec = 9, int width = 26) {
    std::ostringstream os;
    if (std::isinf(v)) {
        os << std::setw(width) << (v < 0 ? "-inf" : "+inf");
        return os.str();
    }
    if (std::isnan(v)) {
        os << std::setw(width) << "nan";
        return os.str();
    }
    const double a = std::fabs(static_cast<double>(v));
    os << std::setw(width);
    if (a >= 1e16 || (a != 0.0 && a < 1e-4))
        os << std::scientific << std::setprecision(prec) << v;
    else
        os << std::fixed << std::setprecision(prec) << v;
    return os.str();
}

inline std::string fmt_auto_d(double v, int prec = 6, int width = 30) {
    std::ostringstream os;
    if (std::isinf(v)) {
        os << std::setw(width) << (v < 0 ? "-inf" : "+inf");
        return os.str();
    }
    if (std::isnan(v)) {
        os << std::setw(width) << "nan";
        return os.str();
    }
    const double a = std::fabs(v);
    os << std::setw(width);
    if (a >= 1e16 || (a != 0.0 && a < 1e-8))
        os << std::scientific << std::setprecision(prec) << v;
    else
        os << std::fixed << std::setprecision(prec) << v;
    return os.str();
}

// %g-подобная печать: компактно и без разъезжающихся "1000.00000000"
inline std::string fmt_g(float v, int width = 0, int prec = 6) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*g", prec, static_cast<double>(v));
    std::string s(buf);
    while (width && utf8_width(s) < static_cast<size_t>(width)) s = " " + s;
    return s;
}

inline std::string fmt_g(double v, int width = 0, int prec = 6) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
    std::string s(buf);
    while (width && utf8_width(s) < static_cast<size_t>(width)) s = " " + s;
    return s;
}

inline std::string fmt_u(uint64_t v, int width = 0) {
    std::ostringstream os;
    if (width) os << std::setw(width);
    os << v;
    return os.str();
}
