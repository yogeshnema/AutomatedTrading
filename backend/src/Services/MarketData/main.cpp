#include "Common/Environment.h"
#include "Common/KiteSession.h"
#include "Database/PostgresConnection.h"
#include "Database/QueryService.h"
#include "MarketData/KiteInstrumentService.h"
#include "MarketData/KiteMarketDataService.h"
#include "Services/MarketData/MarketDataPollingService.h"
#include "Services/MarketData/MarketDataRepository.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    using Json = nlohmann::json;
    using namespace automated_trading::services::market_data;

    std::string env(const char* name, const std::string& fallback)
    {
        const auto value = automated_trading::common::environmentValue(name);
        return value.value_or(fallback);
    }

    int positiveInteger(const char* name, int fallback)
    {
        const auto value = automated_trading::common::environmentValue(name);
        if (!value) return fallback;
        int result{};
        const auto parsed = std::from_chars(value->data(), value->data() + value->size(), result);
        if (parsed.ec != std::errc{} || result <= 0) throw std::runtime_error(std::string(name) + " must be a positive integer.");
        return result;
    }

    void cors(httplib::Response& response)
    {
        response.set_header("Access-Control-Allow-Origin", env("MARKET_DATA_CORS_ORIGIN", "http://localhost:5173"));
        response.set_header("Access-Control-Allow-Headers", "Content-Type, Accept");
        response.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    }

    void json(httplib::Response& response, int status, const Json& body)
    {
        cors(response);
        response.status = status;
        response.set_content(body.dump(), "application/json; charset=utf-8");
    }

    Json instrumentJson(const InstrumentRecord& item)
    {
        return {{"instrumentToken", item.instrumentToken}, {"exchange", item.exchange},
                {"tradingSymbol", item.tradingSymbol}, {"name", item.name}, {"expiry", item.expiry},
                {"strike", item.strike}, {"optionType", item.optionType}, {"subscribed", item.subscribed},
                {"lastRefreshAt", item.lastRefreshAt}, {"lastError", item.lastError}};
    }

    Json subscriptionJson(const SubscriptionRecord& item)
    {
        return {{"instrumentToken", item.instrumentToken}, {"exchange", item.exchange},
                {"tradingSymbol", item.tradingSymbol}, {"expiry", item.expiry}, {"strike", item.strike},
                {"optionType", item.optionType}, {"interval", item.interval}, {"enabled", item.enabled},
                {"lastRefreshAt", item.lastRefreshAt}, {"lastCandleAt", item.lastCandleAt},
                {"lastError", item.lastError}};
    }
}

int main()
{
    try {
        auto session = KiteSession::fromEnvironment();
        auto databaseConfig = automated_trading::database::PostgresConnectionConfig::fromEnvironment();
        databaseConfig.applicationName = "MarketDataService";
        automated_trading::database::PostgresConnection connection(std::move(databaseConfig));
        automated_trading::database::QueryService queries(connection);
        MarketDataRepository repository(queries);
        KiteInstrumentService instruments(session);
        KiteMarketDataService candles(session);
        MarketDataPollingService worker(instruments, candles, repository,
            std::chrono::seconds(positiveInteger("MARKET_DATA_POLL_SECONDS", 60)));
        worker.start();

        httplib::Server server;
        server.Options(R"(.*)", [](const httplib::Request&, httplib::Response& response) {
            cors(response); response.status = 204;
        });
        server.Get("/health/live", [](const httplib::Request&, httplib::Response& response) {
            json(response, 200, {{"status", "live"}, {"service", "market-data"}});
        });
        server.Get("/health/ready", [&](const httplib::Request&, httplib::Response& response) {
            const bool ready = repository.isReady() && worker.running();
            json(response, ready ? 200 : 503, {{"status", ready ? "ready" : "not-ready"}, {"service", "market-data"}});
        });
        server.Get("/api/v1/market-data/status", [&](const httplib::Request&, httplib::Response& response) {
            try {
                const auto status = repository.status(worker.running());
                json(response, 200, {{"databaseReady", status.databaseReady}, {"workerRunning", status.workerRunning},
                    {"instrumentCount", status.instrumentCount}, {"activeSubscriptions", status.activeSubscriptions},
                    {"instrumentMasterRefreshedAt", status.instrumentMasterRefreshedAt},
                    {"pollSeconds", positiveInteger("MARKET_DATA_POLL_SECONDS", 60)}});
            } catch (const std::exception& error) { json(response, 500, {{"error", error.what()}}); }
        });
        server.Post("/api/v1/instruments/refresh", [&](const httplib::Request&, httplib::Response& response) {
            try { worker.refreshInstrumentMaster(); json(response, 202, {{"status", "refreshed"}}); }
            catch (const std::exception& error) { json(response, 502, {{"error", error.what()}}); }
        });
        server.Get("/api/v1/instruments", [&](const httplib::Request& request, httplib::Response& response) {
            try {
                const auto parameter = [&](const char* name) { return request.has_param(name) ? request.get_param_value(name) : std::string{}; };
                int limit = 200;
                if (request.has_param("limit")) limit = std::stoi(request.get_param_value("limit"));
                Json items = Json::array();
                for (const auto& item : repository.searchInstruments(parameter("search"), parameter("expiry"), parameter("optionType"), limit))
                    items.push_back(instrumentJson(item));
                json(response, 200, {{"items", items}, {"count", items.size()}});
            } catch (const std::exception& error) { json(response, 400, {{"error", error.what()}}); }
        });
        server.Get("/api/v1/subscriptions", [&](const httplib::Request&, httplib::Response& response) {
            try {
                Json items = Json::array();
                for (const auto& item : repository.subscriptions()) items.push_back(subscriptionJson(item));
                json(response, 200, {{"items", items}, {"count", items.size()}});
            } catch (const std::exception& error) { json(response, 500, {{"error", error.what()}}); }
        });
        server.Post("/api/v1/subscriptions", [&](const httplib::Request& request, httplib::Response& response) {
            try {
                const auto body = Json::parse(request.body);
                const auto token = body.at("instrumentToken").get<std::int64_t>();
                const auto interval = body.value("interval", std::string("minute"));
                json(response, 201, subscriptionJson(repository.subscribe(token, interval)));
            } catch (const nlohmann::json::exception& error) { json(response, 400, {{"error", std::string("Invalid JSON payload: ") + error.what()}}); }
              catch (const std::invalid_argument& error) { json(response, 400, {{"error", error.what()}}); }
              catch (const std::exception& error) { json(response, 500, {{"error", error.what()}}); }
        });
        server.Delete(R"(/api/v1/subscriptions/(\d+))", [&](const httplib::Request& request, httplib::Response& response) {
            try { repository.unsubscribe(std::stoll(request.matches[1].str())); cors(response); response.status = 204; }
            catch (const std::exception& error) { json(response, 500, {{"error", error.what()}}); }
        });

        const auto host = env("MARKET_DATA_SERVICE_HOST", "0.0.0.0");
        const auto port = positiveInteger("MARKET_DATA_SERVICE_PORT", 8201);
        std::cout << "[market-data-service] listening=http://" << host << ':' << port << '\n';
        if (!server.listen(host, port)) throw std::runtime_error("Failed to bind Market Data HTTP listener.");
        worker.stop();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "[market-data-service] startup failed: " << error.what() << '\n';
        return 1;
    }
}
