# Market Data service

`MarketDataService` is a long-running Linux-friendly backend process. At startup
it downloads the current Kite instrument master, keeps the NFO NIFTY option
catalogue in PostgreSQL, and polls every enabled subscription for one-minute
historical candles. A five-minute rolling window is upserted on every cycle so
late or revised candles do not create duplicates.

Collection follows NSE's weekday session in `Asia/Kolkata`: minute candles are
collected from 09:15 through 15:30. From 15:35, the service reconciles the full
one-minute session and persists the provider's daily candle as the EOD record.
The daily candle is the completion marker, so a service restart or intraday
outage is repaired before the day is considered complete. Exchange holidays are
not yet modelled.

Unsubscribing disables future vendor requests but deliberately retains all
stored minute and daily candles for strategy research and backtesting.

## Database migration

```bash
cd backend
psql -h 127.0.0.1 -U trading_owner -d automated_trading \
  -v ON_ERROR_STOP=1 -f database/migrations/market_data.sql
```

## Daily Kite login

Kite Connect requires an interactive login. Secrets stay on the Linux backend.

```bash
export KITE_API_KEY='...'
export KITE_API_SECRET='...'
```

1. Open `https://kite.zerodha.com/connect/login?v=3&api_key=$KITE_API_KEY`
   in a browser (replace the variable with its value if opening manually).
2. Complete login. Kite redirects to the URL registered for the app; copy the
   short-lived `request_token` query parameter.
3. Exchange it immediately:

```bash
export KITE_REQUEST_TOKEN='...'
eval "$(./build/backend/KiteSessionToken)"
```

`KiteSessionToken` prints only the shell command that exports the access token.
Do not commit it or expose it through a `VITE_*` variable. The access token
normally expires at 6 AM the following day and must be renewed.

## Build and run on Linux

From the repository root:

```bash
cmake --fresh -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MarketDataService KiteSessionToken -j

set -a
. backend/database/connection.env
set +a

export TZ=Asia/Kolkata
./build/backend/MarketDataService
```

The service defaults to `0.0.0.0:8201`. Configuration is entirely environment
based; see `backend/database/connection.env.example`.

## Browser-facing API

- `GET /health/live`
- `GET /health/ready`
- `GET /api/v1/market-data/status`
- `POST /api/v1/instruments/refresh`
- `GET /api/v1/instruments?search=NIFTY&expiry=YYYY-MM-DD&optionType=CE`
- `GET /api/v1/instrument-facets?name=NIFTY&expiry=YYYY-MM-DD&optionType=CE`
- `GET /api/v1/subscriptions`
- `POST /api/v1/subscriptions`
- `DELETE /api/v1/subscriptions/{instrumentToken}`
- `GET /api/v1/instruments/{instrumentToken}/market-data?date=YYYY-MM-DD`
- `GET /api/v1/reference-series?type=risk_free_rate`
- `POST /api/v1/reference-subscriptions`

The market-data response contains the stored one-minute series, the EOD candle
when available, and session statistics (open, high, low, close, absolute and
percentage change, volume, coverage count, and first/last candle timestamps).

Instrument subscriptions accept `minute` or `day`. A minute subscription also
receives the post-close daily candle; an EOD-only `day` request never downgrades
an existing minute subscription.

## Pricing dependencies

Saving an instrument-master-backed option trade requests four dependencies:

1. The selected option's EOD candle.
2. The NIFTY 50 underlying close (`NIFTY_50_EOD`).
3. Contract-scoped implied volatility (`NIFTY_CONTRACT_IV`).
4. The selected INR rate series, such as `FBIL_MIBOR_ON`.

Reference subscriptions are persisted separately from Kite instrument
subscriptions. A `requested` status means the dependency is registered, not that
an observation is ready. FBIL ingestion and the contract-IV derivation worker are
adapter boundaries still to be implemented. MIFOR is retained for FX-linked use
cases and is not selected by default for NIFTY options.

For local development, observations can be inserted explicitly. Rates and
volatility use decimal values, not percentages:

```sql
INSERT INTO market_data.reference_observation
    (series_code, scope_key, observed_at, value, source)
VALUES
    ('NIFTY_50_EOD', 'GLOBAL', CURRENT_TIMESTAMP, 25100.00, 'manual'),
    ('FBIL_MIBOR_ON', 'GLOBAL', CURRENT_TIMESTAMP, 0.0650, 'manual'),
    ('NIFTY_CONTRACT_IV', '<kite-instrument-token>', CURRENT_TIMESTAMP, 0.1825, 'manual')
ON CONFLICT (series_code, scope_key, observed_at) DO UPDATE SET value=EXCLUDED.value;
```

Once these inputs exist at or before the requested valuation time, Pricing and
Risk can calculate a `DRAFT` Trade Library option without a normalized confirmed
trade snapshot. This fallback is intended for the ad-hoc draft workflow; the
normalized snapshot path remains authoritative for confirmed production trades.

The React development server proxies `/market-data` to port `8201`.
