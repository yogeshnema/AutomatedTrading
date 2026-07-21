#pragma once

#include "Analytics/Risk/RiskTypes.h"

#include <cstdint>
#include <string>

namespace automated_trading::services::risk
{
    struct RiskLatency
    {
        std::uint64_t loadNanoseconds{0};
        std::uint64_t computeNanoseconds{0};
        std::uint64_t totalNanoseconds{0};
    };

    struct TradeRiskOutcome
    {
        std::string tradeId;
        std::string valuationDate;
        std::string productType;
        std::string underlyingAssetId;
        std::string currency;
        std::string model;
        std::string method;
        std::string computeBackend;
        double signedQuantity{0.0};
        double contractMultiplier{1.0};
        analytics::risk::EuropeanOptionRiskInput input;
        analytics::risk::RiskResult result;
        RiskLatency latency;
    };
}
