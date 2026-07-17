# Pricing service

`PricingService` is a long-running HTTP microservice. It loads a persisted trade
and the latest complete market snapshot at or before the requested valuation
date, then prices the trade through `IPricingModel`.

## Environment

The database variables are documented in `database/connection.env.example`.
The service also accepts:

| Variable | Default |
| --- | --- |
| `PRICING_SERVICE_HOST` | `0.0.0.0` |
| `PRICING_SERVICE_PORT` | `8080` |

## Linux build and run

```bash
sudo apt install build-essential cmake libpq-dev
cmake --fresh -S . -B build
cmake --build build --target PricingService -j
DATABASE_PASSWORD='your-local-password' ./build/PricingService
```

Open `http://localhost:8080/swagger` or call:

```bash
curl -X POST http://localhost:8080/api/v1/prices \
  -H 'Content-Type: application/json' \
  -d '{
    "tradeId": "90000000-0000-0000-0000-000000000001",
    "valuationDate": "2026-01-01T00:00:00Z"
  }'
```

Swagger UI assets are loaded from the pinned `swagger-ui-dist` CDN. The OpenAPI
document itself is always available locally at `/openapi.json`.
