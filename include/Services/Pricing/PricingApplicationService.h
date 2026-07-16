#pragma once

#include "Analytics/Pricing/IPricingModel.h"
#include "Services/Pricing/ITradePricingRepository.h"

#include <string_view>

namespace automated_trading::services::pricing
{
    class PricingApplicationService
    {
    public:
        PricingApplicationService(
            ITradePricingRepository& repository,
            const analytics::pricing::IPricingModel& pricingModel);

        TradePricingOutcome price(
            std::string_view tradeId,
            std::string_view valuationDate) const;
        bool isReady() const;

    private:
        ITradePricingRepository& repository_;
        const analytics::pricing::IPricingModel& pricingModel_;
    };
}
