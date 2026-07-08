#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "BacktestService.h"
#include "ForwardPriceService.h"
#include "ImpliedVolService.h"
#include "KiteInstrumentService.h"
#include "KiteMarketDataService.h"
#include "KiteSession.h"
#include "OptionChainService.h"
#include "QuoteService.h"
#include "SignalService.h"
#include "VolSurfaceService.h"

namespace
{
    struct VolArbitrageConfig
    {
        std::string expiry;
        double spotPrice;
        bool useLiveSpot;
        std::string spotExchange;
        std::string spotSymbol;
        double indiaVix;
        bool useLiveIndiaVix;
        std::string indiaVixExchange;
        std::string indiaVixSymbol;
        int daysToExpiry;
        double riskFreeRate;
        int strikesEachSide;
        double zScoreThreshold;
        bool backtestEnabled;
        long backtestSpotToken;
        std::string backtestFrom;
        std::string backtestTo;
        std::string backtestInterval;
        int backtestHoldingPeriodMinutes;
        long backtestVixToken;
    };

    VolArbitrageConfig loadVolArbitrageConfig(const std::string& configPath)
    {
        std::ifstream input(configPath);
        if (!input) {
            throw std::runtime_error("Unable to open config file: " + configPath);
        }

        const auto config = nlohmann::json::parse(input);
        const auto strategy = config.at("vol_arbitrage");

        return VolArbitrageConfig{
            strategy.at("expiry").get<std::string>(),
            strategy.at("spot_price").get<double>(),
            strategy.value("use_live_spot", true),
            strategy.value("spot_exchange", std::string("NSE")),
            strategy.value("spot_symbol", std::string("NIFTY 50")),
            strategy.at("india_vix").get<double>(),
            strategy.value("use_live_india_vix", true),
            strategy.value("india_vix_exchange", std::string("NSE")),
            strategy.value("india_vix_symbol", std::string("INDIA VIX")),
            strategy.at("days_to_expiry").get<int>(),
            strategy.at("risk_free_rate").get<double>(),
            strategy.at("strikes_each_side").get<int>(),
            strategy.at("z_score_threshold").get<double>(),
            strategy.value("backtest_enabled", false),
            strategy.value("backtest_spot_token", 256265L),
            strategy.value("backtest_from", std::string("2026-07-08 09:15:00")),
            strategy.value("backtest_to", std::string("2026-07-08 15:30:00")),
            strategy.value("backtest_interval", std::string("minute")),
            strategy.value("backtest_holding_period_minutes", 15),
            strategy.value("backtest_vix_token", 264969L) };
    }

    double quotePrice(const OptionQuote& quote)
    {
        if (quote.bidPrice > 0.0 && quote.askPrice > 0.0) {
            return (quote.bidPrice + quote.askPrice) / 2.0;
        }

        return quote.lastPrice;
    }

    std::string optionTypeText(OptionType optionType)
    {
        return optionType == OptionType::Call ? "CE" : "PE";
    }

    void logConfig(const VolArbitrageConfig& config)
    {
        std::cout << "[config] expiry=" << config.expiry
            << ", spot=" << config.spotPrice
            << ", use_live_spot=" << (config.useLiveSpot ? "true" : "false")
            << ", spot_quote=" << config.spotExchange << ":" << config.spotSymbol
            << ", india_vix=" << config.indiaVix
            << ", use_live_india_vix=" << (config.useLiveIndiaVix ? "true" : "false")
            << ", india_vix_quote=" << config.indiaVixExchange << ":" << config.indiaVixSymbol
            << ", days_to_expiry=" << config.daysToExpiry
            << ", risk_free_rate=" << config.riskFreeRate
            << ", strikes_each_side=" << config.strikesEachSide
            << ", z_score_threshold=" << config.zScoreThreshold
            << ", backtest_enabled=" << (config.backtestEnabled ? "true" : "false")
            << "\n";
    }

    void logAvailableExpiries(const std::vector<OptionInstrument>& options)
    {
        std::map<std::string, int> countsByExpiry;
        for (const auto& option : options) {
            ++countsByExpiry[option.expiry];
        }

        std::cout << "[instrument] available expiries=" << countsByExpiry.size() << "\n";

        int printed = 0;
        for (const auto& [expiry, count] : countsByExpiry) {
            std::cout << "  expiry " << expiry << " -> " << count << " contracts\n";
            ++printed;

            if (printed >= 10 && static_cast<int>(countsByExpiry.size()) > printed) {
                std::cout << "  ... " << countsByExpiry.size() - printed
                    << " more expiries not shown\n";
                break;
            }
        }
    }

    void logOptionSamples(const std::vector<OptionInstrument>& options, const std::string& label)
    {
        std::cout << "[" << label << "] sample contracts\n";

        const auto sampleCount = std::min<size_t>(options.size(), 5);
        for (size_t index = 0; index < sampleCount; ++index) {
            const auto& option = options[index];
            std::cout << "  " << option.tradingSymbol
                << " token=" << option.instrumentToken
                << " expiry=" << option.expiry
                << " strike=" << option.strike
                << " type=" << optionTypeText(option.optionType)
                << "\n";
        }

        if (sampleCount == 0) {
            std::cout << "  none\n";
        }
    }

    std::unordered_map<long, double> calculateZScores(
        const std::vector<ImpliedVolPoint>& impliedVols,
        double indiaVix)
    {
        std::unordered_map<long, double> zScoresByToken;
        if (impliedVols.size() < 2 || indiaVix <= 0.0) {
            return zScoresByToken;
        }

        std::vector<double> differences;
        differences.reserve(impliedVols.size());

        const double vixVol = indiaVix / 100.0;
        for (const auto& point : impliedVols) {
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
            return zScoresByToken;
        }

        for (size_t index = 0; index < impliedVols.size(); ++index) {
            zScoresByToken[impliedVols[index].instrument.instrumentToken] =
                (differences[index] - mean) / stdDev;
        }

        return zScoresByToken;
    }

    void logContractCalculations(
        const std::vector<ImpliedVolPoint>& impliedVols,
        const std::unordered_map<long, OptionQuote>& quotesByToken,
        double indiaVix,
        double zScoreThreshold)
    {
        const auto zScoresByToken = calculateZScores(impliedVols, indiaVix);
        const double vixVol = indiaVix / 100.0;

        std::cout << "[detail] contract calculations\n";
        std::cout
            << std::left
            << std::setw(22) << "symbol"
            << std::setw(6) << "type"
            << std::right
            << std::setw(9) << "strike"
            << std::setw(10) << "last"
            << std::setw(10) << "bid"
            << std::setw(10) << "ask"
            << std::setw(12) << "used_px"
            << std::setw(10) << "iv%"
            << std::setw(10) << "vix%"
            << std::setw(12) << "iv-vix%"
            << std::setw(10) << "z"
            << " status"
            << "\n";

        for (const auto& point : impliedVols) {
            const auto quote = quotesByToken.find(point.instrument.instrumentToken);
            if (quote == quotesByToken.end()) {
                continue;
            }

            const auto zScore = zScoresByToken.find(point.instrument.instrumentToken);
            const bool hasZScore = zScore != zScoresByToken.end();
            const double zScoreValue = hasZScore ? zScore->second : 0.0;
            const double ivMinusVix = point.impliedVolatility - vixVol;

            std::string status = "inside_threshold";
            if (hasZScore && std::abs(zScoreValue) >= zScoreThreshold) {
                status = zScoreValue > 0.0 ? "IV_RICH_VS_VIX" : "IV_CHEAP_VS_VIX";
            }

            std::cout
                << std::left
                << std::setw(22) << point.instrument.tradingSymbol
                << std::setw(6) << optionTypeText(point.instrument.optionType)
                << std::right
                << std::fixed << std::setprecision(2)
                << std::setw(9) << point.instrument.strike
                << std::setw(10) << quote->second.lastPrice
                << std::setw(10) << quote->second.bidPrice
                << std::setw(10) << quote->second.askPrice
                << std::setw(12) << point.optionPrice
                << std::setw(10) << point.impliedVolatility * 100.0
                << std::setw(10) << indiaVix
                << std::setw(12) << ivMinusVix * 100.0;

            if (hasZScore) {
                std::cout << std::setw(10) << zScoreValue;
            }
            else {
                std::cout << std::setw(10) << "n/a";
            }

            std::cout << " " << status << "\n";
        }
    }
}

int main()
{
    try {
        const std::string configPath = "config.json";
        std::cout << "[start] loading config from " << configPath << "\n";
        const auto session = KiteSession::fromConfigFile(configPath);
        const auto strategyConfig = loadVolArbitrageConfig(configPath);
        logConfig(strategyConfig);

        KiteInstrumentService instrumentService(session);
        KiteMarketDataService marketDataService(session);
        OptionChainService optionChainService;
        QuoteService quoteService(session);
        ForwardPriceService forwardPriceService;
        ImpliedVolService impliedVolService;
        VolSurfaceService volSurfaceService;
        SignalService signalService;
        BacktestService backtestService;

        std::cout << "[instrument] loading Kite instrument master with daily CSV cache\n";
        const auto allNiftyOptions = instrumentService.getNiftyOptionInstrumentsWithDailyCache(
            "data/instruments");
        std::cout << "[instrument] parsed NIFTY option contracts=" << allNiftyOptions.size() << "\n";
        logAvailableExpiries(allNiftyOptions);
        logOptionSamples(allNiftyOptions, "instrument");

        const auto expiryOptions = optionChainService.getNiftyOptionsForExpiry(
            allNiftyOptions,
            strategyConfig.expiry);
        std::cout << "[chain] contracts matching configured expiry "
            << strategyConfig.expiry << "=" << expiryOptions.size() << "\n";
        logOptionSamples(expiryOptions, "chain");

        if (expiryOptions.empty()) {
            std::cout << "[stop] no contracts matched the configured expiry. "
                << "Use one of the printed available expiry values in config.json.\n";
            return 0;
        }

        if (strategyConfig.backtestEnabled) {
            std::cout << "[backtest] downloading spot candles token="
                << strategyConfig.backtestSpotToken
                << " from=" << strategyConfig.backtestFrom
                << " to=" << strategyConfig.backtestTo
                << " interval=" << strategyConfig.backtestInterval
                << "\n";

            const auto spotCandles = marketDataService.getHistoricalCandles(
                strategyConfig.backtestSpotToken,
                strategyConfig.backtestFrom,
                strategyConfig.backtestTo,
                strategyConfig.backtestInterval);

            std::cout << "[backtest] spot candles=" << spotCandles.size() << "\n";
            if (spotCandles.empty()) {
                std::cout << "[stop] no spot candles returned for backtest.\n";
                return 0;
            }

            std::cout << "[backtest] downloading India VIX candles token="
                << strategyConfig.backtestVixToken
                << " from=" << strategyConfig.backtestFrom
                << " to=" << strategyConfig.backtestTo
                << " interval=" << strategyConfig.backtestInterval
                << "\n";

            const auto vixCandles = marketDataService.getHistoricalCandles(
                strategyConfig.backtestVixToken,
                strategyConfig.backtestFrom,
                strategyConfig.backtestTo,
                strategyConfig.backtestInterval);

            std::cout << "[backtest] India VIX candles=" << vixCandles.size() << "\n";
            if (vixCandles.empty()) {
                std::cout << "[stop] no India VIX candles returned for backtest. "
                    << "Check backtest_vix_token in config.json.\n";
                return 0;
            }

            const double selectionSpot = spotCandles.front().close;
            const auto selectedBacktestOptions = optionChainService.getStrikesAroundSpot(
                expiryOptions,
                selectionSpot,
                strategyConfig.strikesEachSide);

            std::cout << "[backtest] selected contracts around first spot close="
                << selectionSpot << " count=" << selectedBacktestOptions.size() << "\n";
            logOptionSamples(selectedBacktestOptions, "backtest-selected");

            if (selectedBacktestOptions.empty()) {
                std::cout << "[stop] no option contracts selected for backtest.\n";
                return 0;
            }

            std::map<long, std::vector<Candle>> optionCandlesByToken;
            for (const auto& option : selectedBacktestOptions) {
                std::cout << "[backtest] downloading " << option.tradingSymbol
                    << " token=" << option.instrumentToken << "\n";

                try {
                    auto candles = marketDataService.getHistoricalCandles(
                        option.instrumentToken,
                        strategyConfig.backtestFrom,
                        strategyConfig.backtestTo,
                        strategyConfig.backtestInterval);

                    std::cout << "[backtest] " << option.tradingSymbol
                        << " candles=" << candles.size() << "\n";

                    if (!candles.empty()) {
                        optionCandlesByToken[option.instrumentToken] = candles;
                    }
                }
                catch (const std::exception& ex) {
                    std::cout << "[backtest] failed " << option.tradingSymbol
                        << ": " << ex.what() << "\n";
                }
            }

            const auto trades = backtestService.runSingleDay(
                selectedBacktestOptions,
                optionCandlesByToken,
                spotCandles,
                vixCandles,
                strategyConfig.daysToExpiry,
                strategyConfig.riskFreeRate,
                strategyConfig.zScoreThreshold,
                strategyConfig.backtestHoldingPeriodMinutes);

            const auto summary = backtestService.summarize(trades);
            std::cout << "[backtest-summary] trades=" << summary.tradeCount
                << " winners=" << summary.winners
                << " losers=" << summary.losers
                << " total_pnl=" << summary.totalPnl
                << " average_pnl=" << summary.averagePnl
                << " holding_minutes=" << strategyConfig.backtestHoldingPeriodMinutes
                << "\n";

            return 0;
        }

        double valuationSpotPrice = strategyConfig.spotPrice;
        if (strategyConfig.useLiveSpot) {
            try {
                const auto spotQuote = quoteService.getIndexQuote(
                    strategyConfig.spotExchange,
                    strategyConfig.spotSymbol);
                if (spotQuote.lastPrice > 0.0) {
                    valuationSpotPrice = spotQuote.lastPrice;
                    std::cout << "[spot] live " << spotQuote.exchange << ":"
                        << spotQuote.tradingSymbol << "=" << valuationSpotPrice
                        << " configured_spot=" << strategyConfig.spotPrice
                        << " diff=" << valuationSpotPrice - strategyConfig.spotPrice
                        << "\n";
                }
                else {
                    std::cout << "[spot] live quote was zero. Using configured spot="
                        << valuationSpotPrice << "\n";
                }
            }
            catch (const std::exception& ex) {
                std::cout << "[spot] live quote failed: " << ex.what()
                    << ". Using configured spot=" << valuationSpotPrice << "\n";
            }
        }
        else {
            std::cout << "[spot] using configured spot=" << valuationSpotPrice << "\n";
        }

        const auto selectedOptions = optionChainService.getStrikesAroundSpot(
            expiryOptions,
            valuationSpotPrice,
            strategyConfig.strikesEachSide);

        std::cout << "[chain] selected contracts around spot=" << selectedOptions.size() << "\n";
        logOptionSamples(selectedOptions, "selected");

        if (selectedOptions.empty()) {
            std::cout << "[stop] no strikes selected around spot. "
                << "Check spot_price and strikes_each_side in config.json.\n";
            return 0;
        }

        std::cout << "[quote] fetching quotes for selected contracts\n";
        const auto quotes = quoteService.getQuotes(selectedOptions);
        std::cout << "[quote] received quotes=" << quotes.size() << "\n";

        std::unordered_map<long, OptionQuote> quotesByToken;
        for (const auto& quote : quotes) {
            quotesByToken[quote.instrumentToken] = quote;
        }

        std::vector<ImpliedVolPoint> impliedVols;
        const double yearsToExpiry = static_cast<double>(strategyConfig.daysToExpiry) / 365.0;
        std::cout << "[iv] years_to_expiry=" << yearsToExpiry << "\n";

        double indiaVix = strategyConfig.indiaVix;
        if (strategyConfig.useLiveIndiaVix) {
            try {
                const auto vixQuote = quoteService.getIndexQuote(
                    strategyConfig.indiaVixExchange,
                    strategyConfig.indiaVixSymbol);
                if (vixQuote.lastPrice > 0.0) {
                    indiaVix = vixQuote.lastPrice;
                    std::cout << "[vix] live " << vixQuote.exchange << ":"
                        << vixQuote.tradingSymbol << "=" << indiaVix << "\n";
                }
                else {
                    std::cout << "[vix] live quote was zero. Using configured india_vix="
                        << indiaVix << "\n";
                }
            }
            catch (const std::exception& ex) {
                std::cout << "[vix] live quote failed: " << ex.what()
                    << ". Using configured india_vix=" << indiaVix << "\n";
            }
        }
        else {
            std::cout << "[vix] using configured india_vix=" << indiaVix << "\n";
        }

        const auto forwardPrice = forwardPriceService.inferFromPutCallParity(
            selectedOptions,
            quotes,
            valuationSpotPrice,
            yearsToExpiry,
            strategyConfig.riskFreeRate);

        if (forwardPrice.has_value()) {
            std::cout << "[forward] parity strike=" << forwardPrice->strike
                << ", call=" << forwardPrice->callPrice
                << ", put=" << forwardPrice->putPrice
                << ", forward=" << forwardPrice->forwardPrice
                << ", implied_spot=" << forwardPrice->impliedSpotPrice
                << ", valuation_spot=" << valuationSpotPrice
                << ", diff=" << forwardPrice->impliedSpotPrice - valuationSpotPrice
                << "\n";
        }
        else {
            std::cout << "[forward] could not infer parity spot diagnostic\n";
        }

        for (const auto& option : selectedOptions) {
            const auto quote = quotesByToken.find(option.instrumentToken);
            if (quote == quotesByToken.end()) {
                std::cout << "[iv] missing quote for " << option.tradingSymbol << "\n";
                continue;
            }

            const double price = quotePrice(quote->second);
            if (price <= 0.0) {
                std::cout << "[iv] skipping " << option.tradingSymbol
                    << " because price is " << price << "\n";
                continue;
            }

            try {
                const double iv = impliedVolService.calculate(
                    option.optionType,
                    price,
                    valuationSpotPrice,
                    option.strike,
                    yearsToExpiry,
                    strategyConfig.riskFreeRate);

                impliedVols.push_back(ImpliedVolPoint{
                    option,
                    valuationSpotPrice,
                    price,
                    yearsToExpiry,
                    iv });
            }
            catch (const std::exception& ex) {
                std::cout << "[iv] skipping " << option.tradingSymbol
                    << " because IV could not be solved: " << ex.what() << "\n";
            }
        }

        std::cout << "[iv] calculated IV points=" << impliedVols.size() << "\n";
        logContractCalculations(
            impliedVols,
            quotesByToken,
            indiaVix,
            strategyConfig.zScoreThreshold);

        volSurfaceService.rebuild(impliedVols);
        std::cout << "[surface] points=" << volSurfaceService.points().size() << "\n";

        const auto signals = signalService.findVixDislocations(
            volSurfaceService.points(),
            indiaVix,
            strategyConfig.zScoreThreshold);

        std::cout << "[signal] generated candidates=" << signals.size() << "\n";

        for (const auto& signal : signals) {
            std::cout
                << signal.name << " "
                << signal.expiry << " "
                << optionTypeText(signal.optionType) << " "
                << "K=" << signal.strike << " "
                << "IV=" << signal.impliedVolatility * 100.0 << "% "
                << "VIX=" << signal.referenceVolatility * 100.0 << "% "
                << "z=" << signal.zScore << " "
                << signal.direction
                << "\n";
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
