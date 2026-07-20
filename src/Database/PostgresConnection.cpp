#include "Database/PostgresConnection.h"

#include <libpq-fe.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "libpq.lib")
#endif

namespace automated_trading::database
{
    namespace
    {
        std::optional<std::string> environmentValue(const char* name)
        {
#if defined(_MSC_VER)
            char* value = nullptr;
            std::size_t size = 0;
            if (_dupenv_s(&value, &size, name) != 0 || value == nullptr || size == 0) {
                if (value != nullptr) {
                    std::free(value);
                }
                return std::nullopt;
            }

            std::string result(value);
            std::free(value);
            return result.empty() ? std::nullopt : std::optional<std::string>(std::move(result));
#else
            const char* value = std::getenv(name);
            if (value == nullptr || *value == '\0') {
                return std::nullopt;
            }
            return std::string(value);
#endif
        }

        std::string resultError(PGconn* connection, PGresult* result)
        {
            const char* message = result == nullptr ? nullptr : PQresultErrorMessage(result);
            if (message == nullptr || *message == '\0') {
                message = connection == nullptr ? nullptr : PQerrorMessage(connection);
            }
            return message == nullptr ? "Unknown PostgreSQL error." : std::string(message);
        }

        std::size_t parseAffectedRows(const char* value)
        {
            if (value == nullptr || *value == '\0') {
                return 0;
            }

            std::size_t result = 0;
            const char* end = value;
            while (*end != '\0') {
                ++end;
            }
            const auto conversion = std::from_chars(value, end, result);
            return conversion.ec == std::errc{} ? result : 0;
        }
    }

    struct PostgresConnection::Implementation
    {
        PGconn* connection{nullptr};

        ~Implementation()
        {
            if (connection != nullptr) {
                PQfinish(connection);
            }
        }
    };

    PostgresConnectionConfig PostgresConnectionConfig::fromEnvironment()
    {
        PostgresConnectionConfig config;

        if (const auto value = environmentValue("DATABASE_HOST")) config.host = *value;
        if (const auto value = environmentValue("DATABASE_PORT")) config.port = *value;
        if (const auto value = environmentValue("DATABASE_NAME")) config.database = *value;
        if (const auto value = environmentValue("DATABASE_USER")) config.user = *value;
        if (const auto value = environmentValue("DATABASE_PASSWORD")) config.password = *value;
        if (const auto value = environmentValue("DATABASE_APPLICATION_NAME")) config.applicationName = *value;
        if (const auto value = environmentValue("DATABASE_SSLMODE")) config.sslMode = *value;
        if (const auto value = environmentValue("DATABASE_CONNECT_TIMEOUT")) {
            int timeout = 0;
            const auto conversion = std::from_chars(value->data(), value->data() + value->size(), timeout);
            if (conversion.ec != std::errc{} || timeout <= 0) {
                throw DatabaseError("DATABASE_CONNECT_TIMEOUT must be a positive integer.");
            }
            config.connectTimeoutSeconds = timeout;
        }

        return config;
    }

    PostgresConnection::PostgresConnection(PostgresConnectionConfig config)
        : implementation_(std::make_unique<Implementation>())
    {
        const std::string timeout = std::to_string(config.connectTimeoutSeconds);
        const std::array<const char*, 8> keywords{
            "host", "port", "dbname", "user", "password", "application_name", "connect_timeout", "sslmode"};
        const std::array<const char*, 8> values{
            config.host.c_str(),
            config.port.c_str(),
            config.database.c_str(),
            config.user.c_str(),
            config.password ? config.password->c_str() : nullptr,
            config.applicationName.c_str(),
            timeout.c_str(),
            config.sslMode.c_str()};

        std::array<const char*, 9> terminatedKeywords{};
        std::array<const char*, 9> terminatedValues{};
        std::copy(keywords.begin(), keywords.end(), terminatedKeywords.begin());
        std::copy(values.begin(), values.end(), terminatedValues.begin());

        implementation_->connection = PQconnectdbParams(
            terminatedKeywords.data(), terminatedValues.data(), 0);

        if (!isOpen()) {
            const std::string error = resultError(implementation_->connection, nullptr);
            throw DatabaseError("Failed to connect to PostgreSQL: " + error);
        }
    }

    PostgresConnection::~PostgresConnection() = default;
    PostgresConnection::PostgresConnection(PostgresConnection&&) noexcept = default;
    PostgresConnection& PostgresConnection::operator=(PostgresConnection&&) noexcept = default;

    bool PostgresConnection::isOpen() const noexcept
    {
        return implementation_ && implementation_->connection != nullptr &&
            PQstatus(implementation_->connection) == CONNECTION_OK;
    }

    QueryResult PostgresConnection::execute(
        std::string_view sql,
        const QueryParameters& parameters)
    {
        if (!isOpen()) {
            throw DatabaseError("PostgreSQL connection is not open.");
        }
        if (parameters.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw DatabaseError("Too many query parameters.");
        }

        const std::string statement(sql);
        std::vector<const char*> values;
        values.reserve(parameters.size());
        for (const auto& parameter : parameters) {
            values.push_back(parameter ? parameter->c_str() : nullptr);
        }

        PGresult* rawResult = PQexecParams(
            implementation_->connection,
            statement.c_str(),
            static_cast<int>(parameters.size()),
            nullptr,
            values.empty() ? nullptr : values.data(),
            nullptr,
            nullptr,
            0);

        if (rawResult == nullptr) {
            throw DatabaseError(resultError(implementation_->connection, nullptr));
        }

        std::unique_ptr<PGresult, decltype(&PQclear)> result(rawResult, &PQclear);
        const ExecStatusType status = PQresultStatus(result.get());
        if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
            throw DatabaseError(resultError(implementation_->connection, result.get()));
        }

        QueryResult output;
        if (const char* commandStatus = PQcmdStatus(result.get())) {
            output.commandStatus = commandStatus;
        }
        output.affectedRows = parseAffectedRows(PQcmdTuples(result.get()));

        const int columnCount = PQnfields(result.get());
        const int rowCount = PQntuples(result.get());
        output.columns.reserve(static_cast<std::size_t>(columnCount));
        for (int column = 0; column < columnCount; ++column) {
            output.columns.emplace_back(PQfname(result.get(), column));
        }

        output.rows.reserve(static_cast<std::size_t>(rowCount));
        for (int row = 0; row < rowCount; ++row) {
            std::unordered_map<std::string, QueryParameter> rowValues;
            rowValues.reserve(static_cast<std::size_t>(columnCount));
            for (int column = 0; column < columnCount; ++column) {
                QueryParameter value;
                if (PQgetisnull(result.get(), row, column) == 0) {
                    value = std::string(PQgetvalue(result.get(), row, column));
                }
                rowValues.emplace(output.columns[static_cast<std::size_t>(column)], std::move(value));
            }
            output.rows.emplace_back(std::move(rowValues));
        }

        return output;
    }
}
