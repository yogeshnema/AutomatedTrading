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
                std::stod(row.string("strike")), std::stod(row.string("lot_size")), row.string("option_type"),
                row.string("subscribed") == "t", optional(row, "last_refresh_at"), optional(row, "last_error")};
        }
        ReferenceSeriesRecord referenceSeriesFrom(const database::QueryRow& row)
        {
            return {row.string("series_code"), row.string("series_type"), row.string("display_name"),
                    row.string("provider"), optional(row, "currency"), optional(row, "tenor"),
                    row.string("acquisition_mode"), optional(row, "description"),
                    row.string("subscribed") == "t", optional(row, "status"),
                    optional(row, "last_observed_at"), optional(row, "last_error")};
        }
        SubscriptionRecord subscriptionFrom(const database::QueryRow& row)
        {
            return {
                std::stoll(row.string("instrument_token")), row.string("exchange"),
                row.string("trading_symbol"), row.string("expiry"), std::stod(row.string("strike")),
                row.string("option_type"), row.string("interval"), row.string("enabled") == "t",
                optional(row, "last_refresh_at"), optional(row, "last_candle_at"), optional(row, "last_error")};
        }
        CandleRecord candleFrom(const database::QueryRow& row)
        {
            return {row.string("candle_time"), std::stod(row.string("open")),
                    std::stod(row.string("high")), std::stod(row.string("low")),
                    std::stod(row.string("close")), std::stoll(row.string("volume"))};
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
                        (provider, instrument_token, exchange, trading_symbol, name, expiry, strike, lot_size, option_type, refreshed_at)
                    VALUES ('kite', $1::bigint, $2, $3, $4, $5::date, $6::numeric, $7::numeric, $8, CURRENT_TIMESTAMP)
                    ON CONFLICT (provider, instrument_token) DO UPDATE SET
                        exchange = EXCLUDED.exchange, trading_symbol = EXCLUDED.trading_symbol,
                        name = EXCLUDED.name, expiry = EXCLUDED.expiry, strike = EXCLUDED.strike,
                        lot_size = EXCLUDED.lot_size,
                        option_type = EXCLUDED.option_type, refreshed_at = CURRENT_TIMESTAMP
                )sql", {std::to_string(item.instrumentToken), item.exchange, item.tradingSymbol, item.name,
                         item.expiry, std::to_string(item.strike), std::to_string(item.lotSize),
                         optionTypeText(item.optionType)});
            }
            query.execute("UPDATE market_data.service_state SET instrument_master_refreshed_at = CURRENT_TIMESTAMP WHERE singleton = TRUE");
        });
    }

    std::vector<InstrumentRecord> MarketDataRepository::searchInstruments(
        const std::string& search, const std::string& expiry, const std::string& optionType,
        const std::string& strike, int limit)
    {
        std::scoped_lock lock(mutex_);
        const auto result = queries_.execute(R"sql(
            SELECT m.instrument_token, m.exchange, m.trading_symbol, m.name, m.expiry::text,
                   m.strike::text, m.lot_size::text, m.option_type, (s.instrument_token IS NOT NULL)::text AS subscribed,
                   s.last_refresh_at::text, s.last_error
              FROM market_data.instrument_master m
              LEFT JOIN market_data.subscription s ON s.provider=m.provider AND s.instrument_token=m.instrument_token AND s.enabled
             WHERE m.provider='kite'
               AND m.refreshed_at >= CURRENT_DATE
               AND ($1='' OR m.trading_symbol ILIKE '%' || $1 || '%')
               AND ($2='' OR m.expiry=$2::date)
               AND ($3='' OR m.option_type=$3)
               AND ($4='' OR m.strike=$4::numeric)
             ORDER BY m.expiry, m.strike, m.option_type
             LIMIT $5::integer
        )sql", {search, expiry, optionType, strike, std::to_string(std::clamp(limit, 1, 500))});
        std::vector<InstrumentRecord> output;
        output.reserve(result.rows.size());
        for (const auto& row : result.rows) output.push_back(instrumentFrom(row));
        return output;
    }

    InstrumentFacets MarketDataRepository::instrumentFacets(
        const std::string& name, const std::string& expiry, const std::string& optionType)
    {
        std::scoped_lock lock(mutex_);
        InstrumentFacets output;
        const auto names = queries_.execute(R"sql(
            SELECT DISTINCT name FROM market_data.instrument_master
             WHERE provider='kite' AND refreshed_at >= CURRENT_DATE ORDER BY name
        )sql");
        for (const auto& row : names.rows) output.names.push_back(row.string("name"));

        const auto expiries = queries_.execute(R"sql(
            SELECT DISTINCT expiry::text AS expiry FROM market_data.instrument_master
             WHERE provider='kite' AND refreshed_at >= CURRENT_DATE
               AND ($1='' OR name=$1) ORDER BY expiry
        )sql", {name});
        for (const auto& row : expiries.rows) output.expiries.push_back(row.string("expiry"));

        const auto types = queries_.execute(R"sql(
            SELECT DISTINCT option_type FROM market_data.instrument_master
             WHERE provider='kite' AND refreshed_at >= CURRENT_DATE
               AND ($1='' OR name=$1) AND ($2='' OR expiry=$2::date) ORDER BY option_type
        )sql", {name, expiry});
        for (const auto& row : types.rows) output.optionTypes.push_back(row.string("option_type"));

        const auto strikes = queries_.execute(R"sql(
            SELECT DISTINCT strike::text AS strike FROM market_data.instrument_master
             WHERE provider='kite' AND refreshed_at >= CURRENT_DATE
               AND ($1='' OR name=$1) AND ($2='' OR expiry=$2::date)
               AND ($3='' OR option_type=$3) ORDER BY strike
        )sql", {name, expiry, optionType});
        for (const auto& row : strikes.rows) output.strikes.push_back(std::stod(row.string("strike")));
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
        if (interval != "minute" && interval != "day")
            throw std::invalid_argument("Interval must be 'minute' or 'day'.");
        std::scoped_lock lock(mutex_);
        const auto exists = queries_.queryOne("SELECT 1 AS found FROM market_data.instrument_master WHERE provider='kite' AND instrument_token=$1::bigint", {std::to_string(token)});
        if (!exists) throw std::invalid_argument("Instrument token is not present in the downloaded NIFTY option master.");
        queries_.execute(R"sql(
            INSERT INTO market_data.subscription(provider, instrument_token, interval, enabled, updated_at)
            VALUES ('kite', $1::bigint, $2, TRUE, CURRENT_TIMESTAMP)
            ON CONFLICT(provider, instrument_token) DO UPDATE SET
                interval=CASE WHEN market_data.subscription.interval='minute' OR EXCLUDED.interval='minute'
                              THEN 'minute' ELSE 'day' END, enabled=TRUE,
                last_error=NULL, updated_at=CURRENT_TIMESTAMP
        )sql", {std::to_string(token), interval});
        const auto row = queries_.queryOne(std::string(SubscriptionSelect) + " AND s.instrument_token=$1::bigint", {std::to_string(token)});
        return subscriptionFrom(*row);
    }

    std::vector<ReferenceSeriesRecord> MarketDataRepository::referenceSeries(const std::string& seriesType)
    {
        std::scoped_lock lock(mutex_);
        const auto rows = queries_.execute(R"sql(
            SELECT r.series_code, r.series_type, r.display_name, r.provider, r.currency,
                   r.tenor, r.acquisition_mode, r.description,
                   (s.series_code IS NOT NULL AND s.enabled)::text AS subscribed,
                   s.status, s.last_observed_at::text, s.last_error
              FROM market_data.reference_series r
              LEFT JOIN market_data.reference_subscription s ON s.series_code=r.series_code
             WHERE r.enabled AND ($1='' OR r.series_type=$1)
             ORDER BY r.series_type, r.display_name
        )sql", {seriesType});
        std::vector<ReferenceSeriesRecord> output;
        output.reserve(rows.rows.size());
        for (const auto& row : rows.rows) output.push_back(referenceSeriesFrom(row));
        return output;
    }

    ReferenceSeriesRecord MarketDataRepository::subscribeReferenceSeries(const std::string& seriesCode)
    {
        std::scoped_lock lock(mutex_);
        const auto exists = queries_.queryOne(
            "SELECT 1 AS found FROM market_data.reference_series WHERE series_code=$1 AND enabled", {seriesCode});
        if (!exists) throw std::invalid_argument("Reference series was not found or is disabled.");
        queries_.execute(R"sql(
            INSERT INTO market_data.reference_subscription(series_code, enabled, status, updated_at)
            VALUES ($1, TRUE, 'requested', CURRENT_TIMESTAMP)
            ON CONFLICT(series_code) DO UPDATE SET enabled=TRUE, status='requested',
                last_error=NULL, updated_at=CURRENT_TIMESTAMP
        )sql", {seriesCode});
        const auto row = queries_.queryOne(R"sql(
            SELECT r.series_code, r.series_type, r.display_name, r.provider, r.currency,
                   r.tenor, r.acquisition_mode, r.description, s.enabled::text AS subscribed,
                   s.status, s.last_observed_at::text, s.last_error
              FROM market_data.reference_series r JOIN market_data.reference_subscription s USING(series_code)
             WHERE r.series_code=$1
        )sql", {seriesCode});
        return referenceSeriesFrom(*row);
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
                    last_candle_at=CASE WHEN $2='minute' THEN
                        (SELECT MAX(candle_time) FROM market_data.candle WHERE provider='kite' AND instrument_token=$1::bigint AND interval='minute')
                        ELSE last_candle_at END,
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

    InstrumentMarketData MarketDataRepository::instrumentMarketData(std::int64_t token, const std::string& date)
    {
        std::scoped_lock lock(mutex_);
        const auto instrumentRow = queries_.queryOne(R"sql(
            SELECT m.instrument_token, m.exchange, m.trading_symbol, m.name, m.expiry::text,
                   m.strike::text, m.option_type, (s.instrument_token IS NOT NULL AND s.enabled)::text AS subscribed,
                   s.last_refresh_at::text, s.last_error
              FROM market_data.instrument_master m
              LEFT JOIN market_data.subscription s ON s.provider=m.provider AND s.instrument_token=m.instrument_token
             WHERE m.provider='kite' AND m.instrument_token=$1::bigint
        )sql", {std::to_string(token)});
        if (!instrumentRow) throw std::invalid_argument("Instrument token was not found in the Kite master.");

        InstrumentMarketData output;
        output.instrument = instrumentFrom(*instrumentRow);
        output.summary.date = date;
        const auto rows = queries_.execute(R"sql(
            SELECT candle_time::text, open::text, high::text, low::text, close::text, volume::text
              FROM market_data.candle
             WHERE provider='kite' AND instrument_token=$1::bigint AND interval='minute'
               AND (candle_time AT TIME ZONE 'Asia/Kolkata')::date=$2::date
             ORDER BY candle_time
        )sql", {std::to_string(token), date});
        output.minuteCandles.reserve(rows.rows.size());
        for (const auto& row : rows.rows) output.minuteCandles.push_back(candleFrom(row));

        const auto daily = queries_.queryOne(R"sql(
            SELECT candle_time::text, open::text, high::text, low::text, close::text, volume::text
              FROM market_data.candle
             WHERE provider='kite' AND instrument_token=$1::bigint AND interval='day'
               AND (candle_time AT TIME ZONE 'Asia/Kolkata')::date=$2::date
             ORDER BY candle_time DESC LIMIT 1
        )sql", {std::to_string(token), date});
        if (daily) output.eodCandle = candleFrom(*daily);

        output.summary.minuteCandleCount = output.minuteCandles.size();
        output.summary.eodPersisted = output.eodCandle.has_value();
        const CandleRecord* source = output.eodCandle ? &*output.eodCandle
            : (output.minuteCandles.empty() ? nullptr : &output.minuteCandles.front());
        if (source != nullptr) {
            output.summary.open = source->open;
            output.summary.high = source->high;
            output.summary.low = source->low;
            output.summary.close = source->close;
            output.summary.volume = source->volume;
            if (!output.eodCandle) {
                output.summary.high = output.minuteCandles.front().high;
                output.summary.low = output.minuteCandles.front().low;
                output.summary.close = output.minuteCandles.back().close;
                output.summary.volume = 0;
                for (const auto& candle : output.minuteCandles) {
                    output.summary.high = std::max(*output.summary.high, candle.high);
                    output.summary.low = std::min(*output.summary.low, candle.low);
                    output.summary.volume += candle.volume;
                }
            }
            output.summary.absoluteChange = *output.summary.close - *output.summary.open;
            if (*output.summary.open != 0.0)
                output.summary.percentChange = *output.summary.absoluteChange / *output.summary.open * 100.0;
        }
        if (!output.minuteCandles.empty()) {
            output.summary.firstCandleAt = output.minuteCandles.front().timestamp;
            output.summary.lastCandleAt = output.minuteCandles.back().timestamp;
        }
        return output;
    }

    bool MarketDataRepository::hasDailyCandle(std::int64_t token, const std::string& date)
    {
        std::scoped_lock lock(mutex_);
        return queries_.queryOne(R"sql(
            SELECT 1 AS found FROM market_data.candle
             WHERE provider='kite' AND instrument_token=$1::bigint AND interval='day'
               AND (candle_time AT TIME ZONE 'Asia/Kolkata')::date=$2::date LIMIT 1
        )sql", {std::to_string(token), date}).has_value();
    }

    bool MarketDataRepository::hasMinuteCandle(std::int64_t token, const std::string& date)
    {
        std::scoped_lock lock(mutex_);
        return queries_.queryOne(R"sql(
            SELECT 1 AS found FROM market_data.candle
             WHERE provider='kite' AND instrument_token=$1::bigint AND interval='minute'
               AND (candle_time AT TIME ZONE 'Asia/Kolkata')::date=$2::date LIMIT 1
        )sql", {std::to_string(token), date}).has_value();
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
