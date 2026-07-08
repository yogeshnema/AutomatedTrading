#include "ImpliedVolService.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace
{
    double normalCdf(double value)
    {
        return 0.5 * std::erfc(-value / std::sqrt(2.0));
    }
}

double ImpliedVolService::calculate(
    OptionType optionType,
    double optionPrice,
    double underlyingPrice,
    double strike,
    double yearsToExpiry,
    double riskFreeRate) const
{
    if (optionPrice <= 0.0 || underlyingPrice <= 0.0 || strike <= 0.0 || yearsToExpiry <= 0.0) {
        throw std::invalid_argument("Invalid option inputs for implied volatility calculation.");
    }

    double lowVol = 0.0001;
    double highVol = 1.0;

    const double lowPrice = blackScholesPrice(
        optionType,
        underlyingPrice,
        strike,
        yearsToExpiry,
        riskFreeRate,
        lowVol);
    const double highPrice = blackScholesPrice(
        optionType,
        underlyingPrice,
        strike,
        yearsToExpiry,
        riskFreeRate,
        highVol);

    if (optionPrice < lowPrice || optionPrice > highPrice) {
        std::ostringstream message;
        message << "Option price " << optionPrice
            << " is outside Black-Scholes price range ["
            << lowPrice << ", " << highPrice
            << "] for S=" << underlyingPrice
            << ", K=" << strike
            << ", T=" << yearsToExpiry;
        throw std::runtime_error(message.str());
    }

    for (int iteration = 0; iteration < 100; ++iteration) {
        const double midVol = (lowVol + highVol) / 2.0;
        const double modelPrice = blackScholesPrice(
            optionType,
            underlyingPrice,
            strike,
            yearsToExpiry,
            riskFreeRate,
            midVol);

        if (modelPrice > optionPrice) {
            highVol = midVol;
        }
        else {
            lowVol = midVol;
        }
    }

    return (lowVol + highVol) / 2.0;
}

double ImpliedVolService::blackScholesPrice(
    OptionType optionType,
    double underlyingPrice,
    double strike,
    double yearsToExpiry,
    double riskFreeRate,
    double volatility) const
{
    const double sqrtTime = std::sqrt(yearsToExpiry);
    const double d1 = (
        std::log(underlyingPrice / strike) +
        (riskFreeRate + 0.5 * volatility * volatility) * yearsToExpiry) /
        (volatility * sqrtTime);
    const double d2 = d1 - volatility * sqrtTime;

    if (optionType == OptionType::Call) {
        return underlyingPrice * normalCdf(d1) -
            strike * std::exp(-riskFreeRate * yearsToExpiry) * normalCdf(d2);
    }

    return strike * std::exp(-riskFreeRate * yearsToExpiry) * normalCdf(-d2) -
        underlyingPrice * normalCdf(-d1);
}
