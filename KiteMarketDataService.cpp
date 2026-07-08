#include <iostream>
#include <string>
#include <vector>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

struct Candle {
    std::string timestamp;
    double open;
    double high;
    double low;
    double close;
    long volume;
};

class KiteMarketDataService {
private:
    std::string apiKey_;
    std::string accessToken_;

public:
    KiteMarketDataService(std::string apiKey, std::string accessToken)
        : apiKey_(std::move(apiKey)),
        accessToken_(std::move(accessToken)) {
    }

    std::vector<Candle> getHistoricalCandles(
        long instrumentToken,
        const std::string& interval,
        const std::string& from,
        const std::string& to)
    {
        std::string url =
            "https://api.kite.trade/instruments/historical/" +
            std::to_string(instrumentToken) + "/" + interval;

        auto response = cpr::Get(
            cpr::Url{ url },
            cpr::Header{
                {"X-Kite-Version", "3"},
                {"Authorization", "token " + apiKey_ + ":" + accessToken_}
            },
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

        std::vector<Candle> candles;

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
};