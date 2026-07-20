#pragma once

#include <chrono>
#include <string_view>

namespace automated_trading::products::equity::options::path_features
{
    struct MarketObservation
    {
        std::chrono::sys_seconds time;
        double underlyingPrice;
    };

    class IPathFeature
    {
    public:
        virtual ~IPathFeature() = default;

        virtual std::string_view type() const noexcept = 0;
        virtual void reset() = 0;
        virtual void observe(const MarketObservation& observation) = 0;
        virtual bool isActive() const noexcept = 0;
    };
}
