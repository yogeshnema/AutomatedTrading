#include "ForwardPriceService.h"

#include <cmath>
#include <limits>
#include <unordered_map>

namespace
{
    double quotePrice(const OptionQuote& quote)
    {
        if (quote.bidPrice > 0.0 && quote.askPrice > 0.0) {
            return (quote.bidPrice + quote.askPrice) / 2.0;
        }

        return quote.lastPrice;
    }

    struct StrikePair
    {
        const OptionInstrument* callInstrument = nullptr;
        const OptionInstrument* putInstrument = nullptr;
        const OptionQuote* callQuote = nullptr;
        const OptionQuote* putQuote = nullptr;
    };
}

std::optional<ForwardPrice> ForwardPriceService::inferFromPutCallParity(
    const std::vector<OptionInstrument>& instruments,
    const std::vector<OptionQuote>& quotes,
    double referenceSpotPrice,
    double yearsToExpiry,
    double riskFreeRate) const
{
    std::unordered_map<long, const OptionQuote*> quotesByToken;
    for (const auto& quote : quotes) {
        quotesByToken[quote.instrumentToken] = &quote;
    }

    std::unordered_map<double, StrikePair> pairsByStrike;
    for (const auto& instrument : instruments) {
        const auto quote = quotesByToken.find(instrument.instrumentToken);
        if (quote == quotesByToken.end()) {
            continue;
        }

        auto& pair = pairsByStrike[instrument.strike];
        if (instrument.optionType == OptionType::Call) {
            pair.callInstrument = &instrument;
            pair.callQuote = quote->second;
        }
        else {
            pair.putInstrument = &instrument;
            pair.putQuote = quote->second;
        }
    }

    std::optional<ForwardPrice> bestForward;
    double bestDistance = std::numeric_limits<double>::max();
    const double discountFactor = std::exp(-riskFreeRate * yearsToExpiry);

    for (const auto& [strike, pair] : pairsByStrike) {
        if (pair.callInstrument == nullptr ||
            pair.putInstrument == nullptr ||
            pair.callQuote == nullptr ||
            pair.putQuote == nullptr) {
            continue;
        }

        const double callPrice = quotePrice(*pair.callQuote);
        const double putPrice = quotePrice(*pair.putQuote);
        if (callPrice <= 0.0 || putPrice <= 0.0) {
            continue;
        }

        const double forwardPrice = strike + std::exp(riskFreeRate * yearsToExpiry) * (callPrice - putPrice);
        const double impliedSpotPrice = forwardPrice * discountFactor;
        const double distance = std::abs(strike - referenceSpotPrice);

        if (distance < bestDistance) {
            bestDistance = distance;
            bestForward = ForwardPrice{
                strike,
                callPrice,
                putPrice,
                forwardPrice,
                impliedSpotPrice };
        }
    }

    return bestForward;
}
