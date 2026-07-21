#include "Services/MarketData/MarketDataPollingService.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace automated_trading::services::market_data
{
    namespace
    {
        struct LocalMarketClock
        {
            std::string date;
            int minuteOfDay{};
            bool weekday{};
        };

        std::tm localTm(std::chrono::system_clock::time_point value)
        {
            const auto raw = std::chrono::system_clock::to_time_t(value);
            std::tm time{};
#ifdef _WIN32
            localtime_s(&time, &raw);
#else
            localtime_r(&raw, &time);
#endif
            return time;
        }

        std::string localTime(std::chrono::system_clock::time_point value)
        {
            const auto time = localTm(value);
            std::ostringstream output;
            output << std::put_time(&time, "%Y-%m-%d %H:%M:%S");
            return output.str();
        }

        LocalMarketClock marketClock(std::chrono::system_clock::time_point value)
        {
            const auto time = localTm(value);
            std::ostringstream date;
            date << std::put_time(&time, "%Y-%m-%d");
            return {date.str(), time.tm_hour * 60 + time.tm_min, time.tm_wday >= 1 && time.tm_wday <= 5};
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
        const auto clock = marketClock(now);
        constexpr int MarketOpen = 9 * 60 + 15;
        constexpr int MarketClose = 15 * 60 + 30;
        constexpr int EodReady = 15 * 60 + 35;
        if (!clock.weekday || clock.minuteOfDay < MarketOpen) return;

        for (const auto& subscription : repository_.subscriptions()) {
            try {
                if (clock.minuteOfDay <= MarketClose) {
                    const auto from = repository_.hasMinuteCandle(subscription.instrumentToken, clock.date)
                        ? localTime(now - std::chrono::minutes(5))
                        : clock.date + " 09:15:00";
                    const auto values = candles_.getHistoricalCandles(
                        static_cast<long>(subscription.instrumentToken), from, localTime(now), "minute");
                    repository_.saveCandles(subscription.instrumentToken, "minute", values);
                    std::cout << "[market-data] " << subscription.tradingSymbol << " minute candles=" << values.size() << '\n';
                }
                else if (clock.minuteOfDay >= EodReady) {
                    // The daily candle acts as the reconciliation marker. Until it exists,
                    // re-request the complete minute session so a restart or outage cannot
                    // leave gaps in the history used by strategies and backtests.
                    if (!repository_.hasDailyCandle(subscription.instrumentToken, clock.date)) {
                        const auto values = candles_.getHistoricalCandles(
                            static_cast<long>(subscription.instrumentToken), clock.date + " 09:15:00",
                            clock.date + " 15:30:00", "minute");
                        repository_.saveCandles(subscription.instrumentToken, "minute", values);
                        std::this_thread::sleep_for(std::chrono::milliseconds(350));

                        const auto daily = candles_.getHistoricalCandles(
                            static_cast<long>(subscription.instrumentToken), clock.date + " 00:00:00",
                            clock.date + " 23:59:59", "day");
                        repository_.saveCandles(subscription.instrumentToken, "day", daily);
                        std::cout << "[market-data] " << subscription.tradingSymbol << " EOD reconciled\n";
                    }
                }
            }
            catch (const std::exception& error) {
                repository_.markRefreshFailure(subscription.instrumentToken, error.what());
                std::cerr << "[market-data] " << subscription.tradingSymbol << " failed: " << error.what() << '\n';
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
        }
    }
}
