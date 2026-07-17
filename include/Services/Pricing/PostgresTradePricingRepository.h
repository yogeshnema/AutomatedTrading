#pragma once

#include "Database/QueryService.h"
#include "Services/Pricing/ITradePricingRepository.h"

#include <mutex>

namespace automated_trading::services::pricing
{
    class PostgresTradePricingRepository final : public ITradePricingRepository
    {
    public:
        explicit PostgresTradePricingRepository(database::QueryService& queryService);

        TradePricingInput load(
            std::string_view tradeId,
            std::string_view valuationDate) override;
        bool isReady() override;

    private:
        database::QueryService& queryService_;
        std::mutex connectionMutex_;
    };
}
