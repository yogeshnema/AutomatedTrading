#include "OptionChainService.h"

#include <algorithm>
#include <cmath>

std::vector<OptionInstrument> OptionChainService::getNiftyOptionsForExpiry(
    const std::vector<OptionInstrument>& instruments,
    const std::string& expiry) const
{
    std::vector<OptionInstrument> result;

    for (const auto& instrument : instruments) {
        if (instrument.name == "NIFTY" && instrument.expiry == expiry) {
            result.push_back(instrument);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const OptionInstrument& lhs, const OptionInstrument& rhs) {
            if (lhs.strike == rhs.strike) {
                return static_cast<int>(lhs.optionType) < static_cast<int>(rhs.optionType);
            }

            return lhs.strike < rhs.strike;
        });

    return result;
}
std::vector<OptionInstrument> OptionChainService::getStrikesAroundSpot(
    const std::vector<OptionInstrument>& optionChain,
    double spotPrice,
    int strikesEachSide) const
{
    std::vector<double> strikes;
    for (const auto& option : optionChain) {
        if (std::find(strikes.begin(), strikes.end(), option.strike) == strikes.end()) {
            strikes.push_back(option.strike);
        }
    }

    std::sort(strikes.begin(), strikes.end(),
        [spotPrice](double lhs, double rhs) {
            return std::abs(lhs - spotPrice) < std::abs(rhs - spotPrice);
        });

    const auto strikeCount = std::min<int>(
        static_cast<int>(strikes.size()),
        (strikesEachSide * 2) + 1);

    strikes.resize(strikeCount);

    std::vector<OptionInstrument> result;
    for (const auto& option : optionChain) {
        if (std::find(strikes.begin(), strikes.end(), option.strike) != strikes.end()) {
            result.push_back(option);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const OptionInstrument& lhs, const OptionInstrument& rhs) {
            if (lhs.strike == rhs.strike) {
                return static_cast<int>(lhs.optionType) < static_cast<int>(rhs.optionType);
            }

            return lhs.strike < rhs.strike;
        });

    return result;
}
