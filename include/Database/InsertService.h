#pragma once

#include "Database/QueryService.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace automated_trading::database
{
    using InsertField = std::pair<std::string, QueryParameter>;

    struct InsertRequest
    {
        std::string schema;
        std::string table;
        std::vector<InsertField> fields;
        std::vector<std::string> returningColumns;
    };

    class InsertService
    {
    public:
        explicit InsertService(QueryService& queryService);

        QueryResult insert(const InsertRequest& request) const;

    private:
        static void validateIdentifier(const std::string& identifier);
        static std::string quoteIdentifier(const std::string& identifier);

        QueryService& queryService_;
    };
}
