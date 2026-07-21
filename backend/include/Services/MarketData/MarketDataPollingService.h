#pragma once

#include "MarketData/KiteInstrumentService.h"
#include "MarketData/KiteMarketDataService.h"
#include "Services/MarketData/MarketDataRepository.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace automated_trading::services::market_data
{
    class MarketDataPollingService
    {
    public:
        MarketDataPollingService(KiteInstrumentService& instruments, KiteMarketDataService& candles,
                                 MarketDataRepository& repository, std::chrono::seconds interval);
        ~MarketDataPollingService();
        void start();
        void stop();
        bool running() const noexcept;
        void refreshInstrumentMaster();

    private:
        void run();
        void pollOnce();
        KiteInstrumentService& instruments_;
        KiteMarketDataService& candles_;
        MarketDataRepository& repository_;
        std::chrono::seconds interval_;
        std::atomic_bool running_{false};
        std::thread worker_;
    };
}
