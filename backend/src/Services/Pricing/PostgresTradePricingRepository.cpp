#include "Services/Pricing/PostgresTradePricingRepository.h"

#include <chrono>
#include <stdexcept>
#include <string>

namespace automated_trading::services::pricing
{
    namespace
    {
        constexpr std::string_view LoadTradeSql = R"sql(
WITH selected_snapshot AS
(
    SELECT market_snapshot_id, valuation_time, metadata
    FROM pricing.market_snapshot
    WHERE valuation_time <= $2::timestamptz
      AND snapshot_status = 'complete'
    ORDER BY valuation_time DESC
    LIMIT 1
)
SELECT
    t.trade_id::text AS trade_id,
    pd.product_definition_id::text AS product_id,
    pd.product_type,
    eot.underlying_asset_id::text AS underlying_asset_id,
    t.price_currency AS currency,
    eot.option_right,
    eot.exercise_style,
    eot.strike::text AS strike,
    EXTRACT(EPOCH FROM eot.expiry_time)::bigint::text AS expiry_epoch,
    t.multiplier_snapshot::text AS contract_multiplier,
    (CASE WHEN t.side = 'buy' THEN t.quantity ELSE -t.quantity END)::text AS signed_quantity,
    EXTRACT(EPOCH FROM ss.valuation_time)::bigint::text AS valuation_epoch,
    aq.mid::text AS spot,
    selected_curve.zero_rate::text AS risk_free_rate,
    COALESCE((ss.metadata ->> 'dividend_yield')::double precision, 0.0)::text AS dividend_yield,
    vsp.implied_volatility::text AS volatility
FROM trading.trade t
JOIN product.product_definition pd
  ON pd.product_definition_id = t.product_definition_id
JOIN product.equity_option_terms eot
  ON eot.product_definition_id = pd.product_definition_id
CROSS JOIN selected_snapshot ss
JOIN pricing.market_snapshot_asset_quote msaq
  ON msaq.market_snapshot_id = ss.market_snapshot_id
JOIN pricing.asset_quote_snapshot aq
  ON aq.asset_quote_snapshot_id = msaq.asset_quote_snapshot_id
 AND aq.asset_id = eot.underlying_asset_id
JOIN pricing.market_snapshot_curve msc
  ON msc.market_snapshot_id = ss.market_snapshot_id
 AND msc.curve_role = 'discount'
JOIN LATERAL
(
    SELECT cn.zero_rate
    FROM pricing.curve_node cn
    WHERE cn.curve_snapshot_id = msc.curve_snapshot_id
    ORDER BY ABS(
        cn.tenor_days - GREATEST(
            CEIL(EXTRACT(EPOCH FROM (eot.expiry_time - ss.valuation_time)) / 86400.0),
            0
        )
    )
    LIMIT 1
) selected_curve ON TRUE
JOIN pricing.market_snapshot_vol_surface msv
  ON msv.market_snapshot_id = ss.market_snapshot_id
 AND msv.surface_role = 'equity_vol'
JOIN pricing.vol_surface_point vsp
  ON vsp.vol_surface_snapshot_id = msv.vol_surface_snapshot_id
 AND vsp.expiry_time = eot.expiry_time
 AND vsp.strike = eot.strike
 AND vsp.option_right = eot.option_right
WHERE t.trade_id = $1::uuid
  AND t.status = 'confirmed'
  AND pd.product_type = 'european_vanilla_option'
  AND eot.exercise_style = 'european'
)sql";

        constexpr std::string_view LoadDraftTradeSql = R"sql(
WITH selected_trade AS
(
    SELECT * FROM trade_library.trades
     WHERE id=$1::uuid AND status IN ('DRAFT', 'ACTIVE') AND instrument_token IS NOT NULL
),
selected_spot AS
(
    SELECT o.value
      FROM market_data.reference_observation o, selected_trade t
     WHERE o.series_code=t.spot_series_code AND o.scope_key='GLOBAL'
       AND o.observed_at <= $2::timestamptz
     ORDER BY o.observed_at DESC LIMIT 1
),
selected_rate AS
(
    SELECT o.value
      FROM market_data.reference_observation o, selected_trade t
     WHERE o.series_code=t.rate_series_code AND o.scope_key='GLOBAL'
       AND o.observed_at <= $2::timestamptz
     ORDER BY o.observed_at DESC LIMIT 1
),
selected_vol AS
(
    SELECT o.value
      FROM market_data.reference_observation o, selected_trade t
     WHERE o.series_code=t.implied_vol_series_code
       AND o.scope_key=t.instrument_token::text
       AND o.observed_at <= $2::timestamptz
     ORDER BY o.observed_at DESC LIMIT 1
)
SELECT
    t.id::text AS trade_id,
    t.id::text AS product_id,
    t.product_type,
    t.underlying AS underlying_asset_id,
    t.currency,
    CASE WHEN t.option_type='CE' THEN 'call' ELSE 'put' END AS option_right,
    'european' AS exercise_style,
    t.strike::text AS strike,
    EXTRACT(EPOCH FROM ((t.expiry + TIME '15:30') AT TIME ZONE 'Asia/Kolkata'))::bigint::text AS expiry_epoch,
    t.lot_size::text AS contract_multiplier,
    (CASE WHEN t.side='buy' THEN t.quantity ELSE -t.quantity END)::text AS signed_quantity,
    EXTRACT(EPOCH FROM $2::timestamptz)::bigint::text AS valuation_epoch,
    s.value::text AS spot,
    r.value::text AS risk_free_rate,
    '0' AS dividend_yield,
    v.value::text AS volatility
FROM selected_trade t
CROSS JOIN selected_spot s
CROSS JOIN selected_rate r
CROSS JOIN selected_vol v
)sql";

        double number(const database::QueryRow& row, std::string_view column)
        {
            try {
                return std::stod(row.string(column));
            }
            catch (const std::exception&) {
                throw database::DatabaseError(
                    "Invalid numeric value in pricing column '" + std::string(column) + "'.");
            }
        }

        std::chrono::sys_seconds timestamp(const database::QueryRow& row, std::string_view column)
        {
            try {
                return std::chrono::sys_seconds{
                    std::chrono::seconds{std::stoll(row.string(column))}};
            }
            catch (const std::exception&) {
                throw database::DatabaseError(
                    "Invalid timestamp value in pricing column '" + std::string(column) + "'.");
            }
        }
    }

    PostgresTradePricingRepository::PostgresTradePricingRepository(
        database::QueryService& queryService)
        : queryService_(queryService)
    {
    }

    TradePricingInput PostgresTradePricingRepository::load(
        std::string_view tradeId,
        std::string_view valuationDate)
    {
        std::scoped_lock lock(connectionMutex_);
        auto row = queryService_.queryOne(
            LoadTradeSql,
            {std::string(tradeId), std::string(valuationDate)});
        if (!row) {
            row = queryService_.queryOne(
                LoadDraftTradeSql,
                {std::string(tradeId), std::string(valuationDate)});
        }
        if (!row) {
            throw database::DatabaseError(
                "No priceable European option was found. A draft trade requires current underlying, "
                "contract-IV and rate observations; a confirmed trade requires a complete market snapshot.");
        }

        TradePricingInput input;
        input.tradeId = row->string("trade_id");
        input.productId = row->string("product_id");
        input.underlyingAssetId = row->string("underlying_asset_id");
        input.currency = row->string("currency");
        input.optionRight = row->string("option_right") == "call"
            ? products::equity::options::OptionRight::Call
            : products::equity::options::OptionRight::Put;
        input.strike = number(*row, "strike");
        input.expiry = timestamp(*row, "expiry_epoch");
        input.contractMultiplier = number(*row, "contract_multiplier");
        input.signedQuantity = number(*row, "signed_quantity");
        input.valuationTime = timestamp(*row, "valuation_epoch");
        input.marketData = analytics::pricing::EquityMarketData{
            number(*row, "spot"),
            number(*row, "risk_free_rate"),
            number(*row, "dividend_yield"),
            number(*row, "volatility")};
        return input;
    }

    bool PostgresTradePricingRepository::isReady()
    {
        try {
            std::scoped_lock lock(connectionMutex_);
            const auto result = queryService_.queryOne("SELECT 1 AS ready");
            return result && result->string("ready") == "1";
        }
        catch (...) {
            return false;
        }
    }
}
