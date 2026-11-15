#!/bin/sh
set -eu

chown -R mosquitto:mosquitto /mosquitto/data /mosquitto/log
exec gosu mosquitto "$@"
