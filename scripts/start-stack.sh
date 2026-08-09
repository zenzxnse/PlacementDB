#!/usr/bin/env bash

set -euo pipefail

readonly k_mode="${1:-}"
readonly k_script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly k_root="$(cd -- "${k_script_dir}/.." && pwd)"
readonly k_fixture="${k_root}/tests/db/fixtures/postgres_fixture.sh"
readonly k_archive="${k_root}/pbdata/SRMIST_Placements_NDJSON_SCHEMA2_LOSSLESS_f98563c4.zip"
readonly k_archive_sha256="f98563c46b93923f49dcf900168fa5e6bc271a55718c9e574b95e7a7d9f29d72"
readonly k_run_dir="${k_root}/out/run"
readonly k_lock_file="${k_run_dir}/placedb-stack.lock"
readonly k_backend_port="${PLACEDB_PORT:-8080}"
readonly k_frontend_port="${PORT:-3000}"
readonly k_public_origin="http://127.0.0.1:${k_frontend_port}"

if [[ "${k_mode}" != "dev" && "${k_mode}" != "prod" ]]; then
  printf '%s\n' "Usage: $0 {dev|prod}" >&2
  exit 2
fi

mkdir -p "${k_run_dir}"
exec 9>"${k_lock_file}"
if ! flock --nonblock 9; then
  printf '%s\n' "PlacementDB is already running (lock: ${k_lock_file})." >&2
  exit 1
fi

backend_pid=""
frontend_pid=""
postgres_owned=false
cleaning=false

cleanup() {
  local status=$?
  if [[ "${cleaning}" == true ]]; then
    return
  fi
  cleaning=true
  trap - EXIT HUP INT TERM
  for pid in "${frontend_pid}" "${backend_pid}"; do
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
      kill -TERM -- "-${pid}" 2>/dev/null || true
    fi
  done
  for pid in "${frontend_pid}" "${backend_pid}"; do
    if [[ -n "${pid}" ]]; then
      wait "${pid}" 2>/dev/null || true
    fi
  done
  if [[ "${postgres_owned}" == true ]]; then
    "${k_fixture}" stop >/dev/null 2>&1 || true
  fi
  printf '%s\n' "PlacementDB stopped."
  exit "${status}"
}

trap cleanup EXIT HUP INT TERM

if [[ ! -s /tmp/placedb-postgres-fixture-bootstrap-password ]]; then
  "${k_fixture}" prepare
fi
if ! "${k_fixture}" status >/dev/null 2>&1; then
  "${k_fixture}" start
fi
postgres_owned=true

if [[ "$("${k_fixture}" psql-bootstrap --tuples-only --no-align \
    --command="SELECT to_regclass('public.questions') IS NOT NULL")" != "t" ]]; then
  for migration in "${k_root}"/db/migrations/*.sql; do
    "${k_fixture}" psql-bootstrap --quiet --file="${migration}"
  done
  "${k_fixture}" configure-access
  "${k_fixture}" psql-bootstrap --quiet --file="${k_root}/db/seeds/seed_data.sql"
else
  "${k_fixture}" configure-access
fi

avatar_column_exists="$("${k_fixture}" psql-bootstrap --tuples-only --no-align \
  --command="SELECT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema='public' AND table_name='profiles' AND column_name='avatar_key')")"
if [[ "${avatar_column_exists}" != "t" ]]; then
  "${k_fixture}" psql-bootstrap --quiet \
    --file="${k_root}/db/migrations/013_profiles_comments_avatars.sql"
fi

accepted_imports="$("${k_fixture}" psql-bootstrap --tuples-only --no-align \
  --command="SELECT count(*) FROM import_batches WHERE archive_sha256='${k_archive_sha256}'")"
if [[ "${accepted_imports}" == "0" ]]; then
  PGPASSWORD="$(</tmp/placedb-postgres-fixture-bootstrap-password)" \
    python3 "${k_root}/tools/import/load_archive.py" "${k_archive}" \
    --dsn "host=127.0.0.1 port=55432 dbname=placedb_fixture user=placedb_fixture_admin" \
    --report "${k_root}/out/import/load-report.json" >/dev/null
elif [[ "${accepted_imports}" != "1" ]]; then
  printf '%s\n' "Unexpected accepted import count: ${accepted_imports}." >&2
  exit 1
fi

if [[ "${k_mode}" == "prod" ]]; then
  cmake --preset linux-release
  cmake --build --preset linux-release --target placedb_server
  (cd "${k_root}/web/app" && npm run build)
  backend_binary="${k_root}/out/build/linux-release/placedb_server"
  frontend_command=(node build/index.js)
else
  cmake --preset linux-dev
  cmake --build --preset linux-dev --target placedb_server
  backend_binary="${k_root}/out/build/linux-dev/placedb_server"
  frontend_command=(npm run dev -- --port "${k_frontend_port}")
fi

setsid env \
  PLACEDB_DB_HOST=127.0.0.1 \
  PLACEDB_DB_PORT=55432 \
  PLACEDB_DB_NAME=placedb_fixture \
  PLACEDB_DB_USER=placedb_app \
  PLACEDB_DB_PASSWORD="$(</tmp/placedb-postgres-fixture-app-password)" \
  PLACEDB_PUBLIC_ORIGIN="${k_public_origin}" \
  PLACEDB_LOGIN_CSRF_MAC_KEY=fixture-only-login-csrf-key-32bytes-minimum \
  PLACEDB_SECURE_COOKIES=false \
  PLACEDB_PORT="${k_backend_port}" \
  "${backend_binary}" &
backend_pid=$!

(cd "${k_root}/web/app" && exec setsid env \
  PLACEDB_API_BASE="http://127.0.0.1:${k_backend_port}/api/v1" \
  PLACEDB_PUBLIC_ORIGIN="${k_public_origin}" \
  ORIGIN="${k_public_origin}" \
  HOST=127.0.0.1 \
  PORT="${k_frontend_port}" \
  "${frontend_command[@]}") &
frontend_pid=$!

printf '%s\n' \
  "PlacementDB ${k_mode} stack is running." \
  "Frontend: ${k_public_origin}" \
  "Backend:  http://127.0.0.1:${k_backend_port}" \
  "Database: 127.0.0.1:55432" \
  "Press Ctrl-C once to stop all three services."

wait -n "${backend_pid}" "${frontend_pid}"
printf '%s\n' "A PlacementDB service exited; stopping the stack." >&2
exit 1
