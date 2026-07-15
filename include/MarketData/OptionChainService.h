#pragma once

#include "Common/VolArbitrageModels.h"

#include <string>
#include <vector>

class OptionChainService
{
public:
    std::vector<OptionInstrument> getNiftyOptionsForExpiry(
        const std::vector<OptionInstrument>& instruments,
        const std::string& expiry) const;

    std::vector<OptionInstrument> getStrikesAroundSpot(
        const std::vector<OptionInstrument>& optionChain,
        double spotPrice,
        int strikesEachSide) const;
};
