#include "QuoteService.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    std::string urlEncode(const std::string& value)
    {
        std::ostringstream encoded;
        encoded << std::hex << std::uppercase;

        for (const auto character : value) {
            if (std::isalnum(static_cast<unsigned char>(character)) ||
                character == '-' ||
                character == '_' ||
                character == '.' ||
                character == '~') {
                encoded << character;
            }
            else {
                encoded << '%' << std::setw(2) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(character));
            }
        }

        return encoded.str();
    }
}

QuoteService::QuoteService(const KiteSession& session)
    : session_(session)
{
}

std::vector<OptionQuote> QuoteService::getQuotes(
    const std::vector<OptionInstrument>& instruments) const
{
    std::string url = "https://api.kite.trade/quote";
    for (const auto& instrument : instruments) {
        const auto key = instrument.exchange + ":" + instrument.tradingSymbol;
        url += url.find('?') == std::string::npos ? "?i=" : "&i=";
        url += urlEncode(key);
    }

    auto response = cpr::Get(
        cpr::Url{ url },
        session_.commonHeaders());

    if (response.status_code != 200) {
        throw std::runtime_error(
            "Kite quote API error: HTTP " +
            std::to_string(response.status_code) +
            "\n" + response.text);
    }

    const auto json = nlohmann::json::parse(response.text);
    std::vector<OptionQuote> quotes;

    for (const auto& instrument : instruments) {
        const auto key = instrument.exchange + ":" + instrument.tradingSymbol;
        if (!json["data"].contains(key)) {
            continue;
        }

        const auto quoteJson = json["data"][key];
        OptionQuote quote{};
        quote.instrumentToken = instrument.instrumentToken;
        quote.tradingSymbol = instrument.tradingSymbol;
        quote.lastPrice = quoteJson.value("last_price", 0.0);

        if (quoteJson.contains("depth")) {
            const auto depth = quoteJson["depth"];
            if (depth.contains("buy") && !depth["buy"].empty()) {
                quote.bidPrice = depth["buy"][0].value("price", 0.0);
            }

            if (depth.contains("sell") && !depth["sell"].empty()) {
                quote.askPrice = depth["sell"][0].value("price", 0.0);
            }
        }

        quotes.push_back(quote);
    }

    return quotes;
}

IndexQuote QuoteService::getIndexQuote(
    const std::string& exchange,
    const std::string& tradingSymbol) const
{
    const auto key = exchange + ":" + tradingSymbol;
    const auto url = "https://api.kite.trade/quote?i=" + urlEncode(key);

    auto response = cpr::Get(
        cpr::Url{ url },
        session_.commonHeaders());

    if (response.status_code != 200) {
        throw std::runtime_error(
            "Kite quote API error: HTTP " +
            std::to_string(response.status_code) +
            "\n" + response.text);
    }

    const auto json = nlohmann::json::parse(response.text);
    if (!json["data"].contains(key)) {
        throw std::runtime_error("Kite quote response did not contain " + key);
    }

    return IndexQuote{
        exchange,
        tradingSymbol,
        json["data"][key].value("last_price", 0.0) };
}
