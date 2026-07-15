#include "MarketData/IMarketDataProvider.h"

#include "MarketData/KiteMarketDataProvider.h"
#include "MarketData/PolygonMarketDataProvider.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

IMarketDataProvider::IMarketDataProvider(std::string providerName)
    : providerName_(std::move(providerName))
{
}

const std::string& IMarketDataProvider::providerName() const noexcept
{
    return providerName_;
}

std::unique_ptr<IMarketDataProvider> IMarketDataProvider::create(
    const std::string& providerName,
    const std::string& configPath)
{
    std::string normalized = providerName;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });

    if (normalized == "kite") {
        return std::make_unique<KiteMarketDataProvider>(configPath);
    }

    if (normalized == "polygon") {
        return std::make_unique<PolygonMarketDataProvider>(configPath);
    }

    throw std::invalid_argument(
        "Unsupported market data provider '" + providerName +
        "'. Expected 'kite' or 'polygon'.");
}
