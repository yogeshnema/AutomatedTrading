#pragma once

#include "Common/KiteSession.h"
#include "Common/VolArbitrageModels.h"

#include <string>
#include <vector>

class QuoteService
{
public:
    explicit QuoteService(const KiteSession& session);

    std::vector<OptionQuote> getQuotes(
        const std::vector<OptionInstrument>& instruments) const;

    IndexQuote getIndexQuote(
        const std::string& exchange,
        const std::string& tradingSymbol) const;

private:
    const KiteSession& session_;
};
