#include "MarketData/PublishingMarketDataProvider.h"

#include "MarketData/MarketDataEvent.h"

#include <stdexcept>
#include <utility>

namespace
{
    std::string requireProviderName(
        const std::unique_ptr<IMarketDataProvider>& provider)
    {
        if (!provider) {
            throw std::invalid_argument(
                "PublishingMarketDataProvider requires a provider.");
        }
        return provider->providerName();
    }
}

PublishingMarketDataProvider::PublishingMarketDataProvider(
    std::unique_ptr<IMarketDataProvider> provider,
    IEventPublisher& publisher,
    std::string topicPrefix)
    : IMarketDataProvider(requireProviderName(provider)),
      provider_(std::move(provider)),
      publisher_(publisher),
      topicPrefix_(std::move(topicPrefix))
{
}

std::vector<Candle> PublishingMarketDataProvider::getHistoricalCandles(
    const std::string& instrument,
    const std::string& from,
    const std::string& to,
    const std::string& interval)
{
    auto candles = provider_->getHistoricalCandles(
        instrument, from, to, interval);

    for (const auto& candle : candles) {
        const auto event = MarketDataEvent::fromCandle(
            providerName(), instrument, interval, candle);
        publisher_.publish(event.topic(topicPrefix_), event.toJson());
    }

    return candles;
}
