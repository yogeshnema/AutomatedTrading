#pragma once

#include "Products/Equity/Options/Exercise/IExerciseRule.h"

#include <array>

namespace automated_trading::products::equity::options::exercise
{
    class EuropeanExercise final : public IExerciseRule
    {
    public:
        explicit EuropeanExercise(std::chrono::sys_seconds expiry);

        ExerciseStyle style() const noexcept override;
        bool canExercise(std::chrono::sys_seconds time) const noexcept override;
        std::span<const std::chrono::sys_seconds> exerciseDates() const noexcept override;

    private:
        std::array<std::chrono::sys_seconds, 1> exerciseDates_;
    };
}
