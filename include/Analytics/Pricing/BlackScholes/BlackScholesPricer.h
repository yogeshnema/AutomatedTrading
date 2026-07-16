#pragma once

#include "Analytics/Pricing/IPricingModel.h"

namespace automated_trading::analytics::pricing::black_scholes
{
    class BlackScholesPricer final : public IPricingModel
    {
    public:
        std::string_view name() const noexcept override;
        bool supports(const products::IProduct& product) const noexcept override;
        PricingResult price(
            const products::IProduct& product,
            const MarketContext& marketContext,
            double quantity = 1.0) const override;

    private:
        static double normalCdf(double value) noexcept;
        static double normalPdf(double value) noexcept;
    };
}
