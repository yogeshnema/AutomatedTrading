#pragma once

#include "Common/EventMessaging.h"
#include "MarketData/IMarketDataProvider.h"

#include <memory>
#include <string>

class PublishingMarketDataProvider final : public IMarketDataProvider
{
public:
    PublishingMarketDataProvider(
        std::unique_ptr<IMarketDataProvider> provider,
        IEventPublisher& publisher,
        std::string topicPrefix);

    std::vector<Candle> getHistoricalCandles(
        const std::string& instrument,
        const std::string& from,
        const std::string& to,
        const std::string& interval) override;

private:
    std::unique_ptr<IMarketDataProvider> provider_;
    IEventPublisher& publisher_;
    std::string topicPrefix_;
};
