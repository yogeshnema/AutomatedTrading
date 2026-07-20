#include "Products/Equity/Options/Exercise/EuropeanExercise.h"

namespace automated_trading::products::equity::options::exercise
{
    EuropeanExercise::EuropeanExercise(std::chrono::sys_seconds expiry)
        : exerciseDates_{expiry}
    {
    }

    ExerciseStyle EuropeanExercise::style() const noexcept
    {
        return ExerciseStyle::European;
    }

    bool EuropeanExercise::canExercise(std::chrono::sys_seconds time) const noexcept
    {
        return time >= exerciseDates_.front();
    }

    std::span<const std::chrono::sys_seconds> EuropeanExercise::exerciseDates() const noexcept
    {
        return exerciseDates_;
    }
}
