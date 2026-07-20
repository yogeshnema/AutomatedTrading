#pragma once

namespace automated_trading::products::equity::options
{
    enum class OptionRight
    {
        Call,
        Put
    };

    enum class ExerciseStyle
    {
        European,
        American,
        Bermudan
    };
}
