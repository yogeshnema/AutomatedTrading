# Market Data service

`MarketDataService` is a long-running Linux-friendly backend process. At startup
it downloads the current Kite instrument master, keeps the NFO NIFTY option
catalogue in PostgreSQL, and polls every enabled subscription for one-minute
historical candles. A five-minute rolling window is upserted on every cycle so
late or revised candles do not create duplicates.

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
- `GET /api/v1/subscriptions`
- `POST /api/v1/subscriptions`
- `DELETE /api/v1/subscriptions/{instrumentToken}`

The React development server proxies `/market-data` to port `8201`.
