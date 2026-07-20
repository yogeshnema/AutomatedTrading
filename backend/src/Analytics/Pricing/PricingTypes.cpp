#include "Analytics/Pricing/PricingTypes.h"

#include <cmath>
#include <utility>

namespace automated_trading::analytics::pricing
{
    MarketContext::MarketContext(std::chrono::sys_seconds valuationTime)
        : valuationTime_(valuationTime)
    {
    }

    std::chrono::sys_seconds MarketContext::valuationTime() const noexcept
    {
        return valuationTime_;
    }

    void MarketContext::setEquityData(std::string assetId, EquityMarketData data)
    {
        if (assetId.empty()) throw PricingError("Asset ID must not be empty.");
        if (!std::isfinite(data.spot) || data.spot <= 0.0) throw PricingError("Spot must be positive and finite.");
        if (!std::isfinite(data.volatility) || data.volatility < 0.0) throw PricingError("Volatility must be non-negative and finite.");
        if (!std::isfinite(data.riskFreeRate) || !std::isfinite(data.dividendYield)) throw PricingError("Rates must be finite.");
        equityData_.insert_or_assign(std::move(assetId), data);
    }

    const EquityMarketData& MarketContext::equityData(std::string_view assetId) const
    {
        const auto iterator = equityData_.find(std::string(assetId));
        if (iterator == equityData_.end()) {
            throw PricingError("No equity market data found for asset '" + std::string(assetId) + "'.");
        }
        return iterator->second;
    }
}
