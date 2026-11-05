#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
native_dir="$test_dir/../../main/cpp"
test_output=$(mktemp -d /tmp/oboe-async-tests.XXXXXX)
trap 'rm -rf "$test_output"' EXIT
sanitizers=address,undefined
if [ "${1:-}" = tsan ]; then sanitizers=thread; fi
c++ -std=c++17 -Wall -Wextra -Werror -pthread -fsanitize="$sanitizers" \
    -I"$native_dir" "$test_dir/AsyncAudioTest.cpp" \
    "$native_dir/AsyncAudioFile.cpp" "$native_dir/filter/NoiseReduction.cpp" \
    -o "$test_output/test"
"$test_output/test" "$test_output"
