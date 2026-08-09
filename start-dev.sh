#!/usr/bin/env bash
set -euo pipefail
readonly k_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "${k_root}/scripts/start-stack.sh" dev
