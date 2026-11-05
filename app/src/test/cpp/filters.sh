#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
native_dir="$test_dir/../../main/cpp"
test_binary=$(mktemp /tmp/oboe-filter-tests.XXXXXX)
trap 'rm -f "$test_binary"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I"$test_dir" -I"$native_dir" \
    "$test_dir/FilterBehaviorTest.cpp" "$native_dir"/filter/*.cpp \
    -o "$test_binary"
"$test_binary"
