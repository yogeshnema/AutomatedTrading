#pragma once

#include "MarketData/IMarketDataProvider.h"
#include "Common/KiteSession.h"

#include <string>
#include <vector>

class KiteMarketDataService
{
public:
    explicit KiteMarketDataService(const KiteSession& session);

    std::vector<Candle> getHistoricalCandles(
        long instrumentToken,
        const std::string& from,
        const std::string& to,
        const std::string& interval);

private:
    const KiteSession& session_;
}; 
