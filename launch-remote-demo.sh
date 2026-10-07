#!/usr/bin/env bash
set -e
cd -- "$(dirname -- "$0")"
exec ./build/minato --remote-demo --profile Demonstration
