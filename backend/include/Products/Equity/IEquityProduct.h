#pragma once

#include "Products/IProduct.h"

#include <string_view>

namespace automated_trading::products::equity
{
    class IEquityProduct : public virtual IProduct
    {
    public:
        ~IEquityProduct() override = default;

        virtual std::string_view underlyingAssetId() const noexcept = 0;
    };
}
