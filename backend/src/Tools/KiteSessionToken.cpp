#include "Common/Environment.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    std::string required(const char* name)
    {
        const auto value = automated_trading::common::environmentValue(name);
        if (!value) throw std::runtime_error(std::string(name) + " is required.");
        return *value;
    }

    std::string sha256(const std::string& value)
    {
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), digest);
        std::ostringstream output;
        for (const auto byte : digest) output << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        return output.str();
    }
}

int main(int argc, char** argv)
{
    try {
        const auto apiKey = required("KITE_API_KEY");
        const auto apiSecret = required("KITE_API_SECRET");
        const auto requestToken = argc > 1 ? std::string(argv[1]) : required("KITE_REQUEST_TOKEN");
        const auto response = cpr::Post(cpr::Url{"https://api.kite.trade/session/token"},
            cpr::Header{{"X-Kite-Version", "3"}},
            cpr::Payload{{"api_key", apiKey}, {"request_token", requestToken},
                         {"checksum", sha256(apiKey + requestToken + apiSecret)}});
        if (response.status_code != 200) throw std::runtime_error("Kite session exchange failed: HTTP " + std::to_string(response.status_code) + " " + response.text);
        const auto body = nlohmann::json::parse(response.text);
        std::cout << "export KITE_ACCESS_TOKEN='" << body.at("data").at("access_token").get<std::string>() << "'\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "kite-session-token: " << error.what() << '\n';
        return 1;
    }
}
