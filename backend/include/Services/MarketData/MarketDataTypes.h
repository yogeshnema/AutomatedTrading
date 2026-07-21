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
        double lotSize{1.0};
        std::string optionType;
        bool subscribed{};
        std::optional<std::string> lastRefreshAt;
        std::optional<std::string> lastError;
    };

    struct InstrumentFacets
    {
        std::vector<std::string> names;
        std::vector<std::string> expiries;
        std::vector<double> strikes;
        std::vector<std::string> optionTypes;
    };

    struct ReferenceSeriesRecord
    {
        std::string seriesCode;
        std::string seriesType;
        std::string displayName;
        std::string provider;
        std::optional<std::string> currency;
        std::optional<std::string> tenor;
        std::string acquisitionMode;
        std::optional<std::string> description;
        bool subscribed{};
        std::optional<std::string> status;
        std::optional<std::string> lastObservedAt;
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

    struct CandleRecord
    {
        std::string timestamp;
        double open{};
        double high{};
        double low{};
        double close{};
        std::int64_t volume{};
    };

    struct MarketDataSummary
    {
        std::string date;
        std::optional<double> open;
        std::optional<double> high;
        std::optional<double> low;
        std::optional<double> close;
        std::optional<double> absoluteChange;
        std::optional<double> percentChange;
        std::int64_t volume{};
        std::size_t minuteCandleCount{};
        std::optional<std::string> firstCandleAt;
        std::optional<std::string> lastCandleAt;
        bool eodPersisted{};
    };

    struct InstrumentMarketData
    {
        InstrumentRecord instrument;
        std::vector<CandleRecord> minuteCandles;
        std::optional<CandleRecord> eodCandle;
        MarketDataSummary summary;
    };
}
