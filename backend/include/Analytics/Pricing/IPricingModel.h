#pragma once

#include "Analytics/Pricing/PricingTypes.h"
#include "Products/IProduct.h"

#include <string_view>

namespace automated_trading::analytics::pricing
{
    class IPricingModel
    {
    public:
        virtual ~IPricingModel() = default;

        virtual std::string_view name() const noexcept = 0;
        virtual bool supports(const products::IProduct& product) const noexcept = 0;
        virtual PricingResult price(
            const products::IProduct& product,
            const MarketContext& marketContext,
            double quantity = 1.0) const = 0;
    };
}
