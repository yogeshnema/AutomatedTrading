#include "Database/InsertService.h"

#include <cctype>
#include <sstream>

namespace automated_trading::database
{
    InsertService::InsertService(QueryService& queryService)
        : queryService_(queryService)
    {
    }

    QueryResult InsertService::insert(const InsertRequest& request) const
    {
        validateIdentifier(request.schema);
        validateIdentifier(request.table);
        if (request.fields.empty()) {
            throw DatabaseError("Insert request must contain at least one field.");
        }

        std::ostringstream sql;
        sql << "INSERT INTO " << quoteIdentifier(request.schema) << '.' << quoteIdentifier(request.table) << " (";

        QueryParameters parameters;
        parameters.reserve(request.fields.size());
        for (std::size_t index = 0; index < request.fields.size(); ++index) {
            validateIdentifier(request.fields[index].first);
            if (index != 0) sql << ", ";
            sql << quoteIdentifier(request.fields[index].first);
            parameters.push_back(request.fields[index].second);
        }

        sql << ") VALUES (";
        for (std::size_t index = 0; index < request.fields.size(); ++index) {
            if (index != 0) sql << ", ";
            sql << '$' << (index + 1);
        }
        sql << ')';

        if (!request.returningColumns.empty()) {
            sql << " RETURNING ";
            for (std::size_t index = 0; index < request.returningColumns.size(); ++index) {
                validateIdentifier(request.returningColumns[index]);
                if (index != 0) sql << ", ";
                sql << quoteIdentifier(request.returningColumns[index]);
            }
        }

        return queryService_.execute(sql.str(), parameters);
    }

    void InsertService::validateIdentifier(const std::string& identifier)
    {
        if (identifier.empty() ||
            !(std::isalpha(static_cast<unsigned char>(identifier.front())) || identifier.front() == '_')) {
            throw DatabaseError("Invalid SQL identifier '" + identifier + "'.");
        }
        for (const char character : identifier) {
            const auto value = static_cast<unsigned char>(character);
            if (!(std::isalnum(value) || character == '_')) {
                throw DatabaseError("Invalid SQL identifier '" + identifier + "'.");
            }
        }
    }

    std::string InsertService::quoteIdentifier(const std::string& identifier)
    {
        return '"' + identifier + '"';
    }
}
