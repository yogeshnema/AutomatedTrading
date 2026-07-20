#include "Database/QueryService.h"

namespace automated_trading::database
{
    QueryService::QueryService(IDatabaseConnection& connection)
        : connection_(connection)
    {
    }

    QueryResult QueryService::execute(
        std::string_view sql,
        const QueryParameters& parameters) const
    {
        return connection_.execute(sql, parameters);
    }

    std::optional<QueryRow> QueryService::queryOne(
        std::string_view sql,
        const QueryParameters& parameters) const
    {
        auto result = execute(sql, parameters);
        if (result.rows.empty()) {
            return std::nullopt;
        }
        if (result.rows.size() != 1) {
            throw DatabaseError("Expected at most one query row, but received " +
                std::to_string(result.rows.size()) + ".");
        }
        return std::move(result.rows.front());
    }

    void QueryService::transaction(const std::function<void(QueryService&)>& work)
    {
        execute("BEGIN");
        try {
            work(*this);
            execute("COMMIT");
        }
        catch (...) {
            try {
                execute("ROLLBACK");
            }
            catch (...) {
            }
            throw;
        }
    }
}
