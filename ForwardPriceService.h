#pragma once

#include "VolArbitrageModels.h"

#include <optional>
#include <vector>

class ForwardPriceService
{
public:
    std::optional<ForwardPrice> inferFromPutCallParity(
        const std::vector<OptionInstrument>& instruments,
        const std::vector<OptionQuote>& quotes,
        double referenceSpotPrice,
        double yearsToExpiry,
        double riskFreeRate) const;
};
