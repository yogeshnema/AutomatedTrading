#include "Services/Pricing/PricingApplicationService.h"

#include "Products/Equity/Options/European/EuropeanVanillaOption.h"

#include <stdexcept>
#include <string>

namespace automated_trading::services::pricing
{
    PricingApplicationService::PricingApplicationService(
        ITradePricingRepository& repository,
        const analytics::pricing::IPricingModel& pricingModel)
        : repository_(repository), pricingModel_(pricingModel)
    {
    }

    TradePricingOutcome PricingApplicationService::price(
        std::string_view tradeId,
        std::string_view valuationDate) const
    {
        if (tradeId.empty()) throw std::invalid_argument("tradeId must not be empty.");
        if (valuationDate.empty()) throw std::invalid_argument("valuationDate must not be empty.");

        const auto input = repository_.load(tradeId, valuationDate);
        products::equity::options::european::EuropeanVanillaOption product({
            input.productId,
            input.underlyingAssetId,
            input.optionRight,
            input.strike,
            input.expiry,
            input.contractMultiplier});

        analytics::pricing::MarketContext marketContext(input.valuationTime);
        marketContext.setEquityData(input.underlyingAssetId, input.marketData);

        TradePricingOutcome outcome;
        outcome.tradeId = input.tradeId;
        outcome.valuationDate = std::string(valuationDate);
        outcome.productType = "european_vanilla_option";
        outcome.underlyingAssetId = input.underlyingAssetId;
        outcome.currency = input.currency;
        outcome.signedQuantity = input.signedQuantity;
        outcome.contractMultiplier = input.contractMultiplier;
        outcome.spot = input.marketData.spot;
        outcome.strike = input.strike;
        outcome.riskFreeRate = input.marketData.riskFreeRate;
        outcome.dividendYield = input.marketData.dividendYield;
        outcome.volatility = input.marketData.volatility;
        outcome.result = pricingModel_.price(product, marketContext, input.signedQuantity);
        return outcome;
    }

    bool PricingApplicationService::isReady() const
    {
        return repository_.isReady();
    }
}
