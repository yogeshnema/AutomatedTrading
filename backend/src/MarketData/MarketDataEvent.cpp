#include "MarketData/MarketDataEvent.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace
{
    std::string utcNow()
    {
        const auto now = std::chrono::system_clock::now();
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch());
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(milliseconds);
        const auto time = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(seconds));

        std::tm utc{};
        gmtime_s(&utc, &time);

        std::ostringstream output;
        output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
            << '.' << std::setw(3) << std::setfill('0')
            << (milliseconds.count() % 1000) << 'Z';
        return output.str();
    }

    std::string nextEventId()
    {
        static std::atomic<std::uint64_t> sequence{ 0 };
        return utcNow() + "-" + std::to_string(++sequence);
    }
}

std::string MarketDataEvent::topic(const std::string& prefix) const
{
    return prefix + "." + provider + "." + instrument + "." + interval;
}

std::string MarketDataEvent::toJson() const
{
    return nlohmann::json{
        {"schema_version", schemaVersion},
        {"event_id", eventId},
        {"provider", provider},
        {"instrument", instrument},
        {"interval", interval},
        {"event_timestamp", eventTimestamp},
        {"ingestion_timestamp", ingestionTimestamp},
        {"historical", historical},
        {"open", candle.open},
        {"high", candle.high},
        {"low", candle.low},
        {"close", candle.close},
        {"volume", candle.volume}
    }.dump();
}

MarketDataEvent MarketDataEvent::fromJson(const std::string& payload)
{
    const auto json = nlohmann::json::parse(payload);
    return MarketDataEvent{
        json.at("schema_version").get<int>(),
        json.at("event_id").get<std::string>(),
        json.at("provider").get<std::string>(),
        json.at("instrument").get<std::string>(),
        json.at("interval").get<std::string>(),
        json.at("event_timestamp").get<std::string>(),
        json.at("ingestion_timestamp").get<std::string>(),
        json.value("historical", true),
        Candle{
            json.at("event_timestamp").get<std::string>(),
            json.at("open").get<double>(),
            json.at("high").get<double>(),
            json.at("low").get<double>(),
            json.at("close").get<double>(),
            json.at("volume").get<long>()
        }
    };
}

MarketDataEvent MarketDataEvent::fromCandle(
    const std::string& provider,
    const std::string& instrument,
    const std::string& interval,
    const Candle& candle)
{
    return MarketDataEvent{
        1,
        nextEventId(),
        provider,
        instrument,
        interval,
        candle.timestamp,
        utcNow(),
        true,
        candle
    };
}
