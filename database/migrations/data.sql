\set ON_ERROR_STOP on

BEGIN;

-- IDs intentionally remain fixed so related sample rows are reproducible.

INSERT INTO config.configuration
(
    configuration_id,
    configuration_type,
    name,
    version,
    parameters,
    is_active
)
VALUES
(
    '00000000-0000-0000-0000-000000000001',
    'pricing',
    'black-scholes-default',
    1,
    '{
       "model": "black_scholes",
       "day_count_basis": 365,
       "spot_source": "sample",
       "discount_curve_role": "discount",
       "vol_surface_role": "equity_vol",
       "calculate_greeks": true
     }'::jsonb,
    TRUE
);

INSERT INTO reference.asset
(
    asset_id,
    asset_type,
    symbol,
    name,
    currency,
    exchange_code,
    market_data_source,
    vendor_identifier,
    timezone
)
VALUES
(
    '10000000-0000-0000-0000-000000000001',
    'equity',
    'AAPL',
    'Apple Inc.',
    'USD',
    'NASDAQ',
    'sample',
    'AAPL',
    'America/New_York'
);

INSERT INTO reference.instrument
(
    instrument_id,
    asset_id,
    instrument_type,
    symbol,
    exchange_code,
    currency,
    contract_multiplier,
    lot_size,
    tick_size,
    settlement_type,
    settlement_days
)
VALUES
(
    '20000000-0000-0000-0000-000000000001',
    '10000000-0000-0000-0000-000000000001',
    'equity',
    'AAPL',
    'NASDAQ',
    'USD',
    1,
    1,
    0.01,
    'physical',
    2
);

INSERT INTO reference.instrument
(
    instrument_id,
    asset_id,
    instrument_type,
    symbol,
    exchange_code,
    currency,
    contract_multiplier,
    lot_size,
    tick_size,
    issue_date,
    maturity_date,
    settlement_type,
    settlement_days
)
VALUES
(
    '20000000-0000-0000-0000-000000000002',
    '10000000-0000-0000-0000-000000000001',
    'equity_option',
    'AAPL-C-100-SAMPLE',
    'SAMPLE',
    'USD',
    100,
    1,
    0.01,
    '2026-01-01',
    '2027-01-01',
    'cash',
    1
);

INSERT INTO product.product_definition
(
    product_definition_id,
    instrument_id,
    product_family,
    product_type,
    schema_version,
    effective_date,
    maturity_date,
    settlement_currency,
    settlement_type,
    contract_multiplier,
    metadata
)
VALUES
(
    '30000000-0000-0000-0000-000000000001',
    '20000000-0000-0000-0000-000000000002',
    'equity_option',
    'european_vanilla_option',
    1,
    '2026-01-01',
    '2027-01-01',
    'USD',
    'cash',
    100,
    '{"sample": true}'::jsonb
);

INSERT INTO product.equity_option_terms
(
    equity_option_terms_id,
    product_definition_id,
    underlying_asset_id,
    option_right,
    exercise_style,
    strike,
    expiry_time,
    exercise_timezone,
    premium_currency,
    payoff_currency,
    settlement_price_rule,
    dividend_treatment
)
VALUES
(
    '30000000-0000-0000-0000-000000000002',
    '30000000-0000-0000-0000-000000000001',
    '10000000-0000-0000-0000-000000000001',
    'call',
    'european',
    100,
    '2027-01-01 00:00:00+00',
    'America/New_York',
    'USD',
    'USD',
    'official_close',
    'continuous_yield'
);

INSERT INTO pricing.curve_definition
(
    curve_definition_id,
    curve_type,
    curve_name,
    currency,
    day_count_convention,
    compounding_convention
)
VALUES
(
    '40000000-0000-0000-0000-000000000001',
    'discount',
    'USD_SAMPLE_DISCOUNT',
    'USD',
    'ACT/365',
    'continuous'
);

INSERT INTO pricing.curve_snapshot
(
    curve_snapshot_id,
    curve_definition_id,
    valuation_time,
    market_data_cutoff,
    build_status,
    configuration_id
)
VALUES
(
    '40000000-0000-0000-0000-000000000002',
    '40000000-0000-0000-0000-000000000001',
    '2026-01-01 00:00:00+00',
    '2026-01-01 00:00:00+00',
    'completed',
    '00000000-0000-0000-0000-000000000001'
);

INSERT INTO pricing.curve_node
(
    curve_node_id,
    curve_snapshot_id,
    tenor_days,
    maturity_date,
    market_quote,
    zero_rate,
    discount_factor,
    forward_rate
)
VALUES
(
    '40000000-0000-0000-0000-000000000003',
    '40000000-0000-0000-0000-000000000002',
    365,
    '2027-01-01',
    0.05,
    0.05,
    EXP(-0.05),
    0.05
);

INSERT INTO pricing.vol_surface_definition
(
    vol_surface_definition_id,
    surface_name,
    underlying_asset_id,
    quote_convention,
    interpolation_method
)
VALUES
(
    '50000000-0000-0000-0000-000000000001',
    'AAPL_SAMPLE_VOL',
    '10000000-0000-0000-0000-000000000001',
    'strike',
    'linear'
);

INSERT INTO pricing.vol_surface_snapshot
(
    vol_surface_snapshot_id,
    vol_surface_definition_id,
    valuation_time,
    market_data_cutoff,
    build_status,
    configuration_id
)
VALUES
(
    '50000000-0000-0000-0000-000000000002',
    '50000000-0000-0000-0000-000000000001',
    '2026-01-01 00:00:00+00',
    '2026-01-01 00:00:00+00',
    'completed',
    '00000000-0000-0000-0000-000000000001'
);

INSERT INTO pricing.vol_surface_point
(
    vol_surface_point_id,
    vol_surface_snapshot_id,
    expiry_time,
    strike,
    option_right,
    moneyness,
    delta,
    implied_volatility
)
VALUES
(
    '50000000-0000-0000-0000-000000000003',
    '50000000-0000-0000-0000-000000000002',
    '2027-01-01 00:00:00+00',
    100,
    'call',
    1.0,
    0.6368,
    0.20
);

INSERT INTO pricing.asset_quote_snapshot
(
    asset_quote_snapshot_id,
    asset_id,
    observed_at,
    source,
    bid,
    ask,
    mid
)
VALUES
(
    '60000000-0000-0000-0000-000000000001',
    '10000000-0000-0000-0000-000000000001',
    '2026-01-01 00:00:00+00',
    'sample',
    99.99,
    100.01,
    100.00
);

INSERT INTO pricing.market_snapshot
(
    market_snapshot_id,
    valuation_time,
    market_data_cutoff,
    snapshot_status,
    metadata
)
VALUES
(
    '60000000-0000-0000-0000-000000000002',
    '2026-01-01 00:00:00+00',
    '2026-01-01 00:00:00+00',
    'complete',
    '{"sample": true, "dividend_yield": 0.0}'::jsonb
);

INSERT INTO pricing.market_snapshot_asset_quote
(
    market_snapshot_asset_quote_id,
    market_snapshot_id,
    asset_quote_snapshot_id
)
VALUES
(
    '60000000-0000-0000-0000-000000000003',
    '60000000-0000-0000-0000-000000000002',
    '60000000-0000-0000-0000-000000000001'
);

INSERT INTO pricing.market_snapshot_curve
(
    market_snapshot_curve_id,
    market_snapshot_id,
    curve_snapshot_id,
    curve_role
)
VALUES
(
    '60000000-0000-0000-0000-000000000004',
    '60000000-0000-0000-0000-000000000002',
    '40000000-0000-0000-0000-000000000002',
    'discount'
);

INSERT INTO pricing.market_snapshot_vol_surface
(
    market_snapshot_vol_surface_id,
    market_snapshot_id,
    vol_surface_snapshot_id,
    surface_role
)
VALUES
(
    '60000000-0000-0000-0000-000000000005',
    '60000000-0000-0000-0000-000000000002',
    '50000000-0000-0000-0000-000000000002',
    'equity_vol'
);

INSERT INTO pricing.pricing_model
(
    pricing_model_id,
    model_key,
    model_name,
    implementation_version,
    supported_products,
    default_parameters
)
VALUES
(
    '70000000-0000-0000-0000-000000000001',
    'black_scholes',
    'Black-Scholes Equity Option Pricer',
    '1.0.0',
    '["european_vanilla_option"]'::jsonb,
    '{"day_count_basis": 365}'::jsonb
);

INSERT INTO trading.strategy
(
    strategy_id,
    strategy_type,
    name,
    version,
    configuration_id,
    description
)
VALUES
(
    '80000000-0000-0000-0000-000000000001',
    'manual_pricing',
    'Sample European Option',
    1,
    '00000000-0000-0000-0000-000000000001',
    'Sample strategy used for the initial database pricing path'
);

INSERT INTO trading.trade
(
    trade_id,
    external_trade_id,
    product_definition_id,
    trade_time,
    trade_date,
    settlement_date,
    side,
    quantity,
    execution_price,
    price_currency,
    multiplier_snapshot,
    gross_consideration,
    fees,
    status,
    source,
    metadata
)
VALUES
(
    '90000000-0000-0000-0000-000000000001',
    'SAMPLE-EURO-CALL-001',
    '30000000-0000-0000-0000-000000000001',
    '2026-01-01 00:00:00+00',
    '2026-01-01',
    '2026-01-02',
    'buy',
    10,
    9.50,
    'USD',
    100,
    9500,
    0,
    'confirmed',
    'sample',
    '{"sample": true}'::jsonb
);

INSERT INTO pricing.valuation_run
(
    valuation_run_id,
    valuation_time,
    market_snapshot_id,
    pricing_model_id,
    configuration_id,
    purpose,
    status,
    model_parameters
)
VALUES
(
    'a0000000-0000-0000-0000-000000000001',
    '2026-01-01 00:00:00+00',
    '60000000-0000-0000-0000-000000000002',
    '70000000-0000-0000-0000-000000000001',
    '00000000-0000-0000-0000-000000000001',
    'ad_hoc_pricing',
    'requested',
    '{"day_count_basis": 365}'::jsonb
);

COMMIT;