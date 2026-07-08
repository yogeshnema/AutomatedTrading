#pragma once

#include "KiteMarketDataService.h"
#include "VolArbitrageModels.h"

#include <map>
#include <vector>

class BacktestService
{
public:
    std::vector<BacktestTrade> runSingleDay(
        const std::vector<OptionInstrument>& instruments,
        const std::map<long, std::vector<Candle>>& optionCandlesByToken,
        const std::vector<Candle>& spotCandles,
        const std::vector<Candle>& vixCandles,
        int daysToExpiry,
        double riskFreeRate,
        double zScoreThreshold,
        int holdingPeriodMinutes) const;

    BacktestSummary summarize(const std::vector<BacktestTrade>& trades) const;
};
