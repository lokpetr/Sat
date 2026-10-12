// ---------------------------------------------------------------------------
// decimal.h -- точное десятичное представление того, что реально лежит в памяти.
//
// setprecision(9) у cout показывает "удобное" число, а не то, что там лежит.
// Чтобы увидеть настоящую погрешность (0.2f на самом деле 0.20000000298023223877
// и т.п.), нужно распечатать дробь mant * 2^exp в десятичной системе БЕЗ
// округлений. Делается школьным умножением "столбиком" -- 2^k это 2*2*...*2,
// а 5^k потом просто сдвигает запятую.
//
// Это же "bigint" переиспользуется в пункте 0/3 как независимый эталон для
// проверки my_pow vs std::pow: 3^100 в unsigned long long не влезает никогда,
// а в string влезает всегда.
// ---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "floatbits.h"

namespace dec {

// Строка цифр, индекс 0 = МЛАДШИЙ разряд (так умножать "столбиком" удобнее).
inline std::string from_u64(uint64_t v) {
    std::string s;
    if (v == 0) return "0";
    while (v) {
        s.push_back(static_cast<char>('0' + (v % 10)));
        v /= 10;
    }
    return s;
}

inline std::string mul(const std::string &a, uint64_t m) {
    if (m == 0) return "0";
    std::string out;
    out.reserve(a.size() + 16);
    uint64_t carry = 0;
    for (char c : a) {
        carry += static_cast<uint64_t>(c - '0') * m;
        out.push_back(static_cast<char>('0' + (carry % 10)));
        carry /= 10;
    }
    while (carry) {
        out.push_back(static_cast<char>('0' + (carry % 10)));
        carry /= 10;
    }
    return out;
}

inline std::string pow_pow2(std::string digits, uint64_t k) {
    for (uint64_t i = 0; i < k; ++i) digits = mul(digits, 2);
    return digits;
}

inline std::string pow_pow5(std::string digits, uint64_t k) {
    for (uint64_t i = 0; i < k; ++i) digits = mul(digits, 5);
    return digits;
}

// "1024" из реверснутой строки "4201"
inline std::string normalize(const std::string &rev) {
    std::string s = rev;
    std::reverse(s.begin(), s.end());
    return s;
}

// base^n ровно, в десятичных цифрах.
inline std::string pow_exact(uint64_t base, uint64_t n) {
    std::string d = "1";
    for (uint64_t i = 0; i < n; ++i) d = mul(d, base);
    return normalize(d);
}

// ---------------------------------------------------------------------------
// Точное десятичное значение float/double по его битам.
// ---------------------------------------------------------------------------
template <typename BitsT, int kMantBits, int kBias, int kExpBits>
inline std::string exact_of_bits(BitsT raw) {
    const int kTotal = kMantBits + kExpBits + 1;
    const BitsT sign = (raw >> (kTotal - 1)) & BitsT(1);
    const BitsT exp_field = (raw >> kMantBits) & ((BitsT(1) << kExpBits) - 1);
    const BitsT mant_field = raw & ((BitsT(1) << kMantBits) - 1);
    const BitsT exp_all_ones = (BitsT(1) << kExpBits) - 1;

    if (exp_field == exp_all_ones) return mant_field ? "NaN" : (sign ? "-inf" : "+inf");

    int e;
    BitsT M;
    if (exp_field == 0) {  // субнормаль: скрытая единицы нет
        e = 1 - kBias;
        M = mant_field;
    } else {
        e = static_cast<int>(exp_field) - kBias;
        M = mant_field | (BitsT(1) << kMantBits);
    }
    if (M == 0) return sign ? "-0" : "0";

    // value = M * 2^(e - kMantBits)
    const int k = e - kMantBits;
    std::string digits = from_u64(static_cast<uint64_t>(M));
    std::string result;
    if (k >= 0) {
        result = normalize(pow_pow2(digits, static_cast<uint64_t>(k)));
    } else {
        // M * 5^|k| / 10^|k|  ->  просто ставим запятую после |k| цифр
        const uint64_t shift = static_cast<uint64_t>(-k);
        std::string n = pow_pow5(digits, shift);  // всё ещё реверс (младший спереди)
        while (n.size() <= shift) n.push_back('0');
        std::string frac = n.substr(0, shift);                      // младшие цифры = дробь
        std::string whole = normalize(n.substr(shift));             // остальные = целая часть
        // frac тоже в реверсе, т.е. index 0 -- ПОСЛЕДНЯЯ цифра дроби: нули срезаем с начала
        size_t significant = 0;
        while (significant < frac.size() && frac[significant] == '0') ++significant;
        frac.erase(0, significant);
        result = frac.empty() ? whole : whole + "." + normalize(frac);
    }
    if (sign) result = "-" + result;
    return result;
}

inline std::string exact_of(float f) { return exact_of_bits<uint32_t, 23, 127, 8>(bits_of(f)); }
inline std::string exact_of(double d) { return exact_of_bits<uint64_t, 52, 1023, 11>(bits_of(d)); }

}  // namespace dec
