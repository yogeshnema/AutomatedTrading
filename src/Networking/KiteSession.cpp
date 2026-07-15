#include "Common/KiteSession.h"

#include <fstream>
#include <stdexcept>
#include <utility>
#include <nlohmann/json.hpp>

KiteSession::KiteSession(std::string apiKey, std::string accessToken)
    : apiKey_(std::move(apiKey)), accessToken_(std::move(accessToken))
{
    if (apiKey_.empty()) {
        throw std::runtime_error("Kite API key is missing.");
    }

    if (accessToken_.empty()) {
        throw std::runtime_error("Kite access token is missing.");
    }
}

KiteSession KiteSession::fromConfigFile(const std::string& configPath)
{
    std::ifstream input(configPath);
    if (!input) {
        throw std::runtime_error("Unable to open config file: " + configPath);
    }

    auto config = nlohmann::json::parse(input);
    const auto kite = config.at("kite");

    return KiteSession(
        kite.at("api_key").get<std::string>(),
        kite.at("access_token").get<std::string>());
}

const std::string& KiteSession::apiKey() const
{
    return apiKey_;
}

const std::string& KiteSession::accessToken() const
{
    return accessToken_;
}

cpr::Header KiteSession::commonHeaders() const
{
    return cpr::Header{
        {"X-Kite-Version", "3"},
        {"Authorization", "token " + apiKey_ + ":" + accessToken_}
    };
}
