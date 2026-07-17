#pragma once

#include "Products/Equity/Options/IOptionProduct.h"

#include <span>

namespace automated_trading::products::equity::options::asian
{
    enum class AverageType
    {
        Arithmetic,
        Geometric
    };

    class IAsianOption : public virtual IOptionProduct
    {
    public:
        ~IAsianOption() override = default;

        virtual AverageType averageType() const noexcept = 0;
        virtual std::span<const std::chrono::sys_seconds> observationDates() const noexcept = 0;
    };
}
