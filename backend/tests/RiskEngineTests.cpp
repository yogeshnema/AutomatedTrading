#include "Analytics/Risk/BlackScholes/AnalyticBlackScholesRiskEngine.h"

#include <array>
#include <cassert>
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
    using automated_trading::analytics::risk::EuropeanOptionRiskInput;
    using automated_trading::analytics::risk::RiskResult;
    using automated_trading::analytics::risk::black_scholes::AnalyticBlackScholesRiskEngine;
    using automated_trading::products::equity::options::OptionRight;

    const AnalyticBlackScholesRiskEngine engine;
    const EuropeanOptionRiskInput call{
        OptionRight::Call, 100.0, 100.0, 0.05, 0.0, 0.20, 1.0, 10.0};
    const auto result = engine.calculate(call);

    assert(engine.model() == "black_scholes");
    assert(engine.method() == "analytic");
    assert(engine.computeBackend() == "scalar_cpu");
    assert(closeTo(result.unitPrice, 10.4506, 0.0002));
    assert(closeTo(result.unitGreeks.delta, 0.6368, 0.0002));
    assert(closeTo(result.unitGreeks.gamma, 0.01876, 0.00002));
    assert(closeTo(result.unitGreeks.vega, 0.37524, 0.00002));
    assert(closeTo(result.unitGreeks.theta, -0.01757, 0.00002));
    assert(closeTo(result.unitGreeks.rho, 0.005323, 0.000002));
    assert(closeTo(result.positionGreeks.delta, result.unitGreeks.delta * 10.0, 1e-12));
    assert(closeTo(result.presentValue, result.unitPrice * 10.0, 1e-12));

    std::array<EuropeanOptionRiskInput, 2> inputs{call, call};
    inputs[1].optionRight = OptionRight::Put;
    std::array<RiskResult, 2> outputs{};
    engine.calculateBatch(inputs, outputs);
    assert(outputs[0].unitGreeks.delta > 0.0);
    assert(outputs[1].unitGreeks.delta < 0.0);
    return 0;
}
