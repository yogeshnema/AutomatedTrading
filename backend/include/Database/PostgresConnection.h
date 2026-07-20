#pragma once

#include "Database/IDatabaseConnection.h"

#include <memory>
#include <optional>
#include <string>

namespace automated_trading::database
{
    struct PostgresConnectionConfig
    {
        std::string host{"127.0.0.1"};
        std::string port{"5432"};
        std::string database{"automated_trading"};
        std::string user{"trading_owner"};
        std::optional<std::string> password;
        std::string applicationName{"AutomatedTrading"};
        int connectTimeoutSeconds{5};
        std::string sslMode{"prefer"};

        static PostgresConnectionConfig fromEnvironment();
    };

    class PostgresConnection final : public IDatabaseConnection
    {
    public:
        explicit PostgresConnection(PostgresConnectionConfig config);
        ~PostgresConnection() override;

        PostgresConnection(const PostgresConnection&) = delete;
        PostgresConnection& operator=(const PostgresConnection&) = delete;
        PostgresConnection(PostgresConnection&&) noexcept;
        PostgresConnection& operator=(PostgresConnection&&) noexcept;

        bool isOpen() const noexcept override;
        QueryResult execute(
            std::string_view sql,
            const QueryParameters& parameters = {}) override;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };
}
