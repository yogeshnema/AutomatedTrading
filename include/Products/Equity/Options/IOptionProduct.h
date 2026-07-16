#pragma once

#include "Products/Equity/IEquityProduct.h"
#include "Products/Equity/Options/Exercise/IExerciseRule.h"
#include "Products/Equity/Options/Payoffs/IPayoff.h"

namespace automated_trading::products::equity::options
{
    class IOptionProduct : public virtual IEquityProduct
    {
    public:
        ~IOptionProduct() override = default;

        virtual OptionRight optionRight() const noexcept = 0;
        virtual double strike() const noexcept = 0;
        virtual const payoffs::IPayoff& payoff() const noexcept = 0;
        virtual const exercise::IExerciseRule& exerciseRule() const noexcept = 0;
    };
}
