#pragma once

#include "Products/Equity/Options/OptionTypes.h"

#include <chrono>
#include <span>

namespace automated_trading::products::equity::options::exercise
{
    class IExerciseRule
    {
    public:
        virtual ~IExerciseRule() = default;

        virtual ExerciseStyle style() const noexcept = 0;
        virtual bool canExercise(std::chrono::sys_seconds time) const noexcept = 0;
        virtual std::span<const std::chrono::sys_seconds> exerciseDates() const noexcept = 0;
    };
}
