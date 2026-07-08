#include "BacktestService.h"

#include "ImpliedVolService.h"

#include <cmath>
#include <iostream>
#include <numeric>
#include <string>
#include <unordered_map>

namespace
{
    struct MinuteIvPoint
    {
        const OptionInstrument* instrument;
        double optionPrice;
        double impliedVolatility;
        double zScore;
    };

    std::unordered_map<std::string, Candle> buildCandlesByTimestamp(
        const std::vector<Candle>& candles)
    {
        std::unordered_map<std::string, Candle> result;
        for (const auto& candle : candles) {
            result[candle.timestamp] = candle;
        }

        return result;
    }

    std::vector<MinuteIvPoint> calculateMinuteIvPoints(
        const std::vector<OptionInstrument>& instruments,
        const std::map<long, std::unordered_map<std::string, Candle>>& optionCandlesByToken,
        const std::string& timestamp,
        double spotPrice,
        double indiaVix,
        double yearsToExpiry,
        double riskFreeRate)
    {
        ImpliedVolService impliedVolService;
        std::vector<MinuteIvPoint> points;

        for (const auto& instrument : instruments) {
            const auto tokenCandles = optionCandlesByToken.find(instrument.instrumentToken);
            if (tokenCandles == optionCandlesByToken.end()) {
                continue;
            }

            const auto candle = tokenCandles->second.find(timestamp);
            if (candle == tokenCandles->second.end() || candle->second.close <= 0.0) {
                continue;
            }

            try {
                const double iv = impliedVolService.calculate(
                    instrument.optionType,
                    candle->second.close,
                    spotPrice,
                    instrument.strike,
                    yearsToExpiry,
                    riskFreeRate);

                points.push_back(MinuteIvPoint{
                    &instrument,
                    candle->second.close,
                    iv,
                    0.0 });
            }
            catch (const std::exception&) {
                continue;
            }
        }

        if (points.size() < 2 || indiaVix <= 0.0) {
            return points;
        }

        std::vector<double> differences;
        differences.reserve(points.size());
        const double vixVol = indiaVix / 100.0;

        for (const auto& point : points) {
            differences.push_back(point.impliedVolatility - vixVol);
        }

        const double mean = std::accumulate(differences.begin(), differences.end(), 0.0) /
            static_cast<double>(differences.size());

        double variance = 0.0;
        for (const auto difference : differences) {
            variance += (difference - mean) * (difference - mean);
        }

        const double stdDev = std::sqrt(variance / static_cast<double>(differences.size() - 1));
        if (stdDev == 0.0) {
            return points;
        }

        for (size_t index = 0; index < points.size(); ++index) {
            points[index].zScore = (differences[index] - mean) / stdDev;
        }

        return points;
    }

    std::string optionTypeText(OptionType optionType)
    {
        return optionType == OptionType::Call ? "CE" : "PE";
    }
}

    std::vector<BacktestTrade> BacktestService::runSingleDay(
    const std::vector<OptionInstrument>& instruments,
    const std::map<long, std::vector<Candle>>& optionCandlesByToken,
    const std::vector<Candle>& spotCandles,
    const std::vector<Candle>& vixCandles,
    int daysToExpiry,
    double riskFreeRate,
    double zScoreThreshold,
    int holdingPeriodMinutes) const
{
    std::vector<BacktestTrade> trades;
    if (instruments.empty() || spotCandles.empty() || vixCandles.empty() || holdingPeriodMinutes <= 0) {
        return trades;
    }

    std::map<long, std::unordered_map<std::string, Candle>> optionCandlesByTimestamp;
    for (const auto& [token, candles] : optionCandlesByToken) {
        optionCandlesByTimestamp[token] = buildCandlesByTimestamp(candles);
    }

    const auto vixCandlesByTimestamp = buildCandlesByTimestamp(vixCandles);
    std::unordered_map<long, size_t> nextAllowedEntryIndexByToken;
    const double yearsToExpiry = static_cast<double>(daysToExpiry) / 365.0;

    for (size_t index = 0; index + static_cast<size_t>(holdingPeriodMinutes) < spotCandles.size(); ++index) {
        const auto& spotCandle = spotCandles[index];
        const auto vixCandle = vixCandlesByTimestamp.find(spotCandle.timestamp);
        if (vixCandle == vixCandlesByTimestamp.end() || vixCandle->second.close <= 0.0) {
            continue;
        }

        const double indiaVix = vixCandle->second.close;
        const auto points = calculateMinuteIvPoints(
            instruments,
            optionCandlesByTimestamp,
            spotCandle.timestamp,
            spotCandle.close,
            indiaVix,
            yearsToExpiry,
            riskFreeRate);

        if (points.size() < 2) {
            continue;
        }

        const size_t exitIndex = index + static_cast<size_t>(holdingPeriodMinutes);
        const auto& exitSpotCandle = spotCandles[exitIndex];

        for (const auto& point : points) {
            if (std::abs(point.zScore) < zScoreThreshold) {
                continue;
            }

            const auto nextAllowed = nextAllowedEntryIndexByToken.find(point.instrument->instrumentToken);
            if (nextAllowed != nextAllowedEntryIndexByToken.end() && index < nextAllowed->second) {
                continue;
            }

            const auto tokenCandles = optionCandlesByTimestamp.find(point.instrument->instrumentToken);
            if (tokenCandles == optionCandlesByTimestamp.end()) {
                continue;
            }

            const auto exitCandle = tokenCandles->second.find(exitSpotCandle.timestamp);
            if (exitCandle == tokenCandles->second.end() || exitCandle->second.close <= 0.0) {
                continue;
            }

            const bool isRich = point.zScore > 0.0;
            const std::string side = isRich ? "SELL_RICH_IV" : "BUY_CHEAP_IV";
            const double pnl = isRich ?
                point.optionPrice - exitCandle->second.close :
                exitCandle->second.close - point.optionPrice;

            trades.push_back(BacktestTrade{
                spotCandle.timestamp,
                exitSpotCandle.timestamp,
                point.instrument->tradingSymbol,
                point.instrument->expiry,
                point.instrument->strike,
                point.instrument->optionType,
                side,
                point.optionPrice,
                exitCandle->second.close,
                point.impliedVolatility,
                indiaVix,
                point.zScore,
                pnl });

            nextAllowedEntryIndexByToken[point.instrument->instrumentToken] = exitIndex + 1;

            std::cout << "[backtest-trade] " << spotCandle.timestamp
                << " " << side
                << " " << point.instrument->tradingSymbol
                << " " << optionTypeText(point.instrument->optionType)
                << " K=" << point.instrument->strike
                << " entry=" << point.optionPrice
                << " exit=" << exitCandle->second.close
                << " z=" << point.zScore
                << " pnl=" << pnl
                << "\n";
        }
    }

    return trades;
}

BacktestSummary BacktestService::summarize(const std::vector<BacktestTrade>& trades) const
{
    BacktestSummary summary{};
    summary.tradeCount = static_cast<int>(trades.size());

    for (const auto& trade : trades) {
        summary.totalPnl += trade.pnl;
        if (trade.pnl > 0.0) {
            ++summary.winners;
        }
        else if (trade.pnl < 0.0) {
            ++summary.losers;
        }
    }

    if (!trades.empty()) {
        summary.averagePnl = summary.totalPnl / static_cast<double>(trades.size());
    }

    return summary;
}
