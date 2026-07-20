#pragma once

#include "MarketData/IMarketDataProvider.h"

#include <cstdint>
#include <string>

struct MarketDataEvent
{
    int schemaVersion = 1;
    std::string eventId;
    std::string provider;
    std::string instrument;
    std::string interval;
    std::string eventTimestamp;
    std::string ingestionTimestamp;
    bool historical = true;
    Candle candle{};

    std::string topic(const std::string& prefix = "market.candle") const;
    std::string toJson() const;
    static MarketDataEvent fromJson(const std::string& payload);
    static MarketDataEvent fromCandle(
        const std::string& provider,
        const std::string& instrument,
        const std::string& interval,
        const Candle& candle);
};
