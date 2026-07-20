#include "MarketData/KiteMarketDataService.h"
#include <vector>
#include <string>
#include <stdexcept>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

KiteMarketDataService::KiteMarketDataService(const KiteSession& session)
    : session_(session)
{
    // initialization
}

std::vector<Candle> KiteMarketDataService::getHistoricalCandles(long instrumentToken,
    const std::string& from, const std::string& to, const std::string& interval)
{
    std::vector<Candle> candles;

    std::string url =
        "https://api.kite.trade/instruments/historical/" +
        std::to_string(instrumentToken) + "/" + interval;

    auto response = cpr::Get(
        cpr::Url{ url },
        session_.commonHeaders(),
        cpr::Parameters{
            {"from", from},
            {"to", to}
        }
    );

    if (response.status_code != 200) {
        throw std::runtime_error(
            "Kite API error: HTTP " +
            std::to_string(response.status_code) +
            "\n" + response.text
        );
    }

    auto j = nlohmann::json::parse(response.text);


    for (const auto& row : j["data"]["candles"]) {
        Candle c;
        c.timestamp = row[0].get<std::string>();
        c.open = row[1].get<double>();
        c.high = row[2].get<double>();
        c.low = row[3].get<double>();
        c.close = row[4].get<double>();
        c.volume = row[5].get<long>();

        candles.push_back(c);
    }

    return candles;
}
