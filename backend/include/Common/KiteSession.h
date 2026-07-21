#pragma once

#include <cpr/cpr.h>
#include <string>

class KiteSession
{
public:
    KiteSession(std::string apiKey, std::string accessToken);

    static KiteSession fromConfigFile(const std::string& configPath);
    static KiteSession fromEnvironment();

    const std::string& apiKey() const;
    const std::string& accessToken() const;

    cpr::Header commonHeaders() const;

private:
    std::string apiKey_;
    std::string accessToken_;
};
