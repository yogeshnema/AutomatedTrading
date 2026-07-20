\set ON_ERROR_STOP on

BEGIN;

CREATE SCHEMA IF NOT EXISTS config;
CREATE SCHEMA IF NOT EXISTS reference;
CREATE SCHEMA IF NOT EXISTS product;
CREATE SCHEMA IF NOT EXISTS trading;
CREATE SCHEMA IF NOT EXISTS pricing;
CREATE SCHEMA IF NOT EXISTS risk;
CREATE SCHEMA IF NOT EXISTS backtest;

-- ============================================================
-- Configuration
-- ============================================================

CREATE TABLE IF NOT EXISTS config.configuration
(
    configuration_id       UUID PRIMARY KEY,
    configuration_type     TEXT NOT NULL,
    name                   TEXT NOT NULL,
    version                INTEGER NOT NULL CHECK (version > 0),
    parameters             JSONB NOT NULL DEFAULT '{}'::jsonb,
    checksum               TEXT,
    is_active              BOOLEAN NOT NULL DEFAULT FALSE,
    created_at             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_by             TEXT NOT NULL DEFAULT CURRENT_USER,

    CONSTRAINT uq_configuration_name_version
        UNIQUE (configuration_type, name, version)
);

CREATE UNIQUE INDEX IF NOT EXISTS uq_active_configuration
    ON config.configuration (configuration_type, name)
    WHERE is_active;

-- ============================================================
-- Reference data
-- ============================================================

CREATE TABLE IF NOT EXISTS reference.asset
(
    asset_id               UUID PRIMARY KEY,
    asset_type             TEXT NOT NULL,
    symbol                 TEXT NOT NULL,
    name                   TEXT NOT NULL,
    currency               CHAR(3) NOT NULL,
    exchange_code          TEXT,
    market_data_source     TEXT,
    vendor_identifier      TEXT,
    timezone               TEXT NOT NULL DEFAULT 'UTC',
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,
    valid_from             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    valid_to               TIMESTAMPTZ,

    CONSTRAINT ck_asset_validity
        CHECK (valid_to IS NULL OR valid_to > valid_from),

    CONSTRAINT uq_asset_symbol
        UNIQUE (asset_type, symbol, exchange_code, valid_from)
);

CREATE TABLE IF NOT EXISTS reference.instrument
(
    instrument_id          UUID PRIMARY KEY,
    asset_id               UUID REFERENCES reference.asset(asset_id),
    instrument_type        TEXT NOT NULL,
    symbol                 TEXT NOT NULL,
    exchange_code          TEXT,
    currency               CHAR(3) NOT NULL,
    contract_multiplier    NUMERIC(30,10) NOT NULL DEFAULT 1
                               CHECK (contract_multiplier > 0),
    lot_size               NUMERIC(30,10) NOT NULL DEFAULT 1
                               CHECK (lot_size > 0),
    tick_size              NUMERIC(30,10) CHECK (tick_size > 0),
    issue_date             DATE,
    maturity_date          DATE,
    settlement_type        TEXT NOT NULL DEFAULT 'cash',
    settlement_days        INTEGER NOT NULL DEFAULT 0
                               CHECK (settlement_days >= 0),
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,
    valid_from             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    valid_to               TIMESTAMPTZ,

    CONSTRAINT ck_instrument_validity
        CHECK (valid_to IS NULL OR valid_to > valid_from),

    CONSTRAINT uq_instrument_symbol
        UNIQUE (symbol, exchange_code, valid_from)
);

CREATE INDEX IF NOT EXISTS ix_instrument_asset
    ON reference.instrument(asset_id);

-- ============================================================
-- Product definitions
-- ============================================================

CREATE TABLE IF NOT EXISTS product.product_definition
(
    product_definition_id  UUID PRIMARY KEY,
    instrument_id          UUID REFERENCES reference.instrument(instrument_id),
    product_family         TEXT NOT NULL,
    product_type           TEXT NOT NULL,
    schema_version         INTEGER NOT NULL DEFAULT 1 CHECK (schema_version > 0),
    effective_date         DATE NOT NULL,
    maturity_date          DATE NOT NULL,
    settlement_currency    CHAR(3) NOT NULL,
    settlement_type        TEXT NOT NULL DEFAULT 'cash',
    contract_multiplier    NUMERIC(30,10) NOT NULL DEFAULT 1
                               CHECK (contract_multiplier > 0),
    terms_hash             TEXT,
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_by             TEXT NOT NULL DEFAULT CURRENT_USER,

    CONSTRAINT ck_product_dates
        CHECK (maturity_date >= effective_date)
);

CREATE INDEX IF NOT EXISTS ix_product_instrument
    ON product.product_definition(instrument_id);

CREATE TABLE IF NOT EXISTS product.equity_option_terms
(
    equity_option_terms_id UUID PRIMARY KEY,
    product_definition_id  UUID NOT NULL UNIQUE
                               REFERENCES product.product_definition(product_definition_id)
                               ON DELETE CASCADE,
    underlying_asset_id    UUID NOT NULL
                               REFERENCES reference.asset(asset_id),
    option_right           TEXT NOT NULL
                               CHECK (option_right IN ('call', 'put')),
    exercise_style         TEXT NOT NULL
                               CHECK (exercise_style IN
                                      ('european', 'american', 'bermudan')),
    strike                 NUMERIC(30,10) NOT NULL CHECK (strike > 0),
    expiry_time            TIMESTAMPTZ NOT NULL,
    exercise_timezone      TEXT NOT NULL DEFAULT 'UTC',
    premium_currency       CHAR(3) NOT NULL,
    payoff_currency        CHAR(3) NOT NULL,
    settlement_price_rule  TEXT NOT NULL DEFAULT 'official_close',
    dividend_treatment     TEXT NOT NULL DEFAULT 'continuous_yield',
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb
);

CREATE INDEX IF NOT EXISTS ix_option_underlying
    ON product.equity_option_terms(underlying_asset_id);

CREATE INDEX IF NOT EXISTS ix_option_expiry
    ON product.equity_option_terms(expiry_time);

CREATE TABLE IF NOT EXISTS product.asian_terms
(
    asian_terms_id         UUID PRIMARY KEY,
    product_definition_id  UUID NOT NULL UNIQUE
                               REFERENCES product.product_definition(product_definition_id)
                               ON DELETE CASCADE,
    average_type           TEXT NOT NULL
                               CHECK (average_type IN ('arithmetic', 'geometric')),
    average_target         TEXT NOT NULL
                               CHECK (average_target IN ('price', 'strike')),
    weighting_method       TEXT NOT NULL DEFAULT 'equal',
    initial_average        NUMERIC(30,10),
    observations_completed INTEGER NOT NULL DEFAULT 0
                               CHECK (observations_completed >= 0),
    past_average           NUMERIC(30,10),
    missing_fixing_rule    TEXT NOT NULL DEFAULT 'previous_valid_fixing',
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb
);

CREATE TABLE IF NOT EXISTS product.barrier_terms
(
    barrier_terms_id       UUID PRIMARY KEY,
    product_definition_id  UUID NOT NULL
                               REFERENCES product.product_definition(product_definition_id)
                               ON DELETE CASCADE,
    barrier_asset_id       UUID NOT NULL
                               REFERENCES reference.asset(asset_id),
    barrier_direction      TEXT NOT NULL
                               CHECK (barrier_direction IN ('up', 'down')),
    barrier_action         TEXT NOT NULL
                               CHECK (barrier_action IN ('knock_in', 'knock_out')),
    barrier_level          NUMERIC(30,10) NOT NULL CHECK (barrier_level > 0),
    monitoring_type        TEXT NOT NULL
                               CHECK (monitoring_type IN
                                      ('continuous', 'daily', 'scheduled')),
    monitoring_start       TIMESTAMPTZ NOT NULL,
    monitoring_end         TIMESTAMPTZ NOT NULL,
    rebate_amount          NUMERIC(30,10) NOT NULL DEFAULT 0,
    rebate_timing          TEXT NOT NULL DEFAULT 'maturity',
    already_triggered      BOOLEAN NOT NULL DEFAULT FALSE,
    trigger_time           TIMESTAMPTZ,
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT ck_barrier_monitoring_period
        CHECK (monitoring_end >= monitoring_start),

    CONSTRAINT ck_barrier_trigger
        CHECK (
            (already_triggered = FALSE AND trigger_time IS NULL)
            OR already_triggered = TRUE
        )
);

CREATE TABLE IF NOT EXISTS product.schedule
(
    schedule_id            UUID PRIMARY KEY,
    product_definition_id  UUID NOT NULL
                               REFERENCES product.product_definition(product_definition_id)
                               ON DELETE CASCADE,
    schedule_type          TEXT NOT NULL,
    timezone               TEXT NOT NULL DEFAULT 'UTC',
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb
);

CREATE TABLE IF NOT EXISTS product.schedule_date
(
    schedule_date_id       UUID PRIMARY KEY,
    schedule_id            UUID NOT NULL
                               REFERENCES product.schedule(schedule_id)
                               ON DELETE CASCADE,
    sequence_number        INTEGER NOT NULL CHECK (sequence_number > 0),
    observation_time       TIMESTAMPTZ NOT NULL,
    weight                 NUMERIC(30,15),
    fixing_value           NUMERIC(30,10),
    fixing_status          TEXT NOT NULL DEFAULT 'unfixed',
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT uq_schedule_sequence
        UNIQUE (schedule_id, sequence_number),

    CONSTRAINT uq_schedule_time
        UNIQUE (schedule_id, observation_time)
);

-- ============================================================
-- Trading and strategy data
-- ============================================================

CREATE TABLE IF NOT EXISTS trading.strategy
(
    strategy_id            UUID PRIMARY KEY,
    strategy_type          TEXT NOT NULL,
    name                   TEXT NOT NULL,
    version                INTEGER NOT NULL DEFAULT 1 CHECK (version > 0),
    configuration_id       UUID REFERENCES config.configuration(configuration_id),
    description            TEXT,
    is_active              BOOLEAN NOT NULL DEFAULT TRUE,
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT uq_strategy_name_version
        UNIQUE (name, version)
);

CREATE TABLE IF NOT EXISTS trading.trade_package
(
    trade_package_id       UUID PRIMARY KEY,
    strategy_id            UUID REFERENCES trading.strategy(strategy_id),
    package_type           TEXT NOT NULL,
    package_name           TEXT,
    package_status         TEXT NOT NULL DEFAULT 'proposed',
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS trading.trade
(
    trade_id               UUID PRIMARY KEY,
    external_trade_id      TEXT,
    product_definition_id  UUID NOT NULL
                               REFERENCES product.product_definition(product_definition_id),
    portfolio_id           UUID,
    counterparty_id        UUID,
    trade_time             TIMESTAMPTZ NOT NULL,
    trade_date             DATE NOT NULL,
    settlement_date        DATE,
    side                   TEXT NOT NULL CHECK (side IN ('buy', 'sell')),
    quantity               NUMERIC(30,10) NOT NULL CHECK (quantity > 0),
    execution_price        NUMERIC(30,10) NOT NULL CHECK (execution_price >= 0),
    price_currency         CHAR(3) NOT NULL,
    multiplier_snapshot    NUMERIC(30,10) NOT NULL CHECK (multiplier_snapshot > 0),
    gross_consideration    NUMERIC(30,10),
    fees                   NUMERIC(30,10) NOT NULL DEFAULT 0 CHECK (fees >= 0),
    status                 TEXT NOT NULL DEFAULT 'confirmed',
    source                 TEXT NOT NULL DEFAULT 'manual',
    trade_version          INTEGER NOT NULL DEFAULT 1 CHECK (trade_version > 0),
    parent_trade_id        UUID REFERENCES trading.trade(trade_id),
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_by             TEXT NOT NULL DEFAULT CURRENT_USER,

    CONSTRAINT uq_external_trade
        UNIQUE (source, external_trade_id)
);

CREATE INDEX IF NOT EXISTS ix_trade_product
    ON trading.trade(product_definition_id);

CREATE INDEX IF NOT EXISTS ix_trade_time
    ON trading.trade(trade_time);

CREATE TABLE IF NOT EXISTS trading.trade_package_leg
(
    trade_package_leg_id   UUID PRIMARY KEY,
    trade_package_id       UUID NOT NULL
                               REFERENCES trading.trade_package(trade_package_id)
                               ON DELETE CASCADE,
    trade_id               UUID NOT NULL UNIQUE
                               REFERENCES trading.trade(trade_id),
    leg_number             INTEGER NOT NULL CHECK (leg_number > 0),
    leg_role               TEXT,
    ratio                  NUMERIC(30,10) NOT NULL DEFAULT 1,
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT uq_package_leg_number
        UNIQUE (trade_package_id, leg_number)
);

-- ============================================================
-- Curves
-- ============================================================

CREATE TABLE IF NOT EXISTS pricing.curve_definition
(
    curve_definition_id    UUID PRIMARY KEY,
    curve_type             TEXT NOT NULL,
    curve_name             TEXT NOT NULL,
    currency               CHAR(3) NOT NULL,
    day_count_convention   TEXT NOT NULL,
    compounding_convention TEXT NOT NULL,
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT uq_curve_definition
        UNIQUE (curve_name, currency)
);

CREATE TABLE IF NOT EXISTS pricing.curve_snapshot
(
    curve_snapshot_id      UUID PRIMARY KEY,
    curve_definition_id    UUID NOT NULL
                               REFERENCES pricing.curve_definition(curve_definition_id),
    valuation_time         TIMESTAMPTZ NOT NULL,
    market_data_cutoff     TIMESTAMPTZ NOT NULL,
    build_status           TEXT NOT NULL DEFAULT 'completed',
    configuration_id       UUID REFERENCES config.configuration(configuration_id),
    metadata               JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT uq_curve_snapshot
        UNIQUE (curve_definition_id, valuation_time)
);

CREATE TABLE IF NOT EXISTS pricing.curve_node
(
    curve_node_id          UUID PRIMARY KEY,
    curve_snapshot_id      UUID NOT NULL
                               REFERENCES pricing.curve_snapshot(curve_snapshot_id)
                               ON DELETE CASCADE,
    tenor_days             INTEGER NOT NULL CHECK (tenor_days >= 0),
    maturity_date          DATE NOT NULL,
    market_quote           DOUBLE PRECISION,
    zero_rate              DOUBLE PRECISION NOT NULL,
    discount_factor        DOUBLE PRECISION NOT NULL CHECK (discount_factor > 0),
    forward_rate           DOUBLE PRECISION,

    CONSTRAINT uq_curve_node
        UNIQUE (curve_snapshot_id, tenor_days)
);

-- ============================================================
-- Volatility surfaces
-- ============================================================

CREATE TABLE IF NOT EXISTS pricing.vol_surface_definition
(
    vol_surface_definition_id UUID PRIMARY KEY,
    surface_name              TEXT NOT NULL,
    underlying_asset_id       UUID NOT NULL
                                  REFERENCES reference.asset(asset_id),
    quote_convention          TEXT NOT NULL DEFAULT 'strike',
    interpolation_method      TEXT,
    metadata                  JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT uq_vol_surface_definition
        UNIQUE (surface_name, underlying_asset_id)
);

CREATE TABLE IF NOT EXISTS pricing.vol_surface_snapshot
(
    vol_surface_snapshot_id UUID PRIMARY KEY,
    vol_surface_definition_id UUID NOT NULL
                                  REFERENCES pricing.vol_surface_definition(
                                      vol_surface_definition_id),
    valuation_time           TIMESTAMPTZ NOT NULL,
    market_data_cutoff       TIMESTAMPTZ NOT NULL,
    build_status             TEXT NOT NULL DEFAULT 'completed',
    configuration_id         UUID REFERENCES config.configuration(configuration_id),
    metadata                 JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT uq_vol_surface_snapshot
        UNIQUE (vol_surface_definition_id, valuation_time)
);

CREATE TABLE IF NOT EXISTS pricing.vol_surface_point
(
    vol_surface_point_id    UUID PRIMARY KEY,
    vol_surface_snapshot_id UUID NOT NULL
                                REFERENCES pricing.vol_surface_snapshot(
                                    vol_surface_snapshot_id)
                                ON DELETE CASCADE,
    expiry_time             TIMESTAMPTZ NOT NULL,
    strike                  DOUBLE PRECISION NOT NULL CHECK (strike > 0),
    option_right            TEXT CHECK (option_right IN ('call', 'put')),
    moneyness               DOUBLE PRECISION,
    delta                   DOUBLE PRECISION,
    implied_volatility      DOUBLE PRECISION NOT NULL
                                CHECK (implied_volatility >= 0),
    bid_volatility          DOUBLE PRECISION,
    ask_volatility          DOUBLE PRECISION,
    calibration_weight      DOUBLE PRECISION NOT NULL DEFAULT 1,

    CONSTRAINT uq_vol_surface_point
        UNIQUE (vol_surface_snapshot_id, expiry_time, strike, option_right)
);

-- ============================================================
-- Market snapshots
-- ============================================================

CREATE TABLE IF NOT EXISTS pricing.asset_quote_snapshot
(
    asset_quote_snapshot_id UUID PRIMARY KEY,
    asset_id                UUID NOT NULL REFERENCES reference.asset(asset_id),
    observed_at             TIMESTAMPTZ NOT NULL,
    source                  TEXT NOT NULL,
    bid                     DOUBLE PRECISION,
    ask                     DOUBLE PRECISION,
    mid                     DOUBLE PRECISION NOT NULL CHECK (mid > 0),
    metadata                JSONB NOT NULL DEFAULT '{}'::jsonb,

    CONSTRAINT uq_asset_quote_snapshot
        UNIQUE (asset_id, observed_at, source)
);

CREATE TABLE IF NOT EXISTS pricing.market_snapshot
(
    market_snapshot_id      UUID PRIMARY KEY,
    valuation_time          TIMESTAMPTZ NOT NULL,
    market_data_cutoff      TIMESTAMPTZ NOT NULL,
    snapshot_status         TEXT NOT NULL DEFAULT 'complete',
    metadata                JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at              TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pricing.market_snapshot_asset_quote
(
    market_snapshot_asset_quote_id UUID PRIMARY KEY,
    market_snapshot_id             UUID NOT NULL
                                       REFERENCES pricing.market_snapshot(
                                           market_snapshot_id)
                                       ON DELETE CASCADE,
    asset_quote_snapshot_id        UUID NOT NULL
                                       REFERENCES pricing.asset_quote_snapshot(
                                           asset_quote_snapshot_id),

    CONSTRAINT uq_market_snapshot_asset
        UNIQUE (market_snapshot_id, asset_quote_snapshot_id)
);

CREATE TABLE IF NOT EXISTS pricing.market_snapshot_curve
(
    market_snapshot_curve_id UUID PRIMARY KEY,
    market_snapshot_id       UUID NOT NULL
                                 REFERENCES pricing.market_snapshot(
                                     market_snapshot_id)
                                 ON DELETE CASCADE,
    curve_snapshot_id        UUID NOT NULL
                                 REFERENCES pricing.curve_snapshot(curve_snapshot_id),
    curve_role               TEXT NOT NULL,

    CONSTRAINT uq_market_snapshot_curve_role
        UNIQUE (market_snapshot_id, curve_role)
);

CREATE TABLE IF NOT EXISTS pricing.market_snapshot_vol_surface
(
    market_snapshot_vol_surface_id UUID PRIMARY KEY,
    market_snapshot_id             UUID NOT NULL
                                       REFERENCES pricing.market_snapshot(
                                           market_snapshot_id)
                                       ON DELETE CASCADE,
    vol_surface_snapshot_id        UUID NOT NULL
                                       REFERENCES pricing.vol_surface_snapshot(
                                           vol_surface_snapshot_id),
    surface_role                   TEXT NOT NULL,

    CONSTRAINT uq_market_snapshot_surface_role
        UNIQUE (market_snapshot_id, surface_role)
);

-- ============================================================
-- Pricing models and valuation runs
-- ============================================================

CREATE TABLE IF NOT EXISTS pricing.pricing_model
(
    pricing_model_id        UUID PRIMARY KEY,
    model_key               TEXT NOT NULL,
    model_name              TEXT NOT NULL,
    implementation_version  TEXT NOT NULL,
    supported_products      JSONB NOT NULL DEFAULT '[]'::jsonb,
    default_parameters      JSONB NOT NULL DEFAULT '{}'::jsonb,
    is_active               BOOLEAN NOT NULL DEFAULT TRUE,
    created_at              TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT uq_pricing_model_version
        UNIQUE (model_key, implementation_version)
);

CREATE TABLE IF NOT EXISTS pricing.valuation_run
(
    valuation_run_id        UUID PRIMARY KEY,
    valuation_time          TIMESTAMPTZ NOT NULL,
    market_snapshot_id      UUID NOT NULL
                               REFERENCES pricing.market_snapshot(market_snapshot_id),
    pricing_model_id        UUID NOT NULL
                               REFERENCES pricing.pricing_model(pricing_model_id),
    configuration_id        UUID REFERENCES config.configuration(configuration_id),
    purpose                 TEXT NOT NULL,
    status                  TEXT NOT NULL DEFAULT 'requested',
    model_parameters        JSONB NOT NULL DEFAULT '{}'::jsonb,
    started_at              TIMESTAMPTZ,
    completed_at            TIMESTAMPTZ,
    error_message           TEXT,
    created_at              TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS pricing.valuation_result
(
    valuation_result_id     UUID PRIMARY KEY,
    valuation_run_id        UUID NOT NULL
                               REFERENCES pricing.valuation_run(valuation_run_id)
                               ON DELETE CASCADE,
    trade_id                UUID NOT NULL REFERENCES trading.trade(trade_id),
    unit_price              DOUBLE PRECISION NOT NULL,
    present_value           DOUBLE PRECISION NOT NULL,
    currency                CHAR(3) NOT NULL,
    delta                   DOUBLE PRECISION,
    gamma                   DOUBLE PRECISION,
    vega                    DOUBLE PRECISION,
    theta                   DOUBLE PRECISION,
    rho                     DOUBLE PRECISION,
    implied_volatility      DOUBLE PRECISION,
    calculation_time_ms     DOUBLE PRECISION,
    result_details          JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at              TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT uq_valuation_trade
        UNIQUE (valuation_run_id, trade_id)
);

-- ============================================================
-- Risk
-- ============================================================

CREATE TABLE IF NOT EXISTS risk.risk_measure
(
    risk_measure_id         UUID PRIMARY KEY,
    valuation_run_id        UUID NOT NULL
                               REFERENCES pricing.valuation_run(valuation_run_id)
                               ON DELETE CASCADE,
    trade_id                UUID REFERENCES trading.trade(trade_id),
    calculation_time        TIMESTAMPTZ NOT NULL,
    scope_type              TEXT NOT NULL,
    scope_id                TEXT NOT NULL,
    measure_name            TEXT NOT NULL,
    scenario_id             TEXT,
    bucket                  TEXT,
    value                   DOUBLE PRECISION NOT NULL,
    unit                    TEXT,
    details                 JSONB NOT NULL DEFAULT '{}'::jsonb
);

CREATE INDEX IF NOT EXISTS ix_risk_measure_run
    ON risk.risk_measure(valuation_run_id, measure_name);

-- ============================================================
-- Backtesting
-- ============================================================

CREATE TABLE IF NOT EXISTS backtest.definition
(
    backtest_definition_id UUID PRIMARY KEY,
    name                   TEXT NOT NULL,
    strategy_id            UUID REFERENCES trading.strategy(strategy_id),
    parameter_schema       JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at             TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS backtest.run
(
    backtest_run_id         UUID PRIMARY KEY,
    backtest_definition_id  UUID NOT NULL
                                REFERENCES backtest.definition(
                                    backtest_definition_id),
    configuration_id        UUID REFERENCES config.configuration(configuration_id),
    code_version            TEXT NOT NULL,
    parameters              JSONB NOT NULL DEFAULT '{}'::jsonb,
    period_start            TIMESTAMPTZ NOT NULL,
    period_end              TIMESTAMPTZ NOT NULL,
    random_seed             BIGINT,
    status                  TEXT NOT NULL DEFAULT 'requested',
    started_at              TIMESTAMPTZ,
    completed_at            TIMESTAMPTZ,

    CONSTRAINT ck_backtest_period
        CHECK (period_end > period_start)
);

CREATE TABLE IF NOT EXISTS backtest.metric
(
    backtest_metric_id      UUID PRIMARY KEY,
    backtest_run_id         UUID NOT NULL
                                REFERENCES backtest.run(backtest_run_id)
                                ON DELETE CASCADE,
    metric_name             TEXT NOT NULL,
    value                   DOUBLE PRECISION NOT NULL,
    unit                    TEXT,

    CONSTRAINT uq_backtest_metric
        UNIQUE (backtest_run_id, metric_name)
);

COMMIT;