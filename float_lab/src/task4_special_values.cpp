// ---------------------------------------------------------------------------
// Пункт 4. Особые значения: +inf, -inf, NaN (а заодно +0/-0, субнормалы и
// границы диапазона). Все они -- не "ошибки языка", а заранее зарезервированные
// комбинации поля степени в IEEE 754: E = all ones.
// ---------------------------------------------------------------------------
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>

#include "floatbits.h"

static std::string val(float v) { return fmt_auto_f(v, 9, 22); }
static std::string val(double v) { return fmt_auto_d(v, 17, 26); }

template <typename T>
static void show(const char *how, T v) {
    std::cout << "  " << ljust_u(how, 44) << val(v) << " | " << hex_of(v) << " | " << bits_pretty(v)
              << "\n";
}

// NaN'ов бывает много: у них "только" степень = all ones и НЕНОЛЬ мантисса.
template <typename T>
T nan_with_payload(uint64_t payload);
template <>
float nan_with_payload<float>(uint64_t p) {
    return float_from_bits(0x7FC00000u | (static_cast<uint32_t>(p) & 0x3FFFFFu));
}
template <>
double nan_with_payload<double>(uint64_t p) {
    return double_from_bits(0x7FF8000000000000ull | (p & 0xFFFFFFFFFFFFFull));
}

template <typename T>
static void table(const char *type_name) {
    const T one = static_cast<T>(1);
    const T zero = static_cast<T>(0);
    std::cout << "\n--- " << type_name << " ---\n";
    std::cout << "  " << ljust_u("как получено", 44) << std::right
              << std::setw(static_cast<int>(val(T(0)).size())) << "значение"
              << " |     сырые биты    | расшифровка по полям\n";
    std::cout << "  " << std::string(44 + val(T(0)).size() + 60 + 32, '-') << "\n";

    // +inf: переполнение или деление на ноль. Исключения НЕ будет.
    show("one / zero", one / zero);
    show("-one / zero", -one / zero);
    show("numeric_limits<T>::infinity()", std::numeric_limits<T>::infinity());
    show("pow(10, maxExp10 + 2) -- переполнение",
         std::pow(static_cast<T>(10), static_cast<T>(std::numeric_limits<T>::max_exponent10 + 2)));
    show("max * 2 -- переполнение", std::numeric_limits<T>::max() * static_cast<T>(2));
    show("max + max -- то же самое", std::numeric_limits<T>::max() + std::numeric_limits<T>::max());

    // NaN: 0/0, inf-inf, sqrt(-1), ...
    show("zero / zero", zero / zero);
    show("inf + (-inf)", std::numeric_limits<T>::infinity() - std::numeric_limits<T>::infinity());
    show("sqrt(-1)", std::sqrt(-one));
    show("numeric_limits<T>::quiet_NaN()", std::numeric_limits<T>::quiet_NaN());
    show("numeric_limits<T>::signaling_NaN()", std::numeric_limits<T>::signaling_NaN());
    show("NaN с payload = 1 (свои биты)", nan_with_payload<T>(1));
    show("quiet NaN с максимальным payload", nan_with_payload<T>(~0ull));

    // Нули и субнормалы: тут E = 0, "1.M" превращается в "0.M"
    show("+0", zero);
    show("-0", -zero);
    show("1 / inf  (подflow к нулю)", one / std::numeric_limits<T>::infinity());
    show("min normal", std::numeric_limits<T>::min());
    show("min subnormal = denorm_min", std::numeric_limits<T>::denorm_min());
    show("0.5 * denorm_min (=> 0, underflow)", static_cast<T>(0.5) * std::numeric_limits<T>::denorm_min());
    show("max", std::numeric_limits<T>::max());
    show("epsilon", std::numeric_limits<T>::epsilon());
}

int main() {
    std::cout << "=== 4. Особые значения float/double ===\n";
    table<float>("float (binary32):  S|E(8)|M(23)");
    table<double>("double (binary64): S|E(11)|M(52)");

    std::cout << "\n--- арифметика с ними (IEEE 754 почти замыкательна: inf и NaN"
                 " никуда не деваются) ---\n";
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::cout << std::setprecision(9);
    std::cout << "  inf + 1        = " << inf + 1.0f << "          <- inf уже не станет больше\n";
    std::cout << "  inf + inf      = " << inf + inf << "\n";
    std::cout << "  inf - inf      = " << inf - inf << "          <- NaN, а не 0!\n";
    std::cout << "  inf / inf      = " << inf / inf << "\n";
    std::cout << "  1 / inf        = " << 1.0f / inf << "\n";
    std::cout << "  nan == nan     : " << (nan == nan ? "да" : "НЕТ") << "  <- NaN не равен даже сам себе\n";
    std::cout << "  nan != nan     : " << (nan != nan ? "да" : "НЕТ") << "  <- отсюда идиоматическая проверка (x != x)\n";
    std::cout << "  isnan(nan) = " << std::isnan(nan) << ", isinf(inf) = " << std::isinf(inf)
              << ", isfinite(inf) = " << std::isfinite(inf) << "\n";
    std::cout << "  max > inf      : " << (std::numeric_limits<float>::max() > inf ? "да" : "НЕТ")
              << "  <- любое 'число' меньше бесконечности, и это удобно в алгоритмах\n";
    std::cout << "  0.0f == -0.0f  : " << (0.0f == -0.0f ? "да" : "НЕТ")
              << "  <- но биты разные (см. выше), и 1/0 != 1/(-0)\n";
    std::cout << "\n  зачем это знать: 'нет решения' часто кодируют как inf/NaN, и если NaN затёк"
                 "\n  в сумму, то весь ответ станет NaN. Именно поэтому NaN любят как маркер (или"
                 "\n  ненавидят -- он 'заражает' всё вокруг, но не падает).\n";
    return 0;
}
