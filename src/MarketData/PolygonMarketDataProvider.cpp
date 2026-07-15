#include "MarketData/PolygonMarketDataProvider.h"

#include <chrono>
#include <cpr/cpr.h>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{
    struct PolygonInterval
    {
        int multiplier;
        std::string timespan;
    };

    PolygonInterval parseInterval(const std::string& interval)
    {
        const auto separator = interval.find_first_of(" _-");
        if (separator != std::string::npos) {
            const int multiplier = std::stoi(interval.substr(0, separator));
            const std::string timespan = interval.substr(separator + 1);
            if (multiplier <= 0 || timespan.empty()) {
                throw std::invalid_argument("Invalid Polygon interval: " + interval);
            }
            return { multiplier, timespan };
        }

        std::size_t digitCount = 0;
        while (digitCount < interval.size() &&
            interval[digitCount] >= '0' && interval[digitCount] <= '9') {
            ++digitCount;
        }

        if (digitCount > 0) {
            const int multiplier = std::stoi(interval.substr(0, digitCount));
            const std::string timespan = interval.substr(digitCount);
            if (multiplier <= 0 || timespan.empty()) {
                throw std::invalid_argument("Invalid Polygon interval: " + interval);
            }
            return { multiplier, timespan };
        }

        return { 1, interval };
    }

    std::string isoUtcTimestamp(std::int64_t milliseconds)
    {
        const auto time = static_cast<std::time_t>(milliseconds / 1000);
        std::tm utc{};
        gmtime_s(&utc, &time);

        std::ostringstream output;
        output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
            << '.' << std::setw(3) << std::setfill('0') << (milliseconds % 1000)
            << 'Z';
        return output.str();
    }
}

PolygonMarketDataProvider::PolygonMarketDataProvider(const std::string& configPath)
    : IMarketDataProvider("polygon")
{
    std::ifstream input(configPath);
    if (!input) {
        throw std::runtime_error("Unable to open config file: " + configPath);
    }

    const auto config = nlohmann::json::parse(input);
    apiKey_ = config.at("polygon").at("api_key").get<std::string>();
    if (apiKey_.empty()) {
        throw std::runtime_error("Polygon API key is missing.");
    }
}

std::vector<Candle> PolygonMarketDataProvider::getHistoricalCandles(
    const std::string& instrument,
    const std::string& from,
    const std::string& to,
    const std::string& interval)
{
    if (instrument.empty()) {
        throw std::invalid_argument("Polygon ticker is missing.");
    }

    const auto parsedInterval = parseInterval(interval);
    const std::string url =
        "https://api.polygon.io/v2/aggs/ticker/" + instrument +
        "/range/" + std::to_string(parsedInterval.multiplier) +
        "/" + parsedInterval.timespan + "/" + from + "/" + to;

    const auto response = cpr::Get(
        cpr::Url{ url },
        cpr::Parameters{
            {"adjusted", "true"},
            {"sort", "asc"},
            {"limit", "50000"},
            {"apiKey", apiKey_}
        });

    if (response.status_code != 200) {
        throw std::runtime_error(
            "Polygon API error: HTTP " + std::to_string(response.status_code) +
            "\n" + response.text);
    }

    const auto json = nlohmann::json::parse(response.text);
    if (json.value("status", std::string()) == "ERROR") {
        throw std::runtime_error(
            "Polygon API error: " + json.value("error", std::string("unknown error")));
    }

    std::vector<Candle> candles;
    for (const auto& row : json.value("results", nlohmann::json::array())) {
        candles.push_back(Candle{
            isoUtcTimestamp(row.at("t").get<std::int64_t>()),
            row.at("o").get<double>(),
            row.at("h").get<double>(),
            row.at("l").get<double>(),
            row.at("c").get<double>(),
            static_cast<long>(row.at("v").get<double>())
        });
    }

    return candles;
}
