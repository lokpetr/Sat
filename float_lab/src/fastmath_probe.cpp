// ---------------------------------------------------------------------------
// Мини-пробник "что именно ломает -ffast-math". Собирается ДВАЖДЫ -- один раз с
// обычным -O2, один раз с -ffast-math; выводы сравнивает `make probe`.
// Он же используется в README в разделе про устаревающие рецепты.
//
// Важно: все значения заводятся через volatile, чтобы компилятор не свернул их
// в константы на этапе компиляции -- иначе пробник проверял бы не кодогенерацию,
// а калькулятор GCC.
// ---------------------------------------------------------------------------
#include <cmath>
#include <cstdio>

int main() {
    volatile float big = 1e30f;
    const float inf = big * big;      // overflow -> +inf
    volatile float one = 1.0f, tenth = 0.1f;
    volatile float huge = 1e8f;
    volatile float neg = -1.0f;
    volatile float zero = 0.0f;

    std::printf("1) inf+1.0f == inf          : %s\n", (inf + 1.0f == inf) ? "да" : "нет");
    std::printf("2) inf = %g, isinf(inf)     : %d\n", inf, static_cast<int>(std::isinf(inf)));
    std::printf("3) isfinite(inf)            : %d\n", static_cast<int>(std::isfinite(inf)));
    std::printf("4) 0.0f/0.0f = %g, isnan()  : %d\n", zero / zero,
                static_cast<int>(std::isnan(zero / zero)));
    std::printf("5) sqrt(-1.0f) = %g, isfinite(): %d\n", std::sqrt(neg),
                static_cast<int>(std::isfinite(std::sqrt(neg))));
    {
        const float s = huge + tenth;
        std::printf("6) 1e8f + 0.1f - 1e8f       : %d   (0.1 потерялся? %s)\n",
                    static_cast<int>(s - huge), (s - huge == 0.0f) ? "да" : "нет");
    }
    {
        float x = 16777216.0f;                 // 2^24 -- ровно та точка из пункта 5
        std::printf("7) (2^24 + 1.0f) == 2^24    : %s   <-- детектор зависания из пункта 5\n",
                    (x + one == x) ? "да" : "нет");
    }
    {
        float a = huge, b = tenth, c = -huge;
        std::printf("8) (a+b)+c                  : %g   [асоциативность: (a+b)+c vs a+(b+c)]\n",
                    (a + b) + c);
        std::printf("   a+(b+c)                  : %g\n", a + (b + c));
    }
    return 0;
}
