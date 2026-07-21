#include "Common/Environment.h"

#include <httplib.h>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "libpq.lib")
#endif

namespace
{
    using Json = nlohmann::json;
    using ResultPtr = std::unique_ptr<PGresult, decltype(&PQclear)>;

    std::string environmentValue(const char* name, std::string fallback)
    {
        return automated_trading::common::environmentValue(name).value_or(std::move(fallback));
    }

    int environmentPort(const char* name, int fallback)
    {
        const auto value = automated_trading::common::environmentValue(name);
        if (!value) return fallback;
        const int port = std::stoi(*value);
        if (port < 1 || port > 65535) throw std::runtime_error(std::string(name) + " must be between 1 and 65535");
        return port;
    }

    std::string uuidV4()
    {
        std::array<unsigned char, 16> bytes{};
        std::random_device random;
        for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
        bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fU) | 0x40U);
        bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fU) | 0x80U);

        constexpr char hex[] = "0123456789abcdef";
        std::string value;
        value.reserve(36);
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (index == 4 || index == 6 || index == 8 || index == 10) value.push_back('-');
            value.push_back(hex[bytes[index] >> 4U]);
            value.push_back(hex[bytes[index] & 0x0fU]);
        }
        return value;
    }

    Json tradeFromRow(PGresult* result, int row)
    {
        const auto text = [&](const char* column) -> std::string {
            return PQgetvalue(result, row, PQfnumber(result, column));
        };
        const auto nullableText = [&](const char* column) -> Json {
            const int index = PQfnumber(result, column);
            return index < 0 || PQgetisnull(result, row, index) ? Json(nullptr) : Json(PQgetvalue(result, row, index));
        };
        const auto nullableNumber = [&](const char* column) -> Json {
            const int index = PQfnumber(result, column);
            return index < 0 || PQgetisnull(result, row, index)
                ? Json(nullptr) : Json(std::stod(PQgetvalue(result, row, index)));
        };
        return {
            {"id", text("id")},
            {"version", std::stoi(text("version"))},
            {"name", text("name")},
            {"productType", text("product_type")},
            {"underlying", text("underlying")},
            {"currency", text("currency")},
            {"notional", std::stod(text("notional"))},
            {"status", text("status")},
            {"economics", Json::parse(text("economics"))},
            {"instrumentToken", nullableNumber("instrument_token")},
            {"tradingSymbol", nullableText("trading_symbol")},
            {"expiry", nullableText("expiry")},
            {"strike", nullableNumber("strike")},
            {"optionType", nullableText("option_type")},
            {"lotSize", nullableNumber("lot_size")},
            {"side", nullableText("side")},
            {"quantity", nullableNumber("quantity")},
            {"executionPrice", nullableNumber("execution_price")},
            {"spotSeriesCode", nullableText("spot_series_code")},
            {"impliedVolSeriesCode", nullableText("implied_vol_series_code")},
            {"rateSeriesCode", nullableText("rate_series_code")},
            {"createdAt", text("created_at")},
            {"updatedAt", text("updated_at")}
        };
    }

    class TradeStore
    {
    public:
        TradeStore()
        {
            const std::array<const char*, 8> keywords{
                "host", "port", "dbname", "user", "password", "application_name", "connect_timeout", "sslmode"};
            const auto host = environmentValue("DATABASE_HOST", "127.0.0.1");
            const auto port = environmentValue("DATABASE_PORT", "5432");
            const auto database = environmentValue("DATABASE_NAME", "automated_trading");
            const auto user = environmentValue("DATABASE_USER", "trading_owner");
            const auto password = environmentValue("DATABASE_PASSWORD", "");
            const auto applicationName = environmentValue("DATABASE_APPLICATION_NAME", "TradeLibraryService");
            const auto timeout = environmentValue("DATABASE_CONNECT_TIMEOUT", "5");
            const auto sslMode = environmentValue("DATABASE_SSLMODE", "prefer");
            const std::array<const char*, 9> terminatedKeywords{
                keywords[0], keywords[1], keywords[2], keywords[3], keywords[4], keywords[5], keywords[6], keywords[7], nullptr};
            const std::array<const char*, 9> values{
                host.c_str(), port.c_str(), database.c_str(), user.c_str(),
                password.empty() ? nullptr : password.c_str(), applicationName.c_str(), timeout.c_str(), sslMode.c_str(), nullptr};

            connection_.reset(PQconnectdbParams(terminatedKeywords.data(), values.data(), 0));
            if (!connection_ || PQstatus(connection_.get()) != CONNECTION_OK) {
                throw std::runtime_error("Failed to connect to PostgreSQL: " + std::string(PQerrorMessage(connection_.get())));
            }
            executeCommand(R"sql(
                CREATE SCHEMA IF NOT EXISTS trade_library;
                CREATE TABLE IF NOT EXISTS trade_library.trades (
                    id uuid PRIMARY KEY,
                    version integer NOT NULL DEFAULT 1,
                    name text NOT NULL,
                    product_type text NOT NULL,
                    underlying text NOT NULL,
                    currency char(3) NOT NULL,
                    notional numeric(24,8) NOT NULL,
                    status text NOT NULL DEFAULT 'DRAFT',
                    economics jsonb NOT NULL DEFAULT '{}'::jsonb,
                    instrument_token bigint,
                    trading_symbol text,
                    expiry date,
                    strike numeric(20,6),
                    option_type text,
                    lot_size numeric(20,6),
                    side text,
                    quantity numeric(24,8),
                    execution_price numeric(24,8),
                    spot_series_code text,
                    implied_vol_series_code text,
                    rate_series_code text,
                    created_at timestamptz NOT NULL DEFAULT now(),
                    updated_at timestamptz NOT NULL DEFAULT now(),
                    CONSTRAINT trade_status_valid CHECK (status IN ('DRAFT', 'ACTIVE', 'MATURED', 'CANCELLED'))
                );
                CREATE INDEX IF NOT EXISTS trades_updated_at_idx ON trade_library.trades(updated_at DESC);
                CREATE INDEX IF NOT EXISTS trades_underlying_idx ON trade_library.trades(underlying);
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS instrument_token bigint;
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS trading_symbol text;
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS expiry date;
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS strike numeric(20,6);
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS option_type text;
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS lot_size numeric(20,6);
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS side text;
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS quantity numeric(24,8);
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS execution_price numeric(24,8);
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS spot_series_code text;
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS implied_vol_series_code text;
                ALTER TABLE trade_library.trades ADD COLUMN IF NOT EXISTS rate_series_code text;
                CREATE INDEX IF NOT EXISTS trades_instrument_token_idx ON trade_library.trades(instrument_token);
            )sql");
        }

        Json list(std::string search)
        {
            const std::string pattern = "%" + search + "%";
            auto result = query(R"sql(
                SELECT id::text, version::text, name, product_type, underlying, currency,
                       notional::text, status, economics::text,
                       instrument_token::text, trading_symbol, expiry::text, strike::text,
                       option_type, lot_size::text, side, quantity::text, execution_price::text,
                       spot_series_code, implied_vol_series_code, rate_series_code,
                       to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS created_at,
                       to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS updated_at
                FROM trade_library.trades
                WHERE $1 = '%%' OR name ILIKE $1 OR underlying ILIKE $1 OR product_type ILIKE $1
                ORDER BY updated_at DESC LIMIT 500
            )sql", {pattern});
            Json trades = Json::array();
            for (int row = 0; row < PQntuples(result.get()); ++row) trades.push_back(tradeFromRow(result.get(), row));
            return trades;
        }

        std::optional<Json> get(const std::string& id)
        {
            auto result = selectById(id);
            if (PQntuples(result.get()) == 0) return std::nullopt;
            return tradeFromRow(result.get(), 0);
        }

        Json create(const Json& input)
        {
            validate(input);
            const std::string id = uuidV4();
            const auto instrument = instrumentFor(input.at("instrumentToken").get<long long>());
            validateSeries(input.at("impliedVolSeriesCode").get<std::string>(), "implied_volatility");
            validateSeries(input.at("rateSeriesCode").get<std::string>(), "risk_free_rate");
            const double quantity = input.at("quantity").get<double>();
            const double executionPrice = input.at("executionPrice").get<double>();
            const double lotSize = instrument.at("lotSize").get<double>();
            const std::string notional = std::to_string(quantity * executionPrice * lotSize);
            const std::string economics = Json{{"instrumentToken", instrument.at("instrumentToken")},
                {"tradingSymbol", instrument.at("tradingSymbol")}, {"expiry", instrument.at("expiry")},
                {"strike", instrument.at("strike")}, {"optionType", instrument.at("optionType")},
                {"lotSize", lotSize}, {"side", input.at("side")}, {"quantity", quantity},
                {"executionPrice", executionPrice}, {"spotSeriesCode", "NIFTY_50_EOD"},
                {"impliedVolSeriesCode", input.at("impliedVolSeriesCode")},
                {"rateSeriesCode", input.at("rateSeriesCode")}}.dump();
            auto result = query(R"sql(
                INSERT INTO trade_library.trades
                    (id, name, product_type, underlying, currency, notional, status, economics,
                     instrument_token, trading_symbol, expiry, strike, option_type, lot_size,
                     side, quantity, execution_price, spot_series_code, implied_vol_series_code, rate_series_code)
                VALUES ($1::uuid, $2, 'EQUITY_OPTION_EUROPEAN', $3, 'INR', $4::numeric, $5, $6::jsonb,
                        $7::bigint, $8, $9::date, $10::numeric, $11, $12::numeric,
                        $13, $14::numeric, $15::numeric, 'NIFTY_50_EOD', $16, $17)
                RETURNING id::text, version::text, name, product_type, underlying, currency,
                          notional::text, status, economics::text,
                          instrument_token::text, trading_symbol, expiry::text, strike::text,
                          option_type, lot_size::text, side, quantity::text, execution_price::text,
                          spot_series_code, implied_vol_series_code, rate_series_code,
                          to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS created_at,
                          to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS updated_at
            )sql", {id, input.value("name", instrument.at("tradingSymbol").get<std::string>()),
                    instrument.at("name").get<std::string>(), notional, input.value("status", "DRAFT"), economics,
                    std::to_string(instrument.at("instrumentToken").get<long long>()), instrument.at("tradingSymbol").get<std::string>(),
                    instrument.at("expiry").get<std::string>(), std::to_string(instrument.at("strike").get<double>()),
                    instrument.at("optionType").get<std::string>(), std::to_string(lotSize), input.at("side").get<std::string>(),
                    std::to_string(quantity), std::to_string(executionPrice),
                    input.at("impliedVolSeriesCode").get<std::string>(), input.at("rateSeriesCode").get<std::string>()});
            return tradeFromRow(result.get(), 0);
        }

        std::optional<Json> update(const std::string& id, const Json& input)
        {
            validate(input);
            const auto instrument = instrumentFor(input.at("instrumentToken").get<long long>());
            validateSeries(input.at("impliedVolSeriesCode").get<std::string>(), "implied_volatility");
            validateSeries(input.at("rateSeriesCode").get<std::string>(), "risk_free_rate");
            const std::string expectedVersion = std::to_string(input.at("version").get<int>());
            const double quantity = input.at("quantity").get<double>();
            const double executionPrice = input.at("executionPrice").get<double>();
            const double lotSize = instrument.at("lotSize").get<double>();
            const std::string notional = std::to_string(quantity * executionPrice * lotSize);
            const std::string economics = Json{{"instrumentToken", instrument.at("instrumentToken")},
                {"tradingSymbol", instrument.at("tradingSymbol")}, {"expiry", instrument.at("expiry")},
                {"strike", instrument.at("strike")}, {"optionType", instrument.at("optionType")},
                {"lotSize", lotSize}, {"side", input.at("side")}, {"quantity", quantity},
                {"executionPrice", executionPrice}, {"spotSeriesCode", "NIFTY_50_EOD"},
                {"impliedVolSeriesCode", input.at("impliedVolSeriesCode")},
                {"rateSeriesCode", input.at("rateSeriesCode")}}.dump();
            auto result = query(R"sql(
                UPDATE trade_library.trades
                SET version=version+1, name=$3, product_type='EQUITY_OPTION_EUROPEAN', underlying=$4,
                    currency='INR', notional=$5::numeric, status=$6, economics=$7::jsonb,
                    instrument_token=$8::bigint, trading_symbol=$9, expiry=$10::date, strike=$11::numeric,
                    option_type=$12, lot_size=$13::numeric, side=$14, quantity=$15::numeric,
                    execution_price=$16::numeric, spot_series_code='NIFTY_50_EOD',
                    implied_vol_series_code=$17, rate_series_code=$18, updated_at=now()
                WHERE id = $1::uuid AND version = $2::integer
                RETURNING id::text, version::text, name, product_type, underlying, currency,
                          notional::text, status, economics::text,
                          instrument_token::text, trading_symbol, expiry::text, strike::text,
                          option_type, lot_size::text, side, quantity::text, execution_price::text,
                          spot_series_code, implied_vol_series_code, rate_series_code,
                          to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS created_at,
                          to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS updated_at
            )sql", {id, expectedVersion, input.value("name", instrument.at("tradingSymbol").get<std::string>()),
                    instrument.at("name").get<std::string>(), notional, input.value("status", "DRAFT"), economics,
                    std::to_string(instrument.at("instrumentToken").get<long long>()), instrument.at("tradingSymbol").get<std::string>(),
                    instrument.at("expiry").get<std::string>(), std::to_string(instrument.at("strike").get<double>()),
                    instrument.at("optionType").get<std::string>(), std::to_string(lotSize), input.at("side").get<std::string>(),
                    std::to_string(quantity), std::to_string(executionPrice),
                    input.at("impliedVolSeriesCode").get<std::string>(), input.at("rateSeriesCode").get<std::string>()});
            if (PQntuples(result.get()) == 0) return std::nullopt;
            return tradeFromRow(result.get(), 0);
        }

        bool remove(const std::string& id)
        {
            auto result = query("DELETE FROM trade_library.trades WHERE id = $1::uuid RETURNING id::text", {id});
            return PQntuples(result.get()) == 1;
        }

        bool ready()
        {
            try { return PQntuples(query("SELECT 1", {}).get()) == 1; }
            catch (...) { return false; }
        }

    private:
        struct ConnectionDeleter { void operator()(PGconn* value) const { if (value) PQfinish(value); } };
        std::unique_ptr<PGconn, ConnectionDeleter> connection_;
        std::mutex mutex_;

        static void validate(const Json& input)
        {
            if (!input.contains("instrumentToken") || !input["instrumentToken"].is_number_integer())
                throw std::invalid_argument("instrumentToken from the instrument master is required");
            if (!input.contains("quantity") || !input["quantity"].is_number() || input["quantity"].get<double>() <= 0)
                throw std::invalid_argument("quantity must be positive");
            if (!input.contains("executionPrice") || !input["executionPrice"].is_number() || input["executionPrice"].get<double>() < 0)
                throw std::invalid_argument("executionPrice must be non-negative");
            if (!input.contains("side") || !input["side"].is_string() ||
                (input["side"] != "buy" && input["side"] != "sell"))
                throw std::invalid_argument("side must be buy or sell");
            for (const auto* field : {"impliedVolSeriesCode", "rateSeriesCode"})
                if (!input.contains(field) || !input[field].is_string() || input[field].get<std::string>().empty())
                    throw std::invalid_argument(std::string(field) + " is required");
        }

        Json instrumentFor(long long token)
        {
            auto result = query(R"sql(
                SELECT instrument_token::text, trading_symbol, name, expiry::text,
                       strike::text, lot_size::text, option_type
                  FROM market_data.instrument_master
                 WHERE provider='kite' AND instrument_token=$1::bigint
            )sql", {std::to_string(token)});
            if (PQntuples(result.get()) == 0)
                throw std::invalid_argument("instrumentToken was not found in the current Kite instrument master");
            const auto value = [&](const char* name) { return std::string(PQgetvalue(result.get(), 0, PQfnumber(result.get(), name))); };
            return {{"instrumentToken", std::stoll(value("instrument_token"))}, {"tradingSymbol", value("trading_symbol")},
                    {"name", value("name")}, {"expiry", value("expiry")}, {"strike", std::stod(value("strike"))},
                    {"lotSize", std::stod(value("lot_size"))}, {"optionType", value("option_type")}};
        }

        void validateSeries(const std::string& seriesCode, const std::string& expectedType)
        {
            auto result = query(R"sql(
                SELECT 1 FROM market_data.reference_series
                 WHERE series_code=$1 AND series_type=$2 AND enabled
            )sql", {seriesCode, expectedType});
            if (PQntuples(result.get()) == 0)
                throw std::invalid_argument(seriesCode + " is not an enabled " + expectedType + " series");
        }

        ResultPtr selectById(const std::string& id)
        {
            return query(R"sql(
                SELECT id::text, version::text, name, product_type, underlying, currency,
                       notional::text, status, economics::text,
                       instrument_token::text, trading_symbol, expiry::text, strike::text,
                       option_type, lot_size::text, side, quantity::text, execution_price::text,
                       spot_series_code, implied_vol_series_code, rate_series_code,
                       to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS created_at,
                       to_char(updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"') AS updated_at
                FROM trade_library.trades WHERE id = $1::uuid
            )sql", {id});
        }

        ResultPtr query(const std::string& sql, const std::vector<std::string>& parameters)
        {
            std::lock_guard lock(mutex_);
            std::vector<const char*> values;
            values.reserve(parameters.size());
            for (const auto& parameter : parameters) values.push_back(parameter.c_str());
            ResultPtr result(PQexecParams(connection_.get(), sql.c_str(), static_cast<int>(values.size()), nullptr,
                                          values.empty() ? nullptr : values.data(), nullptr, nullptr, 0), &PQclear);
            if (!result) throw std::runtime_error(PQerrorMessage(connection_.get()));
            const auto status = PQresultStatus(result.get());
            if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
                throw std::runtime_error(PQresultErrorMessage(result.get()));
            return result;
        }

        void executeCommand(const std::string& sql)
        {
            std::lock_guard lock(mutex_);
            ResultPtr result(PQexec(connection_.get(), sql.c_str()), &PQclear);
            if (!result) throw std::runtime_error(PQerrorMessage(connection_.get()));

            const auto status = PQresultStatus(result.get());
            if (status != PGRES_COMMAND_OK) {
                throw std::runtime_error(PQresultErrorMessage(result.get()));
            }
        }
    };

    void jsonResponse(httplib::Response& response, int status, const Json& body)
    {
        response.status = status;
        response.set_content(body.dump(2), "application/json");
    }

    Json parseBody(const httplib::Request& request)
    {
        if (request.body.empty()) throw std::invalid_argument("request body is required");
        return Json::parse(request.body);
    }
}

int main()
{
    try {
        TradeStore store;
        httplib::Server server;
        server.set_default_headers({
            {"Access-Control-Allow-Origin", environmentValue("CORS_ALLOW_ORIGIN", "http://localhost:5173")},
            {"Access-Control-Allow-Headers", "Content-Type, Accept"},
            {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"}
        });
        server.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& response) { response.status = 204; });
        server.Get("/health/live", [](const httplib::Request&, httplib::Response& response) {
            jsonResponse(response, 200, {{"status", "live"}, {"service", "trade-library"}});
        });
        server.Get("/health/ready", [&](const httplib::Request&, httplib::Response& response) {
            const bool ready = store.ready();
            jsonResponse(response, ready ? 200 : 503, {{"status", ready ? "ready" : "not_ready"}, {"database", ready ? "connected" : "unavailable"}});
        });
        server.Get("/api/v1/trades", [&](const httplib::Request& request, httplib::Response& response) {
            try { jsonResponse(response, 200, {{"items", store.list(request.has_param("search") ? request.get_param_value("search") : "")}}); }
            catch (const std::exception& error) { jsonResponse(response, 500, {{"error", error.what()}}); }
        });
        server.Get(R"(/api/v1/trades/([0-9a-fA-F-]+))", [&](const httplib::Request& request, httplib::Response& response) {
            try {
                const auto trade = store.get(request.matches[1]);
                trade ? jsonResponse(response, 200, *trade) : jsonResponse(response, 404, {{"error", "trade not found"}});
            } catch (const std::exception& error) { jsonResponse(response, 400, {{"error", error.what()}}); }
        });
        server.Post("/api/v1/trades", [&](const httplib::Request& request, httplib::Response& response) {
            try { jsonResponse(response, 201, store.create(parseBody(request))); }
            catch (const std::exception& error) { jsonResponse(response, 400, {{"error", error.what()}}); }
        });
        server.Put(R"(/api/v1/trades/([0-9a-fA-F-]+))", [&](const httplib::Request& request, httplib::Response& response) {
            try {
                const auto trade = store.update(request.matches[1], parseBody(request));
                if (trade) {
                    jsonResponse(response, 200, *trade);
                }
                else {
                    jsonResponse(response, 409, {{"error", "trade changed or no longer exists; refresh and retry"}});
                }
            }
            catch (const std::exception& error) { jsonResponse(response, 400, {{"error", error.what()}}); }
        });
        server.Delete(R"(/api/v1/trades/([0-9a-fA-F-]+))", [&](const httplib::Request& request, httplib::Response& response) {
            try {
                if (store.remove(request.matches[1])) {
                    response.status = 204;
                }
                else {
                    jsonResponse(response, 404, {{"error", "trade not found"}});
                }
            }
            catch (const std::exception& error) { jsonResponse(response, 400, {{"error", error.what()}}); }
        });
        server.Get("/openapi.json", [](const httplib::Request&, httplib::Response& response) {
            jsonResponse(response, 200, {{"openapi", "3.0.3"}, {"info", {{"title", "Trade Library API"}, {"version", "1.0.0"}}},
                {"paths", {{"/api/v1/trades", {{"get", {{"summary", "List trades"}}}, {"post", {{"summary", "Create a trade"}}}}},
                           {"/api/v1/trades/{id}", {{"get", {{"summary", "Get a trade"}}}, {"put", {{"summary", "Update a trade"}}}, {"delete", {{"summary", "Delete a trade"}}}}}}}});
        });

        const auto host = environmentValue("TRADE_LIBRARY_HOST", "0.0.0.0");
        const int port = environmentPort("TRADE_LIBRARY_PORT", 8101);
        std::cout << "[trade-library] listening=http://" << host << ':' << port << '\n';
        if (!server.listen(host, port)) throw std::runtime_error("failed to bind HTTP listener");
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "[trade-library] startup failed: " << error.what() << '\n';
        return 1;
    }
}
