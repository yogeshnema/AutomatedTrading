#include "Products/Equity/Options/Payoffs/VanillaPayoff.h"

#include <algorithm>

namespace automated_trading::products::equity::options::payoffs
{
    std::string_view VanillaPayoff::type() const noexcept
    {
        return "vanilla";
    }

    double VanillaPayoff::evaluate(
        double underlyingPrice,
        double strike,
        OptionRight right) const
    {
        return right == OptionRight::Call
            ? std::max(underlyingPrice - strike, 0.0)
            : std::max(strike - underlyingPrice, 0.0);
    }
}
