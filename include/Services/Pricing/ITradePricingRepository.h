#pragma once

#include "Services/Pricing/TradePricingData.h"

#include <string_view>

namespace automated_trading::services::pricing
{
    class ITradePricingRepository
    {
    public:
        virtual ~ITradePricingRepository() = default;

        virtual TradePricingInput load(
            std::string_view tradeId,
            std::string_view valuationDate) = 0;
        virtual bool isReady() = 0;
    };
}
