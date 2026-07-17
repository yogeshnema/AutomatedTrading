#pragma once

#include "Products/Equity/Options/IOptionProduct.h"

#include <memory>
#include <string>

namespace automated_trading::products::equity::options::european
{
    struct EuropeanVanillaOptionTerms
    {
        std::string productId;
        std::string underlyingAssetId;
        OptionRight right{OptionRight::Call};
        double strike{0.0};
        std::chrono::sys_seconds expiry{};
        double multiplier{1.0};
    };

    class EuropeanVanillaOption final : public IOptionProduct
    {
    public:
        explicit EuropeanVanillaOption(EuropeanVanillaOptionTerms terms);

        std::string_view id() const noexcept override;
        std::string_view family() const noexcept override;
        std::chrono::sys_seconds maturity() const noexcept override;
        double contractMultiplier() const noexcept override;
        void validate() const override;

        std::string_view underlyingAssetId() const noexcept override;
        OptionRight optionRight() const noexcept override;
        double strike() const noexcept override;
        const payoffs::IPayoff& payoff() const noexcept override;
        const exercise::IExerciseRule& exerciseRule() const noexcept override;

    private:
        EuropeanVanillaOptionTerms terms_;
        std::shared_ptr<const payoffs::IPayoff> payoff_;
        std::shared_ptr<const exercise::IExerciseRule> exerciseRule_;
    };
}
