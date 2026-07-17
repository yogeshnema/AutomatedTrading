#pragma once

#include <chrono>
#include <string_view>

namespace automated_trading::products
{
    class IProduct
    {
    public:
        virtual ~IProduct() = default;

        virtual std::string_view id() const noexcept = 0;
        virtual std::string_view family() const noexcept = 0;
        virtual std::chrono::sys_seconds maturity() const noexcept = 0;
        virtual double contractMultiplier() const noexcept = 0;
        virtual void validate() const = 0;
    };
}
