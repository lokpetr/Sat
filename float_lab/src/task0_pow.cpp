// ---------------------------------------------------------------------------
// Пункт 0. "Немного чёрной магии": my_pow vs std::pow для float и double.
// Что показываем: две таблицы по три столбца (степень, my_pow, std::pow).
// ---------------------------------------------------------------------------
#include <iostream>
#include <type_traits>

#include "pow_table.h"

int main() {
    std::cout << "=== 0. Чёрная магия std::pow на целых числах ===\n";

    // ------------------------------------------------------------------
    // Сначала покажем, ПОЧЕМУ это вообще магия: std::pow не умеет целые,
    // и перегрузка для unsigned long long тихо превращается в double pow.
    // ------------------------------------------------------------------
    u64 b = 3, n = 17;
    std::cout << "\n- std::pow(unsigned long long, unsigned long long) на самом деле возвращает "
              << (std::is_same<decltype(std::pow(b, n)), double>::value ? "double" : "??")
              << " (тип результата -- самый 'широкий' из доступных)\n";
    std::cout << "- чтобы вызвать pow для float, ОБА аргумента надо привести к float явно\n\n";

    std::cout << "  3^17 точное            = " << my_pow(b, n) << "\n";
    std::cout << "  std::pow(3, 17)        = " << std::fixed << std::setprecision(9) << std::pow(b, n)
              << "   <- double\n";
    std::cout << "  std::pow(3.0f, 17.0f)  = " << std::setprecision(9)
              << std::pow(static_cast<float>(b), static_cast<float>(n)) << "   <- float\n";
    std::cout << "  (float)my_pow(3, 17)   = " << std::setprecision(9)
              << static_cast<float>(my_pow(b, n)) << "   <- то же округление, но вручную\n";

    // ------------------------------------------------------------------
    // Таблицы. Основание 3 интересно тем, что 3^k -- нечётное число всегда,
    // поэтому в float/double оно влезает ровно только пока хватает битов
    // мантиссы. Основания 2 и 10 показаны для контраста.
    // ------------------------------------------------------------------
    print_pow_table<float>(3, 100, /*show_bits = */ false);
    print_pow_table<double>(3, 100, /*show_bits = */ false);

    std::cout << "\n--- для контраста: степень двойки (в float/double всегда точна, пока влезает"
                 " в диапазон) ---\n";
    print_pow_table<float>(2, 130, false);
    std::cout << "\n--- ...а десятичная уже нет: 10^8 для float ---\n";
    print_pow_table<float>(10, 40, false);

    return 0;
}
