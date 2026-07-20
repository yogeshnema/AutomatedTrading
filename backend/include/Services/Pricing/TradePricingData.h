#pragma once

#include "Analytics/Pricing/PricingTypes.h"
#include "Products/Equity/Options/OptionTypes.h"

#include <chrono>
#include <string>

namespace automated_trading::services::pricing
{
    struct TradePricingInput
    {
        std::string tradeId;
        std::string productId;
        std::string underlyingAssetId;
        std::string currency;
        products::equity::options::OptionRight optionRight{
            products::equity::options::OptionRight::Call};
        double strike{0.0};
        std::chrono::sys_seconds expiry{};
        double contractMultiplier{1.0};
        double signedQuantity{0.0};
        std::chrono::sys_seconds valuationTime{};
        analytics::pricing::EquityMarketData marketData;
    };

    struct TradePricingOutcome
    {
        std::string tradeId;
        std::string valuationDate;
        std::string productType;
        std::string underlyingAssetId;
        std::string currency;
        double signedQuantity{0.0};
        double contractMultiplier{1.0};
        double spot{0.0};
        double strike{0.0};
        double riskFreeRate{0.0};
        double dividendYield{0.0};
        double volatility{0.0};
        analytics::pricing::PricingResult result;
    };
}
