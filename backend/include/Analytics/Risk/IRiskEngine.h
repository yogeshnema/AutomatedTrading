#pragma once

#include "Analytics/Risk/RiskTypes.h"

#include <span>
#include <string_view>

namespace automated_trading::analytics::risk
{
    // The interface keeps transport, persistence and hardware selection out of
    // the calculation kernel. Batch buffers are owned by the caller so SIMD or
    // CUDA implementations do not need per-request heap allocation.
    class IRiskEngine
    {
    public:
        virtual ~IRiskEngine() = default;

        virtual std::string_view model() const noexcept = 0;
        virtual std::string_view method() const noexcept = 0;
        virtual std::string_view computeBackend() const noexcept = 0;
        virtual RiskResult calculate(const EuropeanOptionRiskInput& input) const = 0;
        virtual void calculateBatch(
            std::span<const EuropeanOptionRiskInput> inputs,
            std::span<RiskResult> outputs) const = 0;
    };
}
