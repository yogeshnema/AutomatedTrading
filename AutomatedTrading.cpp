#include <iostream>
#include <string>
#include "KiteMarketDataService.h"

int main()
{
    std::string apiKey = "your_api_key";
    std::string accessToken = "your_access_token";

    KiteMarketDataService kite(apiKey, accessToken);

    // NIFTY 50 index token
    long nifty50Token = 256265;

    auto candles = kite.getHistoricalCandles(
        nifty50Token,
        "day",
        "2026-07-01",
        "2026-07-08"
    );

    for (const auto& c : candles) {
        std::cout
            << c.timestamp << " "
            << "O=" << c.open << " "
            << "H=" << c.high << " "
            << "L=" << c.low << " "
            << "C=" << c.close << " "
            << "V=" << c.volume
            << "\n";
    }
}