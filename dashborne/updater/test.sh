#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 build.py --host --test
python3 test_updater.py
