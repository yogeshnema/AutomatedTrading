#pragma once

#include "MarketData/IMarketDataProvider.h"
#include "MarketData/KiteMarketDataService.h"
#include "Common/KiteSession.h"

#include <string>

class KiteMarketDataProvider final : public IMarketDataProvider
{
public:
    explicit KiteMarketDataProvider(const std::string& configPath);

    std::vector<Candle> getHistoricalCandles(
        const std::string& instrument,
        const std::string& from,
        const std::string& to,
        const std::string& interval) override;

private:
    KiteSession session_;
    KiteMarketDataService service_;
};
