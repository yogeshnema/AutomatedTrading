#include "Products/Equity/Options/European/EuropeanVanillaOption.h"

#include "Products/Equity/Options/Exercise/EuropeanExercise.h"
#include "Products/Equity/Options/Payoffs/VanillaPayoff.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace automated_trading::products::equity::options::european
{
    EuropeanVanillaOption::EuropeanVanillaOption(EuropeanVanillaOptionTerms terms)
        : terms_(std::move(terms)),
          payoff_(std::make_shared<payoffs::VanillaPayoff>()),
          exerciseRule_(std::make_shared<exercise::EuropeanExercise>(terms_.expiry))
    {
        validate();
    }

    std::string_view EuropeanVanillaOption::id() const noexcept { return terms_.productId; }
    std::string_view EuropeanVanillaOption::family() const noexcept { return "equity_option"; }
    std::chrono::sys_seconds EuropeanVanillaOption::maturity() const noexcept { return terms_.expiry; }
    double EuropeanVanillaOption::contractMultiplier() const noexcept { return terms_.multiplier; }
    std::string_view EuropeanVanillaOption::underlyingAssetId() const noexcept { return terms_.underlyingAssetId; }
    OptionRight EuropeanVanillaOption::optionRight() const noexcept { return terms_.right; }
    double EuropeanVanillaOption::strike() const noexcept { return terms_.strike; }
    const payoffs::IPayoff& EuropeanVanillaOption::payoff() const noexcept { return *payoff_; }
    const exercise::IExerciseRule& EuropeanVanillaOption::exerciseRule() const noexcept { return *exerciseRule_; }

    void EuropeanVanillaOption::validate() const
    {
        if (terms_.productId.empty()) throw std::invalid_argument("Product ID must not be empty.");
        if (terms_.underlyingAssetId.empty()) throw std::invalid_argument("Underlying asset ID must not be empty.");
        if (!std::isfinite(terms_.strike) || terms_.strike <= 0.0) throw std::invalid_argument("Strike must be positive and finite.");
        if (!std::isfinite(terms_.multiplier) || terms_.multiplier <= 0.0) throw std::invalid_argument("Multiplier must be positive and finite.");
    }
}
