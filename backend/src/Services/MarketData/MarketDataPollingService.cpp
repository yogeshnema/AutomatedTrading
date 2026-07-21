#include "Services/MarketData/MarketDataPollingService.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace automated_trading::services::market_data
{
    namespace
    {
        std::string localTime(std::chrono::system_clock::time_point value)
        {
            const auto raw = std::chrono::system_clock::to_time_t(value);
            std::tm time{};
#ifdef _WIN32
            localtime_s(&time, &raw);
#else
            localtime_r(&raw, &time);
#endif
            std::ostringstream output;
            output << std::put_time(&time, "%Y-%m-%d %H:%M:%S");
            return output.str();
        }
    }

    MarketDataPollingService::MarketDataPollingService(
        KiteInstrumentService& instruments, KiteMarketDataService& candles,
        MarketDataRepository& repository, std::chrono::seconds interval)
        : instruments_(instruments), candles_(candles), repository_(repository), interval_(interval) {}

    MarketDataPollingService::~MarketDataPollingService() { stop(); }
    bool MarketDataPollingService::running() const noexcept { return running_; }

    void MarketDataPollingService::start()
    {
        if (running_.exchange(true)) return;
        worker_ = std::thread([this] { run(); });
    }

    void MarketDataPollingService::stop()
    {
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }

    void MarketDataPollingService::refreshInstrumentMaster()
    {
        const auto values = instruments_.getNiftyOptionInstruments();
        repository_.replaceNiftyOptionMaster(values);
        std::cout << "[market-data] instrument master refreshed: " << values.size() << " NIFTY options\n";
    }

    void MarketDataPollingService::run()
    {
        auto nextInstrumentRefresh = std::chrono::steady_clock::now();
        while (running_) {
            const auto started = std::chrono::steady_clock::now();
            if (started >= nextInstrumentRefresh) {
                try {
                    refreshInstrumentMaster();
                    nextInstrumentRefresh = started + std::chrono::hours(24);
                }
                catch (const std::exception& error) {
                    std::cerr << "[market-data] instrument refresh failed: " << error.what() << '\n';
                    nextInstrumentRefresh = started + std::chrono::minutes(5);
                }
            }
            pollOnce();
            while (running_ && std::chrono::steady_clock::now() - started < interval_)
                std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void MarketDataPollingService::pollOnce()
    {
        const auto now = std::chrono::system_clock::now();
        const auto from = now - std::chrono::minutes(5);
        for (const auto& subscription : repository_.subscriptions()) {
            try {
                const auto values = candles_.getHistoricalCandles(
                    static_cast<long>(subscription.instrumentToken), localTime(from), localTime(now), subscription.interval);
                repository_.saveCandles(subscription.instrumentToken, subscription.interval, values);
                std::cout << "[market-data] " << subscription.tradingSymbol << " candles=" << values.size() << '\n';
            }
            catch (const std::exception& error) {
                repository_.markRefreshFailure(subscription.instrumentToken, error.what());
                std::cerr << "[market-data] " << subscription.tradingSymbol << " failed: " << error.what() << '\n';
            }
        }
    }
}
