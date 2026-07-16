#include "Analytics/Pricing/BlackScholes/BlackScholesPricer.h"

#include "Products/Equity/Options/IOptionProduct.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace automated_trading::analytics::pricing::black_scholes
{
    namespace
    {
        constexpr double SecondsPerYear = 365.0 * 24.0 * 60.0 * 60.0;
    }

    std::string_view BlackScholesPricer::name() const noexcept
    {
        return "black_scholes";
    }

    bool BlackScholesPricer::supports(const products::IProduct& product) const noexcept
    {
        const auto* option = dynamic_cast<const products::equity::options::IOptionProduct*>(&product);
        return option != nullptr &&
            option->exerciseRule().style() == products::equity::options::ExerciseStyle::European &&
            option->payoff().type() == "vanilla";
    }

    PricingResult BlackScholesPricer::price(
        const products::IProduct& product,
        const MarketContext& marketContext,
        double quantity) const
    {
        namespace option_types = products::equity::options;

        product.validate();
        if (!std::isfinite(quantity)) throw PricingError("Quantity must be finite.");

        const auto* option = dynamic_cast<const option_types::IOptionProduct*>(&product);
        if (option == nullptr || !supports(product)) {
            throw PricingError("Black-Scholes supports European vanilla equity options only.");
        }

        const auto& market = marketContext.equityData(option->underlyingAssetId());
        const double spot = market.spot;
        const double strike = option->strike();
        const double rate = market.riskFreeRate;
        const double dividendYield = market.dividendYield;
        const double volatility = market.volatility;
        const double secondsToExpiry = std::chrono::duration<double>(
            option->maturity() - marketContext.valuationTime()).count();
        const double timeToExpiry = std::max(secondsToExpiry / SecondsPerYear, 0.0);

        PricingResult result;
        result.model = std::string(name());

        if (timeToExpiry == 0.0 || volatility == 0.0) {
            const double discountedSpot = spot * std::exp(-dividendYield * timeToExpiry);
            const double discountedStrike = strike * std::exp(-rate * timeToExpiry);
            result.unitPrice = option->payoff().evaluate(discountedSpot, discountedStrike, option->optionRight());
            result.presentValue = result.unitPrice * quantity * product.contractMultiplier();
            return result;
        }

        const double sqrtTime = std::sqrt(timeToExpiry);
        const double d1 = (std::log(spot / strike) +
            (rate - dividendYield + 0.5 * volatility * volatility) * timeToExpiry) /
            (volatility * sqrtTime);
        const double d2 = d1 - volatility * sqrtTime;
        const double discountRate = std::exp(-rate * timeToExpiry);
        const double discountDividend = std::exp(-dividendYield * timeToExpiry);
        const bool isCall = option->optionRight() == option_types::OptionRight::Call;

        if (isCall) {
            result.unitPrice = spot * discountDividend * normalCdf(d1) - strike * discountRate * normalCdf(d2);
            result.delta = discountDividend * normalCdf(d1);
            result.theta = -(spot * discountDividend * normalPdf(d1) * volatility) / (2.0 * sqrtTime)
                - rate * strike * discountRate * normalCdf(d2)
                + dividendYield * spot * discountDividend * normalCdf(d1);
            result.rho = strike * timeToExpiry * discountRate * normalCdf(d2);
        }
        else {
            result.unitPrice = strike * discountRate * normalCdf(-d2) - spot * discountDividend * normalCdf(-d1);
            result.delta = discountDividend * (normalCdf(d1) - 1.0);
            result.theta = -(spot * discountDividend * normalPdf(d1) * volatility) / (2.0 * sqrtTime)
                + rate * strike * discountRate * normalCdf(-d2)
                - dividendYield * spot * discountDividend * normalCdf(-d1);
            result.rho = -strike * timeToExpiry * discountRate * normalCdf(-d2);
        }

        result.gamma = discountDividend * normalPdf(d1) / (spot * volatility * sqrtTime);
        result.vega = spot * discountDividend * normalPdf(d1) * sqrtTime;
        result.presentValue = result.unitPrice * quantity * product.contractMultiplier();
        return result;
    }

    double BlackScholesPricer::normalCdf(double value) noexcept
    {
        return 0.5 * std::erfc(-value / std::numbers::sqrt2);
    }

    double BlackScholesPricer::normalPdf(double value) noexcept
    {
        return std::exp(-0.5 * value * value) / std::sqrt(2.0 * std::numbers::pi);
    }
}
