#include "Analytics/Risk/BlackScholes/AnalyticBlackScholesRiskEngine.h"
#include "Services/Risk/RiskApplicationService.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <string>
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
            return {std::string(tradeId), "sample-product", "sample-asset", "USD",
                OptionRight::Call, 100.0, valuationTime + days{365}, 100.0, 10.0,
                valuationTime, {100.0, 0.05, 0.0, 0.20}};
        }

        bool isReady() override { return true; }
    };
}

int main()
{
    SampleRepository repository;
    automated_trading::analytics::risk::black_scholes::AnalyticBlackScholesRiskEngine engine;
    automated_trading::services::risk::RiskApplicationService service(repository, engine);
    const auto outcome = service.calculate(
        "90000000-0000-0000-0000-000000000001", "2026-01-01T00:00:00Z", "analytic");

    assert(service.isReady());
    assert(outcome.computeBackend == "scalar_cpu");
    assert(std::abs(outcome.result.unitGreeks.delta - 0.6368) < 0.0002);
    assert(std::abs(outcome.result.positionGreeks.delta - 636.8) < 0.2);
    assert(outcome.latency.totalNanoseconds >= outcome.latency.computeNanoseconds);
    return 0;
}
