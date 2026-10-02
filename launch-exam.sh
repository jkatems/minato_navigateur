#!/usr/bin/env bash
set -e
cd -- "$(dirname -- "$0")"
exec ./build/minato --exam --profile Examen
