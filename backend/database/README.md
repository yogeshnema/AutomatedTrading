# Database

PostgreSQL owns the physical database files. This directory contains only the
versioned definitions needed to reproduce the AutomatedTrading database.

## Layout

- `migrations/` contains ordered, immutable schema changes.
- `seeds/` contains non-sensitive sample/reference data for local development.

The C++ database layer reads connection settings from environment variables:

| Variable | Default |
| --- | --- |
| `DATABASE_HOST` | `127.0.0.1` |
| `DATABASE_PORT` | `5432` |
| `DATABASE_NAME` | `automated_trading` |
| `DATABASE_USER` | `trading_owner` |
| `DATABASE_PASSWORD` | unset |
| `DATABASE_APPLICATION_NAME` | `AutomatedTrading` |
| `DATABASE_CONNECT_TIMEOUT` | `5` |
| `DATABASE_SSLMODE` | `prefer` |

Do not commit passwords or connection strings containing credentials.

## Client dependency

The implementation uses PostgreSQL's portable `libpq` client library.

- Ubuntu/Debian development package: `libpq-dev`
- Windows/vcpkg port: `libpq`

Migrations and entity tables will be added separately after their schemas and
retention rules are agreed.
