#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace automated_trading::analytics::pricing
{
    struct EquityMarketData
    {
        double spot{0.0};
        double riskFreeRate{0.0};
        double dividendYield{0.0};
        double volatility{0.0};
    };

    class MarketContext
    {
    public:
        explicit MarketContext(std::chrono::sys_seconds valuationTime);

        std::chrono::sys_seconds valuationTime() const noexcept;
        void setEquityData(std::string assetId, EquityMarketData data);
        const EquityMarketData& equityData(std::string_view assetId) const;

    private:
        std::chrono::sys_seconds valuationTime_;
        std::unordered_map<std::string, EquityMarketData> equityData_;
    };

    struct PricingResult
    {
        double unitPrice{0.0};
        double presentValue{0.0};
        double delta{0.0};
        double gamma{0.0};
        double vega{0.0};
        double theta{0.0};
        double rho{0.0};
        std::string model;
    };

    class PricingError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };
}
