#!/usr/bin/env bash

set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
RUNTIME_DIR="${RUNTIME_DIR:-${ROOT_DIR}/.runtime}"
PID_DIR="${RUNTIME_DIR}/pids"
LOG_DIR="${RUNTIME_DIR}/logs"
ENV_FILE="${AUTOMATED_TRADING_ENV:-${ROOT_DIR}/backend/database/connection.env}"
BUILD_JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || printf '2')}"
FRONTEND_HOST="${FRONTEND_HOST:-0.0.0.0}"
FRONTEND_PORT="${FRONTEND_PORT:-5173}"

SERVICES=(trade-library pricing risk market-data frontend)
BACKEND_SERVICES=(trade-library pricing risk market-data)

declare -A EXECUTABLES=(
  [trade-library]="${BUILD_DIR}/backend/TradeLibraryService"
  [pricing]="${BUILD_DIR}/backend/PricingService"
  [risk]="${BUILD_DIR}/backend/RiskService"
  [market-data]="${BUILD_DIR}/backend/MarketDataService"
)

declare -A PROCESS_MARKERS=(
  [trade-library]="TradeLibraryService"
  [pricing]="PricingService"
  [risk]="RiskService"
  [market-data]="MarketDataService"
  [frontend]="npm"
)

log() { printf '[stack] %s\n' "$*"; }
warn() { printf '[stack] WARNING: %s\n' "$*" >&2; }
die() { printf '[stack] ERROR: %s\n' "$*" >&2; exit 1; }

usage()
{
  cat <<'EOF'
Usage: ./deploy.sh <command> [service]

Commands:
  up                 Incrementally build, test, then restart the complete stack
  build              Build and test backend and frontend without restarting
  start              Start already-built services
  restart            Restart already-built services
  stop               Stop all services started by this script
  status             Show process and HTTP health status
  logs [service]     Follow all logs, or one service log

Services: trade-library, pricing, risk, market-data, frontend

Optional environment variables:
  AUTOMATED_TRADING_ENV  Backend environment file
  BUILD_DIR              CMake build directory
  BUILD_JOBS             Parallel compiler jobs
  RUN_TESTS=0            Skip CTest during build
  FRONTEND_HOST          Frontend bind host (default 0.0.0.0)
  FRONTEND_PORT          Frontend port (default 5173)
EOF
}

require_command()
{
  command -v "$1" >/dev/null 2>&1 || die "Required command '$1' was not found."
}

prepare_runtime()
{
  mkdir -p "${PID_DIR}" "${LOG_DIR}"
}

load_environment()
{
  [[ -f "${ENV_FILE}" ]] || die "Environment file not found: ${ENV_FILE}. Copy backend/database/connection.env.example and add local secrets."
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
}

load_environment_if_present()
{
  if [[ -f "${ENV_FILE}" ]]; then
    set -a
    # shellcheck disable=SC1090
    source "${ENV_FILE}"
    set +a
  fi
}

pid_file() { printf '%s/%s.pid' "${PID_DIR}" "$1"; }
service_log() { printf '%s/%s.log' "${LOG_DIR}" "$1"; }

read_pid()
{
  local file
  file="$(pid_file "$1")"
  [[ -f "${file}" ]] || return 1
  local pid
  pid="$(<"${file}")"
  [[ "${pid}" =~ ^[0-9]+$ ]] || return 1
  printf '%s' "${pid}"
}

is_running()
{
  local pid
  pid="$(read_pid "$1")" || return 1
  kill -0 "${pid}" 2>/dev/null
}

verify_owned_process()
{
  local service="$1" pid="$2" command_line=""
  [[ -r "/proc/${pid}/cmdline" ]] || return 1
  command_line="$(tr '\0' ' ' < "/proc/${pid}/cmdline")"
  [[ "${command_line}" == *"${PROCESS_MARKERS[${service}]}"* ]]
}

build_stack()
{
  require_command cmake
  require_command npm
  prepare_runtime

  log "Configuring Release build in ${BUILD_DIR}"
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

  log "Building four backend services with ${BUILD_JOBS} jobs"
  cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS}" --target \
    TradeLibraryService PricingService RiskService MarketDataService \
    BlackScholesPricerTests PricingApplicationServiceTests \
    RiskEngineTests RiskApplicationServiceTests

  if [[ "${RUN_TESTS:-1}" != "0" ]]; then
    log "Running backend unit tests"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
  fi

  log "Preparing frontend dependencies"
  if [[ ! -d "${ROOT_DIR}/frontend/node_modules" ||
        "${ROOT_DIR}/frontend/package-lock.json" -nt "${ROOT_DIR}/frontend/node_modules/.package-lock.json" ]]; then
    npm --prefix "${ROOT_DIR}/frontend" ci
  fi

  log "Building frontend production bundle"
  npm --prefix "${ROOT_DIR}/frontend" run build
}

start_backend()
{
  local service="$1" executable="${EXECUTABLES[${service}]}"
  local file log_file pid
  file="$(pid_file "${service}")"
  log_file="$(service_log "${service}")"

  if is_running "${service}"; then
    log "${service} is already running (PID $(read_pid "${service}"))"
    return 0
  fi
  [[ -x "${executable}" ]] || die "Missing ${executable}; run './deploy.sh build' first."

  : > "${log_file}"
  nohup setsid "${executable}" >> "${log_file}" 2>&1 < /dev/null &
  pid=$!
  printf '%s\n' "${pid}" > "${file}"
  sleep 1
  if ! kill -0 "${pid}" 2>/dev/null; then
    warn "${service} failed to start. Last log lines:"
    tail -n 20 "${log_file}" >&2 || true
    return 1
  fi
  log "Started ${service} (PID ${pid})"
}

start_frontend()
{
  local service="frontend" file log_file pid
  file="$(pid_file "${service}")"
  log_file="$(service_log "${service}")"

  if is_running "${service}"; then
    log "frontend is already running (PID $(read_pid frontend))"
    return 0
  fi
  [[ -f "${ROOT_DIR}/frontend/dist/index.html" ]] || die "Frontend bundle is missing; run './deploy.sh build' first."

  : > "${log_file}"
  (
    cd "${ROOT_DIR}/frontend"
    nohup setsid npm run preview -- --host "${FRONTEND_HOST}" --port "${FRONTEND_PORT}" \
      >> "${log_file}" 2>&1 < /dev/null &
    printf '%s\n' "$!" > "${file}"
  )
  pid="$(read_pid frontend)" || return 1
  sleep 1
  if ! kill -0 "${pid}" 2>/dev/null; then
    warn "frontend failed to start. Last log lines:"
    tail -n 20 "${log_file}" >&2 || true
    return 1
  fi
  log "Started frontend (PID ${pid})"
}

start_stack()
{
  require_command setsid
  require_command nohup
  require_command npm
  prepare_runtime
  load_environment

  local failures=0 service
  for service in "${BACKEND_SERVICES[@]}"; do
    start_backend "${service}" || failures=1
  done
  start_frontend || failures=1
  status_stack
  [[ "${failures}" -eq 0 ]] || die "One or more services failed. Use './deploy.sh logs <service>'."
}

stop_service()
{
  local service="$1" file pid attempt
  file="$(pid_file "${service}")"
  pid="$(read_pid "${service}")" || { rm -f "${file}"; log "${service} is not running"; return 0; }

  if ! kill -0 "${pid}" 2>/dev/null; then
    rm -f "${file}"
    log "Removed stale PID for ${service}"
    return 0
  fi
  if ! verify_owned_process "${service}" "${pid}"; then
    warn "PID ${pid} no longer belongs to ${service}; refusing to terminate it."
    rm -f "${file}"
    return 1
  fi

  kill -TERM -- "-${pid}" 2>/dev/null || kill -TERM "${pid}" 2>/dev/null || true
  for attempt in {1..20}; do
    kill -0 "${pid}" 2>/dev/null || break
    sleep 0.25
  done
  if kill -0 "${pid}" 2>/dev/null; then
    warn "${service} did not stop gracefully; terminating its recorded process group."
    kill -KILL -- "-${pid}" 2>/dev/null || kill -KILL "${pid}" 2>/dev/null || true
  fi
  rm -f "${file}"
  log "Stopped ${service}"
}

stop_stack()
{
  prepare_runtime
  local index
  for ((index=${#SERVICES[@]}-1; index>=0; index--)); do
    stop_service "${SERVICES[index]}" || true
  done
}

service_url()
{
  case "$1" in
    trade-library) printf 'http://127.0.0.1:%s/health/ready' "${TRADE_LIBRARY_PORT:-8101}" ;;
    pricing) printf 'http://127.0.0.1:%s/health/ready' "${PRICING_SERVICE_PORT:-8080}" ;;
    risk) printf 'http://127.0.0.1:%s/health/ready' "${RISK_SERVICE_PORT:-8081}" ;;
    market-data) printf 'http://127.0.0.1:%s/health/ready' "${MARKET_DATA_SERVICE_PORT:-8201}" ;;
    frontend) printf 'http://127.0.0.1:%s/' "${FRONTEND_PORT}" ;;
  esac
}

status_stack()
{
  prepare_runtime
  load_environment_if_present
  local service pid url health
  printf '\n%-16s %-10s %-8s %s\n' 'SERVICE' 'PROCESS' 'PID' 'HTTP'
  printf '%-16s %-10s %-8s %s\n' '---------------' '---------' '-------' '----'
  for service in "${SERVICES[@]}"; do
    pid='-'
    health='not checked'
    if is_running "${service}"; then
      pid="$(read_pid "${service}")"
      if command -v curl >/dev/null 2>&1; then
        url="$(service_url "${service}")"
        if curl --silent --fail --max-time 2 "${url}" >/dev/null; then
          health='ready'
        else
          health='not ready'
        fi
      fi
      printf '%-16s %-10s %-8s %s\n' "${service}" 'running' "${pid}" "${health}"
    else
      printf '%-16s %-10s %-8s %s\n' "${service}" 'stopped' "${pid}" '-'
    fi
  done
  printf '\nFrontend: http://<linux-vm-ip>:%s\n' "${FRONTEND_PORT}"
  printf 'Logs:    %s\n\n' "${LOG_DIR}"
}

follow_logs()
{
  prepare_runtime
  local service="${1:-all}"
  if [[ "${service}" == 'all' ]]; then
    for service in "${SERVICES[@]}"; do touch "$(service_log "${service}")"; done
    tail -n 100 -F "${LOG_DIR}"/*.log
    return
  fi
  [[ " ${SERVICES[*]} " == *" ${service} "* ]] || die "Unknown service '${service}'."
  touch "$(service_log "${service}")"
  tail -n 100 -F "$(service_log "${service}")"
}

command="${1:-up}"
case "${command}" in
  up|deploy)
    load_environment
    build_stack
    stop_stack
    start_stack
    ;;
  build) build_stack ;;
  start) start_stack ;;
  restart)
    stop_stack
    start_stack
    ;;
  stop) stop_stack ;;
  status) status_stack ;;
  logs) follow_logs "${2:-all}" ;;
  help|-h|--help) usage ;;
  *) usage; die "Unknown command '${command}'." ;;
esac
