\set ON_ERROR_STOP on

BEGIN;

CREATE SCHEMA IF NOT EXISTS market_data;

CREATE TABLE IF NOT EXISTS market_data.instrument_master
(
    provider             TEXT NOT NULL,
    instrument_token     BIGINT NOT NULL,
    exchange             TEXT NOT NULL,
    trading_symbol       TEXT NOT NULL,
    name                 TEXT NOT NULL,
    expiry               DATE NOT NULL,
    strike               NUMERIC(20,6) NOT NULL,
    lot_size             NUMERIC(20,6) NOT NULL DEFAULT 1 CHECK (lot_size > 0),
    option_type          TEXT NOT NULL CHECK (option_type IN ('CE', 'PE')),
    refreshed_at         TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (provider, instrument_token),
    UNIQUE (provider, exchange, trading_symbol)
);

ALTER TABLE market_data.instrument_master
    ADD COLUMN IF NOT EXISTS lot_size NUMERIC(20,6) NOT NULL DEFAULT 1;

CREATE INDEX IF NOT EXISTS ix_instrument_master_search
    ON market_data.instrument_master(provider, expiry, strike, option_type);

CREATE TABLE IF NOT EXISTS market_data.subscription
(
    provider             TEXT NOT NULL,
    instrument_token     BIGINT NOT NULL,
    interval             TEXT NOT NULL DEFAULT 'minute',
    enabled              BOOLEAN NOT NULL DEFAULT TRUE,
    last_refresh_at      TIMESTAMPTZ,
    last_candle_at       TIMESTAMPTZ,
    last_error           TEXT,
    created_at           TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at           TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (provider, instrument_token),
    FOREIGN KEY (provider, instrument_token)
        REFERENCES market_data.instrument_master(provider, instrument_token)
);

ALTER TABLE market_data.subscription
    DROP CONSTRAINT IF EXISTS subscription_interval_check;
ALTER TABLE market_data.subscription
    ADD CONSTRAINT subscription_interval_check CHECK (interval IN ('minute', 'day'));

CREATE TABLE IF NOT EXISTS market_data.candle
(
    provider             TEXT NOT NULL,
    instrument_token     BIGINT NOT NULL,
    interval             TEXT NOT NULL,
    candle_time          TIMESTAMPTZ NOT NULL,
    open                 NUMERIC(20,8) NOT NULL,
    high                 NUMERIC(20,8) NOT NULL,
    low                  NUMERIC(20,8) NOT NULL,
    close                NUMERIC(20,8) NOT NULL,
    volume               BIGINT NOT NULL,
    received_at          TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (provider, instrument_token, interval, candle_time),
    FOREIGN KEY (provider, instrument_token)
        REFERENCES market_data.instrument_master(provider, instrument_token)
);

CREATE INDEX IF NOT EXISTS ix_candle_latest
    ON market_data.candle(provider, instrument_token, interval, candle_time DESC);

CREATE TABLE IF NOT EXISTS market_data.reference_series
(
    series_code          TEXT PRIMARY KEY,
    series_type          TEXT NOT NULL CHECK (series_type IN ('underlying_spot', 'implied_volatility', 'risk_free_rate')),
    display_name         TEXT NOT NULL,
    provider             TEXT NOT NULL,
    currency             CHAR(3),
    tenor                TEXT,
    acquisition_mode     TEXT NOT NULL CHECK (acquisition_mode IN ('vendor', 'derived', 'manual')),
    description          TEXT,
    enabled              BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE TABLE IF NOT EXISTS market_data.reference_subscription
(
    series_code          TEXT PRIMARY KEY REFERENCES market_data.reference_series(series_code),
    enabled              BOOLEAN NOT NULL DEFAULT TRUE,
    status               TEXT NOT NULL DEFAULT 'requested',
    last_refresh_at      TIMESTAMPTZ,
    last_observed_at     TIMESTAMPTZ,
    last_error           TEXT,
    created_at           TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at           TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS market_data.reference_observation
(
    series_code          TEXT NOT NULL REFERENCES market_data.reference_series(series_code),
    scope_key            TEXT NOT NULL DEFAULT 'GLOBAL',
    observed_at          TIMESTAMPTZ NOT NULL,
    value                DOUBLE PRECISION NOT NULL,
    source               TEXT NOT NULL,
    metadata             JSONB NOT NULL DEFAULT '{}'::jsonb,
    received_at          TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (series_code, scope_key, observed_at)
);

INSERT INTO market_data.reference_series
    (series_code, series_type, display_name, provider, currency, tenor, acquisition_mode, description)
VALUES
    ('NIFTY_50_EOD', 'underlying_spot', 'NIFTY 50 official close', 'kite', 'INR', NULL, 'vendor',
     'Underlying close required by option pricing; configure the Kite index token adapter.'),
    ('NIFTY_CONTRACT_IV', 'implied_volatility', 'NIFTY contract implied volatility', 'internal', 'INR', NULL, 'derived',
     'Derived from option EOD, underlying close and the selected discount curve.'),
    ('FBIL_MIBOR_ON', 'risk_free_rate', 'FBIL Overnight MIBOR', 'fbil', 'INR', 'ON', 'vendor',
     'INR overnight benchmark; a licensed/approved FBIL ingestion adapter is required.'),
    ('FBIL_TERM_MIBOR_3M', 'risk_free_rate', 'FBIL Term MIBOR 3M', 'fbil', 'INR', '3M', 'vendor',
     'INR term benchmark; a licensed/approved FBIL ingestion adapter is required.'),
    ('FBIL_MODIFIED_MIFOR_3M', 'risk_free_rate', 'FBIL Modified MIFOR 3M', 'fbil', 'INR', '3M', 'vendor',
     'FX-linked implied INR curve; use only where the product/model requires MIFOR.')
ON CONFLICT (series_code) DO UPDATE SET
    display_name=EXCLUDED.display_name, provider=EXCLUDED.provider, currency=EXCLUDED.currency,
    tenor=EXCLUDED.tenor, acquisition_mode=EXCLUDED.acquisition_mode, description=EXCLUDED.description;

CREATE TABLE IF NOT EXISTS market_data.service_state
(
    singleton                         BOOLEAN PRIMARY KEY DEFAULT TRUE CHECK (singleton),
    instrument_master_refreshed_at    TIMESTAMPTZ
);

INSERT INTO market_data.service_state(singleton) VALUES (TRUE)
ON CONFLICT(singleton) DO NOTHING;

COMMIT;
