#include "Analytics/Pricing/BlackScholes/BlackScholesPricer.h"
#include "Database/PostgresConnection.h"
#include "Database/QueryService.h"
#include "Services/Pricing/PostgresTradePricingRepository.h"
#include "Services/Pricing/PricingApplicationService.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <charconv>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace
{
    using Json = nlohmann::json;

    std::string environmentValue(const char* name, std::string fallback)
    {
        const char* value = std::getenv(name);
        return value == nullptr || *value == '\0' ? std::move(fallback) : std::string(value);
    }

    int servicePort()
    {
        const std::string value = environmentValue("PRICING_SERVICE_PORT", "8080");
        int port = 0;
        const auto conversion = std::from_chars(value.data(), value.data() + value.size(), port);
        if (conversion.ec != std::errc{} || port <= 0 || port > 65535) {
            throw std::runtime_error("PRICING_SERVICE_PORT must be between 1 and 65535.");
        }
        return port;
    }

    void jsonResponse(httplib::Response& response, int status, const Json& body)
    {
        response.status = status;
        response.set_content(body.dump(2), "application/json");
    }

    Json outcomeJson(const automated_trading::services::pricing::TradePricingOutcome& outcome)
    {
        return {
            {"tradeId", outcome.tradeId},
            {"valuationDate", outcome.valuationDate},
            {"productType", outcome.productType},
            {"underlyingAssetId", outcome.underlyingAssetId},
            {"currency", outcome.currency},
            {"inputs", {
                {"signedQuantity", outcome.signedQuantity},
                {"contractMultiplier", outcome.contractMultiplier},
                {"spot", outcome.spot},
                {"strike", outcome.strike},
                {"riskFreeRate", outcome.riskFreeRate},
                {"dividendYield", outcome.dividendYield},
                {"volatility", outcome.volatility}}},
            {"result", {
                {"model", outcome.result.model},
                {"unitPrice", outcome.result.unitPrice},
                {"presentValue", outcome.result.presentValue},
                {"delta", outcome.result.delta},
                {"gamma", outcome.result.gamma},
                {"vega", outcome.result.vega},
                {"theta", outcome.result.theta},
                {"rho", outcome.result.rho}}}
        };
    }

    const char* openApiDocument()
    {
        return R"json({
  "openapi": "3.0.3",
  "info": {
    "title": "AutomatedTrading Pricing Service",
    "version": "1.0.0",
    "description": "Prices persisted trades using versioned PostgreSQL market snapshots."
  },
  "servers": [{"url": "/"}],
  "paths": {
    "/health/live": {
      "get": {
        "summary": "Process liveness",
        "responses": {"200": {"description": "Service is running"}}
      }
    },
    "/health/ready": {
      "get": {
        "summary": "Database readiness",
        "responses": {
          "200": {"description": "Service and database are ready"},
          "503": {"description": "Database is unavailable"}
        }
      }
    },
    "/api/v1/prices": {
      "post": {
        "summary": "Price a persisted trade",
        "operationId": "priceTrade",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {"$ref": "#/components/schemas/PriceTradeRequest"},
              "example": {
                "tradeId": "90000000-0000-0000-0000-000000000001",
                "valuationDate": "2026-01-01T00:00:00Z"
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Price and Greeks",
            "content": {"application/json": {"schema": {"type": "object"}}}
          },
          "400": {"description": "Invalid request"},
          "404": {"description": "Trade or market snapshot not found"},
          "500": {"description": "Pricing failure"}
        }
      }
    }
  },
  "components": {
    "schemas": {
      "PriceTradeRequest": {
        "type": "object",
        "required": ["tradeId", "valuationDate"],
        "properties": {
          "tradeId": {"type": "string", "format": "uuid"},
          "valuationDate": {"type": "string", "format": "date-time"}
        }
      }
    }
  }
})json";
    }

    const char* swaggerPage()
    {
        return R"html(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>AutomatedTrading Pricing API</title>
  <link rel="stylesheet" href="https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui.css" />
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui-bundle.js"></script>
  <script>
    window.onload = () => SwaggerUIBundle({
      url: '/openapi.json',
      dom_id: '#swagger-ui',
      deepLinking: true
    });
  </script>
</body>
</html>)html";
    }
}

int main()
{
    try {
        automated_trading::database::PostgresConnection connection(
            automated_trading::database::PostgresConnectionConfig::fromEnvironment());
        automated_trading::database::QueryService queryService(connection);
        automated_trading::services::pricing::PostgresTradePricingRepository repository(queryService);
        automated_trading::analytics::pricing::black_scholes::BlackScholesPricer pricer;
        automated_trading::services::pricing::PricingApplicationService pricingService(repository, pricer);

        httplib::Server server;
        server.set_payload_max_length(1024 * 1024);

        server.Get("/", [](const httplib::Request&, httplib::Response& response) {
            response.set_redirect("/swagger");
        });
        server.Get("/swagger", [](const httplib::Request&, httplib::Response& response) {
            response.set_content(swaggerPage(), "text/html; charset=utf-8");
        });
        server.Get("/openapi.json", [](const httplib::Request&, httplib::Response& response) {
            response.set_content(openApiDocument(), "application/json");
        });
        server.Get("/health/live", [](const httplib::Request&, httplib::Response& response) {
            jsonResponse(response, 200, {{"status", "live"}, {"service", "pricing"}});
        });
        server.Get("/health/ready", [&](const httplib::Request&, httplib::Response& response) {
            const bool ready = pricingService.isReady();
            jsonResponse(response, ready ? 200 : 503, {
                {"status", ready ? "ready" : "not_ready"},
                {"service", "pricing"},
                {"database", ready ? "connected" : "unavailable"}});
        });
        server.Post("/api/v1/prices", [&](const httplib::Request& request, httplib::Response& response) {
            try {
                const Json payload = Json::parse(request.body);
                if (!payload.contains("tradeId") || !payload["tradeId"].is_string() ||
                    !payload.contains("valuationDate") || !payload["valuationDate"].is_string()) {
                    jsonResponse(response, 400, {{"error", "tradeId and valuationDate are required strings."}});
                    return;
                }

                const auto outcome = pricingService.price(
                    payload["tradeId"].get<std::string>(),
                    payload["valuationDate"].get<std::string>());
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
                jsonResponse(response, 500, {{"error", "Pricing failed."}, {"detail", error.what()}});
            }
        });

        const std::string host = environmentValue("PRICING_SERVICE_HOST", "0.0.0.0");
        const int port = servicePort();
        std::cout << "[pricing-service] listening=http://" << host << ':' << port << '\n'
                  << "[pricing-service] swagger=http://localhost:" << port << "/swagger\n";
        if (!server.listen(host, port)) {
            std::cerr << "[pricing-service] failed to bind HTTP listener\n";
            return 1;
        }
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "[pricing-service] startup failed: " << error.what() << '\n';
        return 1;
    }
}
