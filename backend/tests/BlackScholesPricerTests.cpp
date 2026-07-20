#include "Analytics/Pricing/BlackScholes/BlackScholesPricer.h"
#include "Products/Equity/Options/European/EuropeanVanillaOption.h"

#include <cassert>
#include <chrono>
#include <cmath>

namespace
{
    bool closeTo(double actual, double expected, double tolerance)
    {
        return std::abs(actual - expected) <= tolerance;
    }
}

int main()
{
    using namespace std::chrono;
    using automated_trading::analytics::pricing::EquityMarketData;
    using automated_trading::analytics::pricing::MarketContext;
    using automated_trading::analytics::pricing::black_scholes::BlackScholesPricer;
    using automated_trading::products::equity::options::OptionRight;
    using automated_trading::products::equity::options::european::EuropeanVanillaOption;
    using automated_trading::products::equity::options::european::EuropeanVanillaOptionTerms;

    const sys_seconds valuationTime{seconds{0}};
    const auto expiry = valuationTime + days{365};
    EuropeanVanillaOption option(EuropeanVanillaOptionTerms{
        "test-call", "test-equity", OptionRight::Call, 100.0, expiry, 1.0});

    MarketContext market(valuationTime);
    market.setEquityData("test-equity", EquityMarketData{100.0, 0.05, 0.0, 0.20});

    const BlackScholesPricer pricer;
    const auto result = pricer.price(option, market);

    assert(pricer.supports(option));
    assert(closeTo(result.unitPrice, 10.4506, 0.0002));
    assert(closeTo(result.delta, 0.6368, 0.0002));
    assert(closeTo(result.gamma, 0.01876, 0.00002));
    assert(closeTo(result.vega, 37.5240, 0.001));
    assert(closeTo(result.rho, 53.2325, 0.001));
    return 0;
}
