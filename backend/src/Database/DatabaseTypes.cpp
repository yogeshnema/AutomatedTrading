#include "Database/DatabaseTypes.h"

#include <utility>

namespace automated_trading::database
{
    QueryRow::QueryRow(std::unordered_map<std::string, QueryParameter> values)
        : values_(std::move(values))
    {
    }

    bool QueryRow::contains(std::string_view column) const
    {
        return values_.contains(std::string(column));
    }

    const QueryParameter& QueryRow::value(std::string_view column) const
    {
        const auto iterator = values_.find(std::string(column));
        if (iterator == values_.end()) {
            throw DatabaseError("Query result does not contain column '" + std::string(column) + "'.");
        }
        return iterator->second;
    }

    std::optional<std::string> QueryRow::optionalString(std::string_view column) const
    {
        return value(column);
    }

    std::string QueryRow::string(std::string_view column) const
    {
        const auto& result = value(column);
        if (!result) {
            throw DatabaseError("Column '" + std::string(column) + "' is NULL.");
        }
        return *result;
    }
}
