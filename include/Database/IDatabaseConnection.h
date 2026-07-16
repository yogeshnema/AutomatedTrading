#pragma once

#include "Database/DatabaseTypes.h"

#include <string_view>

namespace automated_trading::database
{
    class IDatabaseConnection
    {
    public:
        virtual ~IDatabaseConnection() = default;

        virtual bool isOpen() const noexcept = 0;
        virtual QueryResult execute(
            std::string_view sql,
            const QueryParameters& parameters = {}) = 0;
    };
}
