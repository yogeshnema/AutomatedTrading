#include "Analytics/Pricing/BlackScholes/BlackScholesPricer.h"
#include "Services/Pricing/PricingApplicationService.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <string_view>

namespace
{
    class SampleRepository final
        : public automated_trading::services::pricing::ITradePricingRepository
    {
    public:
        automated_trading::services::pricing::TradePricingInput load(
            std::string_view tradeId,
            std::string_view) override
        {
            using namespace std::chrono;
            using automated_trading::products::equity::options::OptionRight;
            const sys_seconds valuationTime{seconds{0}};
            return {
                std::string(tradeId),
                "sample-product",
                "sample-asset",
                "USD",
                OptionRight::Call,
                100.0,
                valuationTime + days{365},
                100.0,
                10.0,
                valuationTime,
                {100.0, 0.05, 0.0, 0.20}};
        }

        bool isReady() override { return true; }
    };
}

int main()
{
    SampleRepository repository;
    automated_trading::analytics::pricing::black_scholes::BlackScholesPricer pricer;
    automated_trading::services::pricing::PricingApplicationService service(repository, pricer);

    const auto outcome = service.price(
        "90000000-0000-0000-0000-000000000001",
        "2026-01-01T00:00:00Z");

    assert(service.isReady());
    assert(std::abs(outcome.result.unitPrice - 10.4506) < 0.0002);
    assert(std::abs(outcome.result.presentValue - 10450.6) < 0.2);
    assert(outcome.result.model == "black_scholes");
    return 0;
}
