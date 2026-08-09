#!/usr/bin/env bash

set -euo pipefail

readonly k_minimum_cmake_major=4
readonly k_postgresql_major=17
readonly k_pgdg_key_url="https://www.postgresql.org/media/keys/ACCC4CF8.asc"
readonly k_pgdg_key_path="/usr/share/postgresql-common/pgdg/apt.postgresql.org.asc"
readonly k_pgdg_source_path="/etc/apt/sources.list.d/pgdg.sources"
readonly k_cluster_policy_path="/etc/postgresql-common/createcluster.d/placedb.conf"
readonly k_script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly k_repository_root="$(cd -- "${k_script_dir}/.." && pwd)"
readonly k_etl_venv="${k_repository_root}/out/venv/etl"
readonly k_etl_requirements="${k_repository_root}/tools/etl/requirements.txt"
readonly k_packages=(
  build-essential
  ca-certificates
  curl
  libdrogon-dev
  libjsoncpp-dev
  ninja-build
  pkg-config
  python3
  python3-venv
  postgresql-common
  postgresql-17
  postgresql-client-17
)

fail() {
  printf '%s\n' "bootstrap-local-fixture: $*" >&2
  exit 1
}

configure_pgdg_repository() {
  local distribution_codename="$1"
  local architecture="$2"
  local temporary_directory
  temporary_directory="$(mktemp -d /tmp/placedb-pgdg-bootstrap.XXXXXX)"
  trap 'rm -rf -- "${temporary_directory}"' RETURN

  curl --fail --silent --show-error --location \
    --output "${temporary_directory}/apt.postgresql.org.asc" \
    "${k_pgdg_key_url}"
  printf '%s\n' \
    "Types: deb" \
    "URIs: https://apt.postgresql.org/pub/repos/apt" \
    "Suites: ${distribution_codename}-pgdg" \
    "Architectures: ${architecture}" \
    "Components: main" \
    "Signed-By: ${k_pgdg_key_path}" \
    > "${temporary_directory}/pgdg.sources"
  printf '%s\n' "create_main_cluster = false" \
    > "${temporary_directory}/placedb.conf"

  sudo install -d -o root -g root -m 0755 \
    /usr/share/postgresql-common/pgdg
  sudo install -o root -g root -m 0644 \
    "${temporary_directory}/apt.postgresql.org.asc" \
    "${k_pgdg_key_path}"
  sudo install -o root -g root -m 0644 \
    "${temporary_directory}/pgdg.sources" \
    "${k_pgdg_source_path}"
  sudo install -d -o root -g root -m 0755 \
    /etc/postgresql-common/createcluster.d
  sudo install -o root -g root -m 0644 \
    "${temporary_directory}/placedb.conf" \
    "${k_cluster_policy_path}"
  trap - RETURN
  rm -rf -- "${temporary_directory}"
}

if [[ ! -r /etc/os-release ]]; then
  fail "cannot identify the operating system"
fi

# shellcheck disable=SC1091
source /etc/os-release
readonly k_distribution_id="${ID:-unknown}"
readonly k_distribution_family=" ${ID_LIKE:-} "
readonly k_distribution_codename="${UBUNTU_CODENAME:-${VERSION_CODENAME:-}}"
if [[ "${k_distribution_id}" != "debian" \
      && "${k_distribution_id}" != "ubuntu" \
      && "${k_distribution_id}" != "linuxmint" \
      && "${k_distribution_family}" != *" debian "* \
      && "${k_distribution_family}" != *" ubuntu "* ]]; then
  fail "supported hosts are Debian-family distributions; found ${k_distribution_id}"
fi

if [[ -z "${k_distribution_codename}" ]]; then
  fail "cannot determine the Debian or Ubuntu base codename"
fi

if ! command -v sudo >/dev/null 2>&1; then
  fail "sudo is required to install system packages"
fi
if ! command -v curl >/dev/null 2>&1; then
  fail "curl is required for the first PGDG repository setup"
fi

sudo -v
readonly k_package_architecture="$(dpkg --print-architecture)"
configure_pgdg_repository \
  "${k_distribution_codename}" \
  "${k_package_architecture}"

sudo apt-get update
sudo apt-get install -y --no-install-recommends "${k_packages[@]}"

if [[ ! -r "${k_etl_requirements}" ]]; then
  fail "ETL requirements not found at ${k_etl_requirements}"
fi
/usr/bin/python3 -m venv "${k_etl_venv}"
"${k_etl_venv}/bin/python" -m pip install \
  --disable-pip-version-check \
  --requirement "${k_etl_requirements}"

readonly k_postgresql_bin_dir="/usr/lib/postgresql/${k_postgresql_major}/bin"
if [[ ! -x "${k_postgresql_bin_dir}/initdb" \
      || ! -x "${k_postgresql_bin_dir}/pg_ctl" ]]; then
  fail "PostgreSQL ${k_postgresql_major} server binaries were not installed"
fi

if pg_lsclusters --no-header 2>/dev/null \
    | awk -v version="${k_postgresql_major}" \
      '$1 == version && $2 == "main" { found = 1 } END { exit !found }'; then
  fail "unexpected PostgreSQL ${k_postgresql_major}/main system cluster exists; it was not removed"
fi

if ! command -v cmake >/dev/null 2>&1; then
  fail "CMake 4 or newer is required but was not found"
fi

cmake_version="$(cmake --version | awk 'NR == 1 { print $3 }')"
cmake_major="${cmake_version%%.*}"
if [[ ! "${cmake_major}" =~ ^[0-9]+$ ]] \
    || (( cmake_major < k_minimum_cmake_major )); then
  fail "CMake 4 or newer is required; found ${cmake_version}"
fi

printf '%s\n' "Local fixture-server dependencies are ready."
printf '%s\n' "Host: ${PRETTY_NAME:-${k_distribution_id}}"
printf '%s\n' "CMake: ${cmake_version}"
drogon_version="$(dpkg-query -W -f='${Version}' libdrogon-dev)"
printf '%s\n' "Drogon package: ${drogon_version}"
postgresql_version="$("${k_postgresql_bin_dir}/postgres" --version)"
printf '%s\n' "PostgreSQL: ${postgresql_version}"
printf '%s\n' "ETL Python: ${k_etl_venv}/bin/python"
printf '%s\n' "No system PostgreSQL cluster was created or started."
