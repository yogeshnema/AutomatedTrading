#pragma once

#include "Database/IDatabaseConnection.h"

#include <functional>
#include <optional>
#include <string_view>

namespace automated_trading::database
{
    class QueryService
    {
    public:
        explicit QueryService(IDatabaseConnection& connection);

        QueryResult execute(
            std::string_view sql,
            const QueryParameters& parameters = {}) const;

        std::optional<QueryRow> queryOne(
            std::string_view sql,
            const QueryParameters& parameters = {}) const;

        void transaction(const std::function<void(QueryService&)>& work);

    private:
        IDatabaseConnection& connection_;
    };
}
