#pragma once

#include "Products/Equity/Options/Payoffs/IPayoff.h"

namespace automated_trading::products::equity::options::payoffs
{
    class VanillaPayoff final : public IPayoff
    {
    public:
        std::string_view type() const noexcept override;
        double evaluate(
            double underlyingPrice,
            double strike,
            OptionRight right) const override;
    };
}
