#pragma once

#include "Products/Equity/Options/OptionTypes.h"

#include <string_view>

namespace automated_trading::products::equity::options::payoffs
{
    class IPayoff
    {
    public:
        virtual ~IPayoff() = default;

        virtual std::string_view type() const noexcept = 0;
        virtual double evaluate(
            double underlyingPrice,
            double strike,
            OptionRight right) const = 0;
    };
}
