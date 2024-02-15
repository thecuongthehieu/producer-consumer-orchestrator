#!/bin/sh

set -e

make
# ./test_rate_limiter
# ./test_tcp_utils
./main
