#pragma once

#include <string>
#include <memory>
#include <vector>

struct Candle
{
    std::string timestamp;
    double open;
    double high;
    double low;
    double close;
    long volume;
};

class IMarketDataProvider
{
public:
    explicit IMarketDataProvider(std::string providerName);
    virtual ~IMarketDataProvider() = default;

    IMarketDataProvider(const IMarketDataProvider&) = delete;
    IMarketDataProvider& operator=(const IMarketDataProvider&) = delete;

    const std::string& providerName() const noexcept;

    virtual std::vector<Candle> getHistoricalCandles(
        const std::string& instrument,
        const std::string& from,
        const std::string& to,
        const std::string& interval) = 0;

    static std::unique_ptr<IMarketDataProvider> create(
        const std::string& providerName,
        const std::string& configPath);

private:
    std::string providerName_;
};
