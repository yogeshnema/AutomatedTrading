#pragma once

#include "KiteSession.h"

#include <string>
#include <vector>

struct Candle
{
    std::string timestamp;
    double open;
    double high;
    double low;
    double close;
    long volume;
};

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
