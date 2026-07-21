#pragma once

#include "Products/Equity/Options/OptionTypes.h"

namespace automated_trading::analytics::risk
{
    struct EuropeanOptionRiskInput
    {
        products::equity::options::OptionRight optionRight{
            products::equity::options::OptionRight::Call};
        double spot{0.0};
        double strike{0.0};
        double riskFreeRate{0.0};
        double dividendYield{0.0};
        double volatility{0.0};
        double timeToExpiryYears{0.0};
        double positionScale{1.0};
    };

    struct GreekVector
    {
        double delta{0.0};
        double gamma{0.0};
        double vega{0.0};
        double theta{0.0};
        double rho{0.0};
    };

    struct RiskResult
    {
        double unitPrice{0.0};
        double presentValue{0.0};
        GreekVector unitGreeks;
        GreekVector positionGreeks;
    };
}
