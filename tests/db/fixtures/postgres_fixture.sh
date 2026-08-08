#!/usr/bin/env bash

set -euo pipefail

# Disposable native PostgreSQL 17 fixture. It runs an ephemeral cluster with
# initdb and pg_ctl from the PGDG postgresql-17 package. No Docker, no system
# cluster, loopback only. The destructive reset scope is exactly the fixture
# root directory and the three named password files.

readonly k_pg_bindir="/usr/lib/postgresql/17/bin"
readonly k_fixture_root="/tmp/placedb-postgres-fixture"
readonly k_data_dir="${k_fixture_root}/data"
readonly k_socket_dir="${k_fixture_root}/socket"
readonly k_log_file="${k_fixture_root}/postgres.log"
readonly k_host_port="55432"
readonly k_bootstrap_password_file="/tmp/placedb-postgres-fixture-bootstrap-password"
readonly k_app_password_file="/tmp/placedb-postgres-fixture-app-password"
readonly k_migration_password_file="/tmp/placedb-postgres-fixture-migration-password"
readonly k_script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

require_pg_binaries() {
  if [[ ! -x "${k_pg_bindir}/initdb" || ! -x "${k_pg_bindir}/pg_ctl" ]]; then
    printf '%s\n' "PostgreSQL 17 binaries not found in ${k_pg_bindir}." >&2
    printf '%s\n' "Install the PGDG postgresql-17 package first." >&2
    exit 1
  fi
}

generate_password_file() {
  local target_path="$1"
  umask 077
  od -An -N32 -tx1 /dev/urandom | tr -d ' \n' > "${target_path}"
}

prepare_secrets() {
  if [[ ! -s "${k_bootstrap_password_file}" ]]; then
    generate_password_file "${k_bootstrap_password_file}"
  fi
  if [[ ! -s "${k_app_password_file}" ]]; then
    generate_password_file "${k_app_password_file}"
  fi
  if [[ ! -s "${k_migration_password_file}" ]]; then
    generate_password_file "${k_migration_password_file}"
  fi
  chmod 0600 \
    "${k_bootstrap_password_file}" \
    "${k_app_password_file}" \
    "${k_migration_password_file}"
  printf '%s\n' "Prepared fixture secrets without printing them."
}

init_cluster() {
  umask 077
  mkdir -p "${k_data_dir}" "${k_socket_dir}"
  "${k_pg_bindir}/initdb" \
    --pgdata="${k_data_dir}" \
    --username=placedb_fixture_admin \
    --pwfile="${k_bootstrap_password_file}" \
    --auth-host=scram-sha-256 \
    --auth-local=scram-sha-256 \
    --encoding=UTF8 \
    --no-instructions
  {
    printf '%s\n' "listen_addresses = '127.0.0.1'"
    printf '%s\n' "port = ${k_host_port}"
    printf '%s\n' "unix_socket_directories = '${k_socket_dir}'"
    printf '%s\n' "password_encryption = scram-sha-256"
    printf '%s\n' "max_connections = 30"
    printf '%s\n' "statement_timeout = 10000"
    printf '%s\n' "idle_in_transaction_session_timeout = 10000"
  } >> "${k_data_dir}/postgresql.conf"
}

start_fixture() {
  require_pg_binaries
  if [[ ! -s "${k_bootstrap_password_file}" ]]; then
    printf '%s\n' "Run '$0 prepare' first." >&2
    exit 1
  fi
  if [[ ! -s "${k_data_dir}/PG_VERSION" ]]; then
    init_cluster
  fi
  "${k_pg_bindir}/pg_ctl" \
    --pgdata="${k_data_dir}" \
    --log="${k_log_file}" \
    --timeout=30 \
    --wait \
    start
  ensure_database
  "${k_pg_bindir}/pg_isready" --host=127.0.0.1 --port="${k_host_port}"
}

ensure_database() {
  local exists
  exists="$(run_psql_safe bootstrap postgres --tuples-only --no-align \
    --command="SELECT 1 FROM pg_database WHERE datname = 'placedb_fixture'")"
  if [[ "${exists}" != "1" ]]; then
    run_psql_safe bootstrap postgres \
      --command="CREATE DATABASE placedb_fixture OWNER placedb_fixture_admin"
  fi
}

show_status() {
  require_pg_binaries
  "${k_pg_bindir}/pg_ctl" --pgdata="${k_data_dir}" status
  "${k_pg_bindir}/pg_isready" --host=127.0.0.1 --port="${k_host_port}"
}

show_connection() {
  printf '%s\n' "host=127.0.0.1 port=${k_host_port} dbname=placedb_fixture user=placedb_app"
  printf '%s\n' "App password file: ${k_app_password_file}"
  printf '%s\n' "Migration password file: ${k_migration_password_file}"
}

stop_fixture() {
  require_pg_binaries
  "${k_pg_bindir}/pg_ctl" \
    --pgdata="${k_data_dir}" \
    --mode=fast \
    --timeout=30 \
    --wait \
    stop
}

reset_fixture() {
  if [[ -s "${k_data_dir}/PG_VERSION" ]] \
      && "${k_pg_bindir}/pg_ctl" --pgdata="${k_data_dir}" status \
        >/dev/null 2>&1; then
    "${k_pg_bindir}/pg_ctl" \
      --pgdata="${k_data_dir}" \
      --mode=fast \
      --timeout=30 \
      --wait \
      stop
  fi
  rm -rf "${k_fixture_root}"
  rm -f \
    "${k_bootstrap_password_file}" \
    "${k_app_password_file}" \
    "${k_migration_password_file}"
}

# Password-safe psql wrapper. It creates a mode-0600 PGPASSFILE for exactly the
# fixture host, port, and user. The child environment contains only the
# temporary file path, not the password. The file is deleted immediately after
# psql exits.
#
# Usage:
#   $0 psql-bootstrap <args>  - run as the fixture bootstrap role
#   $0 psql-app <args>        - run as placedb_app
#   $0 psql-migrate <args>    - run as placedb_migrate
run_psql_safe() {
  local role="$1"
  shift
  local dbname="placedb_fixture"
  if [[ "${1:-}" == "postgres" ]]; then
    dbname="postgres"
    shift
  fi
  local password_file
  local psql_user
  case "${role}" in
    bootstrap)
      password_file="${k_bootstrap_password_file}"
      psql_user="placedb_fixture_admin"
      ;;
    app)
      password_file="${k_app_password_file}"
      psql_user="placedb_app"
      ;;
    migrate)
      password_file="${k_migration_password_file}"
      psql_user="placedb_migrate"
      ;;
    *)
      printf '%s\n' "Unknown role: ${role}." >&2
      exit 2
      ;;
  esac
  if [[ ! -s "${password_file}" ]]; then
    printf '%s\n' "Password file not found. Run '$0 prepare' first." >&2
    exit 1
  fi
  local pgpass_file
  pgpass_file="$(mktemp /tmp/placedb-postgres-fixture-pgpass.XXXXXX)"
  chmod 0600 "${pgpass_file}"
  printf '127.0.0.1:%s:%s:%s:%s\n' \
    "${k_host_port}" "${dbname}" "${psql_user}" "$(<"${password_file}")" \
    > "${pgpass_file}"
  local psql_status=0
  PGPASSFILE="${pgpass_file}" "${k_pg_bindir}/psql" \
    --host=127.0.0.1 \
    --port="${k_host_port}" \
    --dbname="${dbname}" \
    --username="${psql_user}" \
    --no-psqlrc \
    --set=ON_ERROR_STOP=1 \
    "$@" || psql_status=$?
  rm -f "${pgpass_file}"
  return "${psql_status}"
}

configure_fixture_access() {
  local access_file
  if [[ ! -s "${k_app_password_file}" \
      || ! -s "${k_migration_password_file}" ]]; then
    printf '%s\n' "Run '$0 prepare' first." >&2
    exit 1
  fi
  access_file="$(mktemp /tmp/placedb-postgres-fixture-access.XXXXXX.sql)"
  chmod 0600 "${access_file}"
  printf "\\set app_password '%s'\n" "$(<"${k_app_password_file}")" \
    > "${access_file}"
  printf "\\set migration_password '%s'\n" \
    "$(<"${k_migration_password_file}")" >> "${access_file}"
  printf "\\ir '%s'\n" "${k_script_dir}/fixture_access.sql" \
    >> "${access_file}"
  local psql_status=0
  run_psql_safe bootstrap --file="${access_file}" || psql_status=$?
  rm -f "${access_file}"
  return "${psql_status}"
}

case "${1:-}" in
  prepare)
    prepare_secrets
    ;;
  start)
    start_fixture
    ;;
  status)
    show_status
    ;;
  connection)
    show_connection
    ;;
  stop)
    stop_fixture
    ;;
  reset)
    reset_fixture
    ;;
  psql-app)
    shift
    run_psql_safe app "$@"
    ;;
  psql-bootstrap)
    shift
    run_psql_safe bootstrap "$@"
    ;;
  psql-migrate)
    shift
    run_psql_safe migrate "$@"
    ;;
  configure-access)
    configure_fixture_access
    ;;
  *)
    printf '%s\n' "Usage: $0 {prepare|start|status|connection|stop|reset|configure-access|psql-bootstrap|psql-app|psql-migrate}" >&2
    exit 2
    ;;
esac
