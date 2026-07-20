#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace automated_trading::database
{
    using QueryParameter = std::optional<std::string>;
    using QueryParameters = std::vector<QueryParameter>;

    class DatabaseError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    class QueryRow
    {
    public:
        QueryRow() = default;
        explicit QueryRow(std::unordered_map<std::string, QueryParameter> values);

        bool contains(std::string_view column) const;
        const QueryParameter& value(std::string_view column) const;
        std::optional<std::string> optionalString(std::string_view column) const;
        std::string string(std::string_view column) const;

    private:
        std::unordered_map<std::string, QueryParameter> values_;
    };

    struct QueryResult
    {
        std::vector<std::string> columns;
        std::vector<QueryRow> rows;
        std::size_t affectedRows{0};
        std::string commandStatus;

        bool empty() const noexcept { return rows.empty(); }
    };
}
