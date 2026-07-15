#pragma once

#include <vector>
#include <string>

enum class OptionType
{
    Call,
    Put
};

struct OptionInstrument
{
    long instrumentToken;
    std::string exchange;
    std::string tradingSymbol;
    std::string name;
    std::string expiry;
    double strike;
    OptionType optionType;
};

struct OptionQuote
{
    long instrumentToken;
    std::string tradingSymbol;
    double lastPrice;
    double bidPrice;
    double askPrice;
};

struct IndexQuote
{
    std::string exchange;
    std::string tradingSymbol;
    double lastPrice;
};

struct ForwardPrice
{
    double strike;
    double callPrice;
    double putPrice;
    double forwardPrice;
    double impliedSpotPrice;
};

struct ImpliedVolPoint
{
    OptionInstrument instrument;
    double underlyingPrice;
    double optionPrice;
    double yearsToExpiry;
    double impliedVolatility;
};

struct VolSurfacePoint
{
    std::string expiry;
    double strike;
    OptionType optionType;
    double impliedVolatility;
};

struct VolSignal
{
    std::string name;
    std::string expiry;
    double strike;
    OptionType optionType;
    double impliedVolatility;
    double referenceVolatility;
    double zScore;
    std::string direction;
};

struct BacktestTrade
{
    std::string entryTimestamp;
    std::string exitTimestamp;
    std::string tradingSymbol;
    std::string expiry;
    double strike;
    OptionType optionType;
    std::string side;
    double entryPrice;
    double exitPrice;
    double entryIv;
    double entryVix;
    double entryZScore;
    double pnl;
};

struct BacktestSummary
{
    int tradeCount;
    int winners;
    int losers;
    double totalPnl;
    double averagePnl;
};
