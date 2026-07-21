#include "Services/MarketData/MarketDataRepository.h"

#include <algorithm>
#include <stdexcept>

namespace automated_trading::services::market_data
{
    namespace
    {
        std::string optionTypeText(OptionType type) { return type == OptionType::Call ? "CE" : "PE"; }
        std::optional<std::string> optional(const database::QueryRow& row, const char* name)
        {
            return row.optionalString(name);
        }
        InstrumentRecord instrumentFrom(const database::QueryRow& row)
        {
            return {
                std::stoll(row.string("instrument_token")), row.string("exchange"),
                row.string("trading_symbol"), row.string("name"), row.string("expiry"),
                std::stod(row.string("strike")), row.string("option_type"),
                row.string("subscribed") == "t", optional(row, "last_refresh_at"), optional(row, "last_error")};
        }
        SubscriptionRecord subscriptionFrom(const database::QueryRow& row)
        {
            return {
                std::stoll(row.string("instrument_token")), row.string("exchange"),
                row.string("trading_symbol"), row.string("expiry"), std::stod(row.string("strike")),
                row.string("option_type"), row.string("interval"), row.string("enabled") == "t",
                optional(row, "last_refresh_at"), optional(row, "last_candle_at"), optional(row, "last_error")};
        }
        constexpr auto SubscriptionSelect = R"sql(
            SELECT s.instrument_token, m.exchange, m.trading_symbol, m.expiry::text,
                   m.strike::text, m.option_type, s.interval, s.enabled,
                   s.last_refresh_at::text, s.last_candle_at::text, s.last_error
              FROM market_data.subscription s
              JOIN market_data.instrument_master m
                ON m.provider = s.provider AND m.instrument_token = s.instrument_token
             WHERE s.provider = 'kite'
        )sql";
    }

    MarketDataRepository::MarketDataRepository(database::QueryService& queries) : queries_(queries) {}

    void MarketDataRepository::replaceNiftyOptionMaster(const std::vector<OptionInstrument>& instruments)
    {
        std::scoped_lock lock(mutex_);
        queries_.transaction([&](database::QueryService& query) {
            for (const auto& item : instruments) {
                query.execute(R"sql(
                    INSERT INTO market_data.instrument_master
                        (provider, instrument_token, exchange, trading_symbol, name, expiry, strike, option_type, refreshed_at)
                    VALUES ('kite', $1::bigint, $2, $3, $4, $5::date, $6::numeric, $7, CURRENT_TIMESTAMP)
                    ON CONFLICT (provider, instrument_token) DO UPDATE SET
                        exchange = EXCLUDED.exchange, trading_symbol = EXCLUDED.trading_symbol,
                        name = EXCLUDED.name, expiry = EXCLUDED.expiry, strike = EXCLUDED.strike,
                        option_type = EXCLUDED.option_type, refreshed_at = CURRENT_TIMESTAMP
                )sql", {std::to_string(item.instrumentToken), item.exchange, item.tradingSymbol, item.name,
                         item.expiry, std::to_string(item.strike), optionTypeText(item.optionType)});
            }
            query.execute("UPDATE market_data.service_state SET instrument_master_refreshed_at = CURRENT_TIMESTAMP WHERE singleton = TRUE");
        });
    }

    std::vector<InstrumentRecord> MarketDataRepository::searchInstruments(
        const std::string& search, const std::string& expiry, const std::string& optionType, int limit)
    {
        std::scoped_lock lock(mutex_);
        const auto result = queries_.execute(R"sql(
            SELECT m.instrument_token, m.exchange, m.trading_symbol, m.name, m.expiry::text,
                   m.strike::text, m.option_type, (s.instrument_token IS NOT NULL)::text AS subscribed,
                   s.last_refresh_at::text, s.last_error
              FROM market_data.instrument_master m
              LEFT JOIN market_data.subscription s ON s.provider=m.provider AND s.instrument_token=m.instrument_token AND s.enabled
             WHERE m.provider='kite'
               AND m.refreshed_at >= CURRENT_DATE
               AND ($1='' OR m.trading_symbol ILIKE '%' || $1 || '%')
               AND ($2='' OR m.expiry=$2::date)
               AND ($3='' OR m.option_type=$3)
             ORDER BY m.expiry, m.strike, m.option_type
             LIMIT $4::integer
        )sql", {search, expiry, optionType, std::to_string(std::clamp(limit, 1, 500))});
        std::vector<InstrumentRecord> output;
        output.reserve(result.rows.size());
        for (const auto& row : result.rows) output.push_back(instrumentFrom(row));
        return output;
    }

    std::vector<SubscriptionRecord> MarketDataRepository::subscriptions()
    {
        std::scoped_lock lock(mutex_);
        const auto result = queries_.execute(std::string(SubscriptionSelect) + " AND s.enabled ORDER BY m.expiry, m.strike, m.option_type");
        std::vector<SubscriptionRecord> output;
        output.reserve(result.rows.size());
        for (const auto& row : result.rows) output.push_back(subscriptionFrom(row));
        return output;
    }

    SubscriptionRecord MarketDataRepository::subscribe(std::int64_t token, const std::string& interval)
    {
        if (interval != "minute") throw std::invalid_argument("Only the minute interval is supported by this worker.");
        std::scoped_lock lock(mutex_);
        const auto exists = queries_.queryOne("SELECT 1 AS found FROM market_data.instrument_master WHERE provider='kite' AND instrument_token=$1::bigint", {std::to_string(token)});
        if (!exists) throw std::invalid_argument("Instrument token is not present in the downloaded NIFTY option master.");
        queries_.execute(R"sql(
            INSERT INTO market_data.subscription(provider, instrument_token, interval, enabled, updated_at)
            VALUES ('kite', $1::bigint, $2, TRUE, CURRENT_TIMESTAMP)
            ON CONFLICT(provider, instrument_token) DO UPDATE SET interval=EXCLUDED.interval, enabled=TRUE,
                last_error=NULL, updated_at=CURRENT_TIMESTAMP
        )sql", {std::to_string(token), interval});
        const auto row = queries_.queryOne(std::string(SubscriptionSelect) + " AND s.instrument_token=$1::bigint", {std::to_string(token)});
        return subscriptionFrom(*row);
    }

    void MarketDataRepository::unsubscribe(std::int64_t token)
    {
        std::scoped_lock lock(mutex_);
        queries_.execute("UPDATE market_data.subscription SET enabled=FALSE, updated_at=CURRENT_TIMESTAMP WHERE provider='kite' AND instrument_token=$1::bigint", {std::to_string(token)});
    }

    void MarketDataRepository::saveCandles(std::int64_t token, const std::string& interval, const std::vector<Candle>& candles)
    {
        std::scoped_lock lock(mutex_);
        queries_.transaction([&](database::QueryService& query) {
            for (const auto& candle : candles) {
                query.execute(R"sql(
                    INSERT INTO market_data.candle(provider, instrument_token, interval, candle_time, open, high, low, close, volume)
                    VALUES ('kite',$1::bigint,$2,$3::timestamptz,$4::numeric,$5::numeric,$6::numeric,$7::numeric,$8::bigint)
                    ON CONFLICT(provider,instrument_token,interval,candle_time) DO UPDATE SET
                        open=EXCLUDED.open, high=EXCLUDED.high, low=EXCLUDED.low, close=EXCLUDED.close,
                        volume=EXCLUDED.volume, received_at=CURRENT_TIMESTAMP
                )sql", {std::to_string(token), interval, candle.timestamp, std::to_string(candle.open),
                         std::to_string(candle.high), std::to_string(candle.low), std::to_string(candle.close),
                         std::to_string(candle.volume)});
            }
            query.execute(R"sql(
                UPDATE market_data.subscription SET last_refresh_at=CURRENT_TIMESTAMP,
                    last_candle_at=(SELECT MAX(candle_time) FROM market_data.candle WHERE provider='kite' AND instrument_token=$1::bigint AND interval=$2),
                    last_error=NULL, updated_at=CURRENT_TIMESTAMP
                 WHERE provider='kite' AND instrument_token=$1::bigint
            )sql", {std::to_string(token), interval});
        });
    }

    void MarketDataRepository::markRefreshFailure(std::int64_t token, const std::string& error)
    {
        std::scoped_lock lock(mutex_);
        queries_.execute("UPDATE market_data.subscription SET last_refresh_at=CURRENT_TIMESTAMP,last_error=$2,updated_at=CURRENT_TIMESTAMP WHERE provider='kite' AND instrument_token=$1::bigint", {std::to_string(token), error.substr(0, 1000)});
    }

    ServiceStatus MarketDataRepository::status(bool workerRunning)
    {
        std::scoped_lock lock(mutex_);
        const auto row = queries_.queryOne(R"sql(
            SELECT (SELECT COUNT(*) FROM market_data.instrument_master WHERE provider='kite' AND refreshed_at >= CURRENT_DATE)::text AS instruments,
                   (SELECT COUNT(*) FROM market_data.subscription WHERE provider='kite' AND enabled)::text AS subscriptions,
                   instrument_master_refreshed_at::text
              FROM market_data.service_state WHERE singleton=TRUE
        )sql");
        return {true, workerRunning, static_cast<std::size_t>(std::stoull(row->string("instruments"))),
                static_cast<std::size_t>(std::stoull(row->string("subscriptions"))), optional(*row, "instrument_master_refreshed_at")};
    }

    bool MarketDataRepository::isReady()
    {
        try { std::scoped_lock lock(mutex_); return queries_.queryOne("SELECT 1 AS ready").has_value(); }
        catch (...) { return false; }
    }
}
