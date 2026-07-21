# Local Linux stack deployment

For the inexpensive single-VM deployment, `deploy.sh` provides one repeatable
command for the four C++ services and the React frontend. It performs incremental
CMake compilation, backend tests, the frontend production build, and a controlled
restart with separate PID and log files.

## First-time preparation

Install the existing C++ dependencies plus Node.js/npm, then create the local
environment file:

```bash
cd /home/yogesh-nema/AutomatedTrading
cp backend/database/connection.env.example backend/database/connection.env
nano backend/database/connection.env
chmod +x deploy.sh
```

Keep PostgreSQL and Kite credentials only in `connection.env`; the file is
ignored by Git. Refresh `KITE_ACCESS_TOKEN` after the daily Kite login.

## Rebuild and restart after a pull or code change

```bash
./deploy.sh up
```

This is equivalent to build, test, stop, and start. CMake reuses the `build/`
directory, so unchanged C++ files are not recompiled. `npm ci` runs only when
dependencies are missing or `package-lock.json` changed.

## Operations

```bash
./deploy.sh status
./deploy.sh logs
./deploy.sh logs risk
./deploy.sh restart
./deploy.sh stop
./deploy.sh start
./deploy.sh build
```

Set `RUN_TESTS=0` only when intentionally skipping the test gate:

```bash
RUN_TESTS=0 ./deploy.sh up
```

## Default endpoints

| Component | URL |
| --- | --- |
| Frontend | `http://<linux-vm-ip>:5173` |
| Trade Library | `http://<linux-vm-ip>:8101` |
| Pricing Swagger | `http://<linux-vm-ip>:8080/swagger` |
| Risk Swagger | `http://<linux-vm-ip>:8081/swagger` |
| Market Data | `http://<linux-vm-ip>:8201` |

The frontend is the single browser-facing origin. Its preview proxy routes
`/trade-library`, `/pricing`, `/risk`, and `/market-data` to localhost backend
ports, avoiding browser CORS configuration and keeping backend URLs out of the
mobile/web client.

## Runtime files

The controller writes process IDs and logs beneath `.runtime/`, which is ignored
by Git:

```text
.runtime/pids/
.runtime/logs/
```

It verifies that a recorded PID still belongs to the expected service before
terminating it. Each service is started in its own detached process group, so a
normal SSH disconnect does not tie it to the terminal.

## Next deployment step

Vite preview plus the stack controller is appropriate for the current hobby VM,
but it is not a production process manager. Before public exposure, place Nginx
or Caddy in front of the built frontend and APIs, and install the four backends
as `systemd` services (or containerize them). That adds restart-on-failure,
restart-on-boot, TLS, log rotation, resource limits, and safer zero-downtime
deployments while retaining `deploy.sh build` as the build/test gate.
