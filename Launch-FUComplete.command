#!/bin/bash
set -e
WORKDIR="$(cd -- "$(dirname -- "$0")" && pwd)"
cd -- "$WORKDIR"
exec python3 "$WORKDIR/run_fuc.py"
