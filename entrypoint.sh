#!/bin/bash

DSN="${DATABASE_URL:-postgres}"
PORT="${PORT:-5000}"

exec /usr/local/bin/rest-in-c-major "$DSN" "$PORT"
