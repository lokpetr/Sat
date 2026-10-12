// ---------------------------------------------------------------------------
// Пункт 2. float -> двоичное представление в памяти + ручная сверка с
// IEEE 754-2008. Смотрим на те же самые биты как на unsigned int (union!),
// а не присваиваем float в unsigned int (тогда было бы просто округление).
// ---------------------------------------------------------------------------
#include <cmath>
#include <iomanip>
#include <iostream>

#include "decimal.h"
#include "floatbits.h"

static void show(float v) {
    const uint32_t raw = bits_of(v);
    const F32Fields f = split32(raw);
    const std::string raw_bits = to_binary_bits(raw);

    std::cout << "\nfloat " << std::setprecision(9) << std::fixed << v << "   (0x" << std::hex
              << std::uppercase << std::setw(8) << std::setfill('0') << raw << std::dec
              << std::setfill(' ') << ")\n";
    std::cout << "  все 32 бита как есть : " << raw_bits << "\n";
    std::cout << "  то же, с пробелами   : " << bits32_pretty(raw) << "\n";
    std::cout << "  что где лежит        : " << marks32()
              << "   (S - знак, E - степень, M - мантисса)\n";
    std::cout << "  " << decode32(raw) << "\n";

    if (!f.special) {
        const double recovered = recover32(f);  // собираем число ОБРАТНО из битов
        std::cout << "  собранное вручную: " << std::setprecision(9) << std::fixed << recovered
                  << (recovered == static_cast<double>(v) ? "   == исходное (IEEE 754 работает)"
                                                           : "   != исходное (!!)")
                  << "\n";
        std::cout << "  в памяти реально : " << dec::exact_of(v) << "\n";
    }
}

int main() {
    std::cout << "=== 2. float -> binary, расшифровка IEEE 754-2008 ===\n";

    show(1.0f);      // 0 01111111 0...0
    show(0.5f);      // 0 01111110 0...0
    show(-0.5f);
    show(3.1415927f);
    show(100.5f);    // "пример с лекции"
    show(0.2f);      // 0.2 НЕ представимо!
    show(1.0f / 3.0f);
    show(1.0f + 1.0f / 8388608.0f);  // самый маленький шаг около единицы: 2^-23

    // Крайние случаи формата: нули, субнормалы, границы диапазона.
    std::cout << "\n--- особые случаи формата ---\n";
    struct { const char *name; float v; } cases[] = {
        {"+0.0f", 0.0f},
        {"-0.0f", -0.0f},
        {"min normal", std::numeric_limits<float>::min()},
        {"min subnormal", std::numeric_limits<float>::denorm_min()},
        {"max float", std::numeric_limits<float>::max()},
        {"epsilon (1+eps != 1)", std::numeric_limits<float>::epsilon()},
        {"1.0f + eps/2 (round-half-to-even)", 1.0f + 0.5f * std::numeric_limits<float>::epsilon()},
    };
    for (auto &c : cases) {
        const uint32_t raw = bits_of(c.v);
        std::cout << std::left << std::setw(34) << c.name << std::right << " " << bits32_pretty(raw)
                  << "   E=" << std::setw(3) << split32(raw).exp_field << "  M=" << std::hex
                  << std::setw(6) << std::setfill('0') << split32(raw).mant_field << std::dec
                  << std::setfill(' ') << "   " << fmt_auto_f(c.v, 9, 18) << "\n";
    }
    // "округление к чётному" на примере 1.0f + eps/2: это ровно середина между
    // 1.0f и 1.0f+eps, и почему-то оно равно 1.0f, а не 1.0f+eps
    const float half = 1.0f + 0.5f * std::numeric_limits<float>::epsilon();
    std::cout << "\n  1.0f + eps/2 == 1.0f ? " << (half == 1.0f ? "да (округлили к чётной мантиссе)" : "нет")
              << ",  1.0f + eps*0.5000001 == 1.0f ? "
              << (1.0f + 0.5000001f * std::numeric_limits<float>::epsilon() == 1.0f ? "да" : "нет")
              << "\n  (вот она, tie-to-even: при точной середине мантисса выбирается чётной)\n";

    std::cout << "\n  epsilon = " << dec::exact_of(std::numeric_limits<float>::epsilon())
              << " = 2^-23, это и есть 'цена последнего разряда' около 1\n";
    return 0;
}
