#include "Analytics/Risk/BlackScholes/AnalyticBlackScholesRiskEngine.h"
#include "Database/PostgresConnection.h"
#include "Database/QueryService.h"
#include "Services/Pricing/PostgresTradePricingRepository.h"
#include "Services/Risk/RiskApplicationService.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace
{
    using Json = nlohmann::json;
    using automated_trading::analytics::risk::GreekVector;
    using automated_trading::services::risk::TradeRiskOutcome;

    std::string environmentValue(const char* name, std::string fallback)
    {
        const char* value = std::getenv(name);
        return value == nullptr || *value == '\0' ? std::move(fallback) : std::string(value);
    }

    int positiveInteger(const char* name, int fallback, int maximum)
    {
        const std::string value = environmentValue(name, std::to_string(fallback));
        int result = 0;
        const auto conversion = std::from_chars(value.data(), value.data() + value.size(), result);
        if (conversion.ec != std::errc{} || result <= 0 || result > maximum)
            throw std::runtime_error(std::string(name) + " must be between 1 and " + std::to_string(maximum) + ".");
        return result;
    }

    void jsonResponse(httplib::Response& response, int status, const Json& body)
    {
        response.status = status;
        response.set_content(body.dump(), "application/json; charset=utf-8");
    }

    Json greeksJson(const GreekVector& greeks)
    {
        return {{"delta", greeks.delta}, {"gamma", greeks.gamma}, {"vega", greeks.vega},
                {"theta", greeks.theta}, {"rho", greeks.rho}};
    }

    Json outcomeJson(const TradeRiskOutcome& outcome)
    {
        return {
            {"tradeId", outcome.tradeId},
            {"valuationDate", outcome.valuationDate},
            {"productType", outcome.productType},
            {"underlyingAssetId", outcome.underlyingAssetId},
            {"currency", outcome.currency},
            {"engine", {{"model", outcome.model}, {"method", outcome.method},
                        {"computeBackend", outcome.computeBackend}}},
            {"inputs", {
                {"signedQuantity", outcome.signedQuantity},
                {"contractMultiplier", outcome.contractMultiplier},
                {"positionScale", outcome.input.positionScale},
                {"spot", outcome.input.spot},
                {"strike", outcome.input.strike},
                {"riskFreeRate", outcome.input.riskFreeRate},
                {"dividendYield", outcome.input.dividendYield},
                {"volatility", outcome.input.volatility},
                {"timeToExpiryYears", outcome.input.timeToExpiryYears}}},
            {"result", {
                {"unitPrice", outcome.result.unitPrice},
                {"presentValue", outcome.result.presentValue},
                {"unitGreeks", greeksJson(outcome.result.unitGreeks)},
                {"positionGreeks", greeksJson(outcome.result.positionGreeks)}}},
            {"conventions", {
                {"delta", "per 1.00 spot move"},
                {"gamma", "delta change per 1.00 spot move"},
                {"vega", "per 1 volatility-point move"},
                {"theta", "per calendar day"},
                {"rho", "per 1 basis-point rate move"}}},
            {"latencyNanoseconds", {
                {"marketAndTradeLoad", outcome.latency.loadNanoseconds},
                {"calculation", outcome.latency.computeNanoseconds},
                {"totalApplication", outcome.latency.totalNanoseconds}}}
        };
    }

    const char* openApiDocument()
    {
        return R"json({
  "openapi": "3.0.3",
  "info": {
    "title": "AutomatedTrading Risk Service",
    "version": "1.0.0",
    "description": "Calculates theoretical and position-scaled Greeks for persisted trades."
  },
  "paths": {
    "/health/live": {"get": {"summary": "Process liveness", "responses": {"200": {"description": "Live"}}}},
    "/health/ready": {"get": {"summary": "Database readiness", "responses": {"200": {"description": "Ready"}, "503": {"description": "Unavailable"}}}},
    "/api/v1/risk/capabilities": {
      "get": {"summary": "Available risk engines", "responses": {"200": {"description": "Capabilities"}}}
    },
    "/api/v1/risk/greeks": {
      "post": {
        "summary": "Calculate Greeks for a persisted trade",
        "operationId": "calculateTradeGreeks",
        "requestBody": {
          "required": true,
          "content": {"application/json": {
            "schema": {"$ref": "#/components/schemas/GreekRequest"},
            "example": {
              "tradeId": "90000000-0000-0000-0000-000000000001",
              "valuationDate": "2026-01-01T00:00:00Z",
              "method": "analytic"
            }
          }}
        },
        "responses": {
          "200": {"description": "Unit and position Greeks"},
          "400": {"description": "Invalid request or unavailable method"},
          "404": {"description": "Trade or market snapshot not found"},
          "500": {"description": "Risk calculation failed"}
        }
      }
    }
  },
  "components": {"schemas": {
    "GreekRequest": {
      "type": "object",
      "required": ["tradeId", "valuationDate"],
      "properties": {
        "tradeId": {"type": "string", "format": "uuid"},
        "valuationDate": {"type": "string", "format": "date-time"},
        "method": {"type": "string", "enum": ["analytic"], "default": "analytic"}
      }
    }
  }}
})json";
    }

    const char* swaggerPage()
    {
        return R"html(<!doctype html>
<html lang="en"><head><meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>AutomatedTrading Risk API</title>
<link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui.css"/></head>
<body><div id="swagger-ui"></div><script src="https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui-bundle.js"></script>
<script>window.onload=()=>SwaggerUIBundle({url:'/openapi.json',dom_id:'#swagger-ui',deepLinking:true});</script></body></html>)html";
    }
}

int main()
{
    try {
        auto databaseConfig = automated_trading::database::PostgresConnectionConfig::fromEnvironment();
        databaseConfig.applicationName = "RiskService";
        automated_trading::database::PostgresConnection connection(std::move(databaseConfig));
        automated_trading::database::QueryService queries(connection);
        automated_trading::services::pricing::PostgresTradePricingRepository repository(queries);
        automated_trading::analytics::risk::black_scholes::AnalyticBlackScholesRiskEngine engine;
        automated_trading::services::risk::RiskApplicationService riskService(repository, engine);

        httplib::Server server;
        const int workerThreads = positiveInteger("RISK_SERVICE_THREADS", 4, 256);
        server.new_task_queue = [workerThreads] { return new httplib::ThreadPool(workerThreads); };
        server.set_payload_max_length(64 * 1024);

        server.Get("/", [](const httplib::Request&, httplib::Response& response) { response.set_redirect("/swagger"); });
        server.Get("/swagger", [](const httplib::Request&, httplib::Response& response) {
            response.set_content(swaggerPage(), "text/html; charset=utf-8");
        });
        server.Get("/openapi.json", [](const httplib::Request&, httplib::Response& response) {
            response.set_content(openApiDocument(), "application/json; charset=utf-8");
        });
        server.Get("/health/live", [](const httplib::Request&, httplib::Response& response) {
            jsonResponse(response, 200, {{"status", "live"}, {"service", "risk"}});
        });
        server.Get("/health/ready", [&](const httplib::Request&, httplib::Response& response) {
            const bool ready = riskService.isReady();
            jsonResponse(response, ready ? 200 : 503,
                {{"status", ready ? "ready" : "not_ready"}, {"service", "risk"},
                 {"database", ready ? "connected" : "unavailable"}});
        });
        server.Get("/api/v1/risk/capabilities", [&](const httplib::Request&, httplib::Response& response) {
            jsonResponse(response, 200, {{"engines", Json::array({{
                {"model", engine.model()}, {"method", engine.method()},
                {"computeBackend", engine.computeBackend()}, {"batchSupported", true}}})},
                {"workerThreads", workerThreads}});
        });
        server.Post("/api/v1/risk/greeks", [&](const httplib::Request& request, httplib::Response& response) {
            try {
                const Json payload = Json::parse(request.body);
                if (!payload.contains("tradeId") || !payload["tradeId"].is_string() ||
                    !payload.contains("valuationDate") || !payload["valuationDate"].is_string()) {
                    jsonResponse(response, 400, {{"error", "tradeId and valuationDate are required strings."}});
                    return;
                }
                const auto outcome = riskService.calculate(
                    payload["tradeId"].get<std::string>(),
                    payload["valuationDate"].get<std::string>(),
                    payload.value("method", "analytic"));
                jsonResponse(response, 200, outcomeJson(outcome));
            }
            catch (const nlohmann::json::exception& error) {
                jsonResponse(response, 400, {{"error", "Invalid JSON payload."}, {"detail", error.what()}});
            }
            catch (const automated_trading::database::DatabaseError& error) {
                jsonResponse(response, 404, {{"error", error.what()}});
            }
            catch (const std::invalid_argument& error) {
                jsonResponse(response, 400, {{"error", error.what()}});
            }
            catch (const std::exception& error) {
                jsonResponse(response, 500, {{"error", "Risk calculation failed."}, {"detail", error.what()}});
            }
        });

        const std::string host = environmentValue("RISK_SERVICE_HOST", "0.0.0.0");
        const int port = positiveInteger("RISK_SERVICE_PORT", 8081, 65535);
        std::cout << "[risk-service] listening=http://" << host << ':' << port << '\n'
                  << "[risk-service] swagger=http://localhost:" << port << "/swagger\n"
                  << "[risk-service] engine=" << engine.model() << '/' << engine.method()
                  << " backend=" << engine.computeBackend() << " threads=" << workerThreads << '\n';
        if (!server.listen(host, port)) {
            std::cerr << "[risk-service] failed to bind HTTP listener\n";
            return 1;
        }
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "[risk-service] startup failed: " << error.what() << '\n';
        return 1;
    }
}
