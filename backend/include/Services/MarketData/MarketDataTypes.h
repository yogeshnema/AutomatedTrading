#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace automated_trading::services::market_data
{
    struct InstrumentRecord
    {
        std::int64_t instrumentToken{};
        std::string exchange;
        std::string tradingSymbol;
        std::string name;
        std::string expiry;
        double strike{};
        std::string optionType;
        bool subscribed{};
        std::optional<std::string> lastRefreshAt;
        std::optional<std::string> lastError;
    };

    struct SubscriptionRecord
    {
        std::int64_t instrumentToken{};
        std::string exchange;
        std::string tradingSymbol;
        std::string expiry;
        double strike{};
        std::string optionType;
        std::string interval{"minute"};
        bool enabled{true};
        std::optional<std::string> lastRefreshAt;
        std::optional<std::string> lastCandleAt;
        std::optional<std::string> lastError;
    };

    struct ServiceStatus
    {
        bool databaseReady{};
        bool workerRunning{};
        std::size_t instrumentCount{};
        std::size_t activeSubscriptions{};
        std::optional<std::string> instrumentMasterRefreshedAt;
    };
}
