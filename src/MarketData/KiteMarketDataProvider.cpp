#include "MarketData/KiteMarketDataProvider.h"

#include <cstddef>
#include <stdexcept>

KiteMarketDataProvider::KiteMarketDataProvider(const std::string& configPath)
    : IMarketDataProvider("kite"),
      session_(KiteSession::fromConfigFile(configPath)),
      service_(session_)
{
}

std::vector<Candle> KiteMarketDataProvider::getHistoricalCandles(
    const std::string& instrument,
    const std::string& from,
    const std::string& to,
    const std::string& interval)
{
    std::size_t parsedCharacters = 0;
    long instrumentToken = 0;

    try {
        instrumentToken = std::stol(instrument, &parsedCharacters);
    }
    catch (const std::exception&) {
        throw std::invalid_argument(
            "Kite instrument must be a numeric instrument token: " + instrument);
    }

    if (parsedCharacters != instrument.size()) {
        throw std::invalid_argument(
            "Kite instrument must be a numeric instrument token: " + instrument);
    }

    return service_.getHistoricalCandles(
        instrumentToken, from, to, interval);
}
