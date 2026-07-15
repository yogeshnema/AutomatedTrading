#pragma once

#include "MarketData/IMarketDataProvider.h"

#include <string>

class PolygonMarketDataProvider final : public IMarketDataProvider
{
public:
    explicit PolygonMarketDataProvider(const std::string& configPath);

    std::vector<Candle> getHistoricalCandles(
        const std::string& instrument,
        const std::string& from,
        const std::string& to,
        const std::string& interval) override;

private:
    std::string apiKey_;
};
