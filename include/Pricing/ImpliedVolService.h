#pragma once

#include "Common/VolArbitrageModels.h"

class ImpliedVolService
{
public:
    double calculate(
        OptionType optionType,
        double optionPrice,
        double underlyingPrice,
        double strike,
        double yearsToExpiry,
        double riskFreeRate) const;

private:
    double blackScholesPrice(
        OptionType optionType,
        double underlyingPrice,
        double strike,
        double yearsToExpiry,
        double riskFreeRate,
        double volatility) const;
};
