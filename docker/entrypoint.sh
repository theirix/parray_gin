#!/bin/sh
set -e
pg_ctl start
exec "$@"
