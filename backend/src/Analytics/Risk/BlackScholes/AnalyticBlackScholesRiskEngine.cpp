#include "Analytics/Risk/BlackScholes/AnalyticBlackScholesRiskEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace automated_trading::analytics::risk::black_scholes
{
    namespace
    {
        constexpr double VolatilityPoint = 0.01;
        constexpr double BasisPoint = 0.0001;
        constexpr double DaysPerYear = 365.0;

        void validate(const EuropeanOptionRiskInput& input)
        {
            if (!std::isfinite(input.spot) || input.spot <= 0.0)
                throw std::invalid_argument("Spot must be positive and finite.");
            if (!std::isfinite(input.strike) || input.strike <= 0.0)
                throw std::invalid_argument("Strike must be positive and finite.");
            if (!std::isfinite(input.volatility) || input.volatility < 0.0)
                throw std::invalid_argument("Volatility must be non-negative and finite.");
            if (!std::isfinite(input.timeToExpiryYears) || input.timeToExpiryYears < 0.0)
                throw std::invalid_argument("Time to expiry must be non-negative and finite.");
            if (!std::isfinite(input.riskFreeRate) || !std::isfinite(input.dividendYield) ||
                !std::isfinite(input.positionScale))
                throw std::invalid_argument("Rates and position scale must be finite.");
        }

        GreekVector scale(const GreekVector& value, double factor) noexcept
        {
            return {value.delta * factor, value.gamma * factor, value.vega * factor,
                    value.theta * factor, value.rho * factor};
        }
    }

    std::string_view AnalyticBlackScholesRiskEngine::model() const noexcept
    {
        return "black_scholes";
    }

    std::string_view AnalyticBlackScholesRiskEngine::method() const noexcept
    {
        return "analytic";
    }

    std::string_view AnalyticBlackScholesRiskEngine::computeBackend() const noexcept
    {
        return "scalar_cpu";
    }

    RiskResult AnalyticBlackScholesRiskEngine::calculate(
        const EuropeanOptionRiskInput& input) const
    {
        validate(input);
        const bool isCall = input.optionRight == products::equity::options::OptionRight::Call;
        RiskResult result;

        if (input.timeToExpiryYears == 0.0 || input.volatility == 0.0) {
            const double discountedSpot = input.spot * std::exp(-input.dividendYield * input.timeToExpiryYears);
            const double discountedStrike = input.strike * std::exp(-input.riskFreeRate * input.timeToExpiryYears);
            const double signedIntrinsic = isCall
                ? discountedSpot - discountedStrike
                : discountedStrike - discountedSpot;
            result.unitPrice = std::max(signedIntrinsic, 0.0);
            if (signedIntrinsic > 0.0) {
                result.unitGreeks.delta = isCall
                    ? std::exp(-input.dividendYield * input.timeToExpiryYears)
                    : -std::exp(-input.dividendYield * input.timeToExpiryYears);
            }
            result.presentValue = result.unitPrice * input.positionScale;
            result.positionGreeks = scale(result.unitGreeks, input.positionScale);
            return result;
        }

        const double sqrtTime = std::sqrt(input.timeToExpiryYears);
        const double d1 = (std::log(input.spot / input.strike) +
            (input.riskFreeRate - input.dividendYield + 0.5 * input.volatility * input.volatility) *
                input.timeToExpiryYears) / (input.volatility * sqrtTime);
        const double d2 = d1 - input.volatility * sqrtTime;
        const double discountRate = std::exp(-input.riskFreeRate * input.timeToExpiryYears);
        const double discountDividend = std::exp(-input.dividendYield * input.timeToExpiryYears);

        double annualTheta = 0.0;
        double rawRho = 0.0;
        if (isCall) {
            result.unitPrice = input.spot * discountDividend * normalCdf(d1) -
                input.strike * discountRate * normalCdf(d2);
            result.unitGreeks.delta = discountDividend * normalCdf(d1);
            annualTheta = -(input.spot * discountDividend * normalPdf(d1) * input.volatility) /
                    (2.0 * sqrtTime)
                - input.riskFreeRate * input.strike * discountRate * normalCdf(d2)
                + input.dividendYield * input.spot * discountDividend * normalCdf(d1);
            rawRho = input.strike * input.timeToExpiryYears * discountRate * normalCdf(d2);
        }
        else {
            result.unitPrice = input.strike * discountRate * normalCdf(-d2) -
                input.spot * discountDividend * normalCdf(-d1);
            result.unitGreeks.delta = discountDividend * (normalCdf(d1) - 1.0);
            annualTheta = -(input.spot * discountDividend * normalPdf(d1) * input.volatility) /
                    (2.0 * sqrtTime)
                + input.riskFreeRate * input.strike * discountRate * normalCdf(-d2)
                - input.dividendYield * input.spot * discountDividend * normalCdf(-d1);
            rawRho = -input.strike * input.timeToExpiryYears * discountRate * normalCdf(-d2);
        }

        result.unitGreeks.gamma = discountDividend * normalPdf(d1) /
            (input.spot * input.volatility * sqrtTime);
        result.unitGreeks.vega = input.spot * discountDividend * normalPdf(d1) * sqrtTime * VolatilityPoint;
        result.unitGreeks.theta = annualTheta / DaysPerYear;
        result.unitGreeks.rho = rawRho * BasisPoint;
        result.presentValue = result.unitPrice * input.positionScale;
        result.positionGreeks = scale(result.unitGreeks, input.positionScale);
        return result;
    }

    void AnalyticBlackScholesRiskEngine::calculateBatch(
        std::span<const EuropeanOptionRiskInput> inputs,
        std::span<RiskResult> outputs) const
    {
        if (inputs.size() != outputs.size())
            throw std::invalid_argument("Risk batch input and output sizes must match.");
        for (std::size_t index = 0; index < inputs.size(); ++index)
            outputs[index] = calculate(inputs[index]);
    }

    double AnalyticBlackScholesRiskEngine::normalCdf(double value) noexcept
    {
        return 0.5 * std::erfc(-value / std::numbers::sqrt2);
    }

    double AnalyticBlackScholesRiskEngine::normalPdf(double value) noexcept
    {
        return std::exp(-0.5 * value * value) / std::sqrt(2.0 * std::numbers::pi);
    }
}
