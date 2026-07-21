#pragma once

#include "Analytics/Risk/IRiskEngine.h"
#include "Services/Pricing/ITradePricingRepository.h"
#include "Services/Risk/TradeRiskData.h"

#include <string_view>

namespace automated_trading::services::risk
{
    class RiskApplicationService
    {
    public:
        RiskApplicationService(
            pricing::ITradePricingRepository& repository,
            const analytics::risk::IRiskEngine& riskEngine);

        TradeRiskOutcome calculate(
            std::string_view tradeId,
            std::string_view valuationDate,
            std::string_view method) const;
        bool isReady() const;

    private:
        pricing::ITradePricingRepository& repository_;
        const analytics::risk::IRiskEngine& riskEngine_;
    };
}
