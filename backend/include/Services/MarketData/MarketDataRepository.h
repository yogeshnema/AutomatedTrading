#pragma once

#include "Common/VolArbitrageModels.h"
#include "Database/QueryService.h"
#include "MarketData/IMarketDataProvider.h"
#include "Services/MarketData/MarketDataTypes.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace automated_trading::services::market_data
{
    class MarketDataRepository
    {
    public:
        explicit MarketDataRepository(database::QueryService& queries);

        void replaceNiftyOptionMaster(const std::vector<OptionInstrument>& instruments);
        std::vector<InstrumentRecord> searchInstruments(
            const std::string& search,
            const std::string& expiry,
            const std::string& optionType,
            int limit);
        std::vector<SubscriptionRecord> subscriptions();
        SubscriptionRecord subscribe(std::int64_t instrumentToken, const std::string& interval);
        void unsubscribe(std::int64_t instrumentToken);
        void saveCandles(std::int64_t instrumentToken, const std::string& interval, const std::vector<Candle>& candles);
        void markRefreshFailure(std::int64_t instrumentToken, const std::string& error);
        InstrumentMarketData instrumentMarketData(std::int64_t instrumentToken, const std::string& date);
        bool hasMinuteCandle(std::int64_t instrumentToken, const std::string& date);
        bool hasDailyCandle(std::int64_t instrumentToken, const std::string& date);
        ServiceStatus status(bool workerRunning);
        bool isReady();

    private:
        database::QueryService& queries_;
        std::mutex mutex_;
    };
}
