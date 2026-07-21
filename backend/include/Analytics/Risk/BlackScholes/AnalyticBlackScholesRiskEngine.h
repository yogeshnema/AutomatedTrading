#pragma once

#include "Analytics/Risk/IRiskEngine.h"

namespace automated_trading::analytics::risk::black_scholes
{
    class AnalyticBlackScholesRiskEngine final : public IRiskEngine
    {
    public:
        std::string_view model() const noexcept override;
        std::string_view method() const noexcept override;
        std::string_view computeBackend() const noexcept override;
        RiskResult calculate(const EuropeanOptionRiskInput& input) const override;
        void calculateBatch(
            std::span<const EuropeanOptionRiskInput> inputs,
            std::span<RiskResult> outputs) const override;

    private:
        static double normalCdf(double value) noexcept;
        static double normalPdf(double value) noexcept;
    };
}
