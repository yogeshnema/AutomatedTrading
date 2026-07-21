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
    option_type          TEXT NOT NULL CHECK (option_type IN ('CE', 'PE')),
    refreshed_at         TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (provider, instrument_token),
    UNIQUE (provider, exchange, trading_symbol)
);

CREATE INDEX IF NOT EXISTS ix_instrument_master_search
    ON market_data.instrument_master(provider, expiry, strike, option_type);

CREATE TABLE IF NOT EXISTS market_data.subscription
(
    provider             TEXT NOT NULL,
    instrument_token     BIGINT NOT NULL,
    interval             TEXT NOT NULL DEFAULT 'minute' CHECK (interval = 'minute'),
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

CREATE TABLE IF NOT EXISTS market_data.service_state
(
    singleton                         BOOLEAN PRIMARY KEY DEFAULT TRUE CHECK (singleton),
    instrument_master_refreshed_at    TIMESTAMPTZ
);

INSERT INTO market_data.service_state(singleton) VALUES (TRUE)
ON CONFLICT(singleton) DO NOTHING;

COMMIT;
