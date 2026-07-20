#pragma once

#include "Products/Equity/Options/IOptionProduct.h"

namespace automated_trading::products::equity::options::barrier
{
    enum class BarrierDirection
    {
        Up,
        Down
    };

    enum class BarrierAction
    {
        KnockIn,
        KnockOut
    };

    class IBarrierOption : public virtual IOptionProduct
    {
    public:
        ~IBarrierOption() override = default;

        virtual BarrierDirection barrierDirection() const noexcept = 0;
        virtual BarrierAction barrierAction() const noexcept = 0;
        virtual double barrierLevel() const noexcept = 0;
        virtual double rebate() const noexcept = 0;
    };
}
