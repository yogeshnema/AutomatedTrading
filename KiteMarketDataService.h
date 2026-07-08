#pragma once

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
    KiteMarketDataService(const std::string& apiKey,
        const std::string& accessToken);

    std::vector<Candle> getHistoricalCandles(
        long instrumentToken,
        const std::string& interval,
        const std::string& from,
        const std::string& to);

private:
    std::string apiKey_;
    std::string accessToken_;
}; 
