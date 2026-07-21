#include "Services/Risk/RiskApplicationService.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>

namespace automated_trading::services::risk
{
    namespace
    {
        constexpr double SecondsPerYear = 365.0 * 24.0 * 60.0 * 60.0;

        std::uint64_t nanoseconds(
            std::chrono::steady_clock::time_point from,
            std::chrono::steady_clock::time_point to) noexcept
        {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(to - from).count());
        }
    }

    RiskApplicationService::RiskApplicationService(
        pricing::ITradePricingRepository& repository,
        const analytics::risk::IRiskEngine& riskEngine)
        : repository_(repository), riskEngine_(riskEngine)
    {
    }

    TradeRiskOutcome RiskApplicationService::calculate(
        std::string_view tradeId,
        std::string_view valuationDate,
        std::string_view method) const
    {
        if (tradeId.empty()) throw std::invalid_argument("tradeId must not be empty.");
        if (valuationDate.empty()) throw std::invalid_argument("valuationDate must not be empty.");
        if (method.empty()) method = riskEngine_.method();
        if (method != riskEngine_.method()) {
            throw std::invalid_argument(
                "Risk method '" + std::string(method) + "' is not available. Use '" +
                std::string(riskEngine_.method()) + "'.");
        }

        const auto totalStarted = std::chrono::steady_clock::now();
        const auto input = repository_.load(tradeId, valuationDate);
        const auto loaded = std::chrono::steady_clock::now();
        const double secondsToExpiry = std::chrono::duration<double>(
            input.expiry - input.valuationTime).count();

        TradeRiskOutcome outcome;
        outcome.tradeId = input.tradeId;
        outcome.valuationDate = std::string(valuationDate);
        outcome.productType = "european_vanilla_option";
        outcome.underlyingAssetId = input.underlyingAssetId;
        outcome.currency = input.currency;
        outcome.model = riskEngine_.model();
        outcome.method = riskEngine_.method();
        outcome.computeBackend = riskEngine_.computeBackend();
        outcome.signedQuantity = input.signedQuantity;
        outcome.contractMultiplier = input.contractMultiplier;
        outcome.input = {
            input.optionRight,
            input.marketData.spot,
            input.strike,
            input.marketData.riskFreeRate,
            input.marketData.dividendYield,
            input.marketData.volatility,
            std::max(secondsToExpiry / SecondsPerYear, 0.0),
            input.signedQuantity * input.contractMultiplier};

        outcome.result = riskEngine_.calculate(outcome.input);
        const auto computed = std::chrono::steady_clock::now();
        outcome.latency.loadNanoseconds = nanoseconds(totalStarted, loaded);
        outcome.latency.computeNanoseconds = nanoseconds(loaded, computed);
        outcome.latency.totalNanoseconds = nanoseconds(totalStarted, computed);
        return outcome;
    }

    bool RiskApplicationService::isReady() const
    {
        return repository_.isReady();
    }
}
