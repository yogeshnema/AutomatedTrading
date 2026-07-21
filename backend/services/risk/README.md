# Risk service

`RiskService` is a long-running HTTP microservice that loads a persisted trade
and the latest complete PostgreSQL market snapshot at or before the requested
valuation time. The first implementation calculates analytic Black-Scholes
Greeks for European vanilla equity options.

The calculation is separated behind `IRiskEngine`. Its scalar CPU implementation
also exposes an allocation-free batch contract using caller-owned spans. Future
SIMD, CUDA, automatic-differentiation, bump-and-revalue, and scenario engines can
implement the same boundary without changing the HTTP or application layers.

## Greek conventions

- Delta: value change for a 1.00 spot move.
- Gamma: delta change for a 1.00 spot move.
- Vega: value change for one volatility-point move.
- Theta: value change per calendar day.
- Rho: value change for one basis-point rate move.

The response contains both theoretical unit Greeks and position Greeks scaled by
signed quantity and contract multiplier.

## Linux build and run

```bash
cmake --fresh -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target RiskService -j

set -a
. backend/database/connection.env
set +a

./build/backend/RiskService
```

Configuration:

| Variable | Default |
| --- | --- |
| `RISK_SERVICE_HOST` | `0.0.0.0` |
| `RISK_SERVICE_PORT` | `8081` |
| `RISK_SERVICE_THREADS` | `4` |

Open `http://localhost:8081/swagger`, or call:

```bash
curl -X POST http://localhost:8081/api/v1/risk/greeks \
  -H 'Content-Type: application/json' \
  -d '{
    "tradeId": "90000000-0000-0000-0000-000000000001",
    "valuationDate": "2026-01-01T00:00:00Z",
    "method": "analytic"
  }'
```

`GET /api/v1/risk/capabilities` reports the installed model, method, compute
backend, batch support, and HTTP worker count. Latency fields separate PostgreSQL
loading from the calculation kernel so optimization targets remain visible.
