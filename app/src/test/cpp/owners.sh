#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
native_dir="$test_dir/../../main/cpp"
test_output=$(mktemp -d /tmp/oboe-owner-tests.XXXXXX)
trap 'rm -rf "$test_output"' EXIT
sanitizers=address,undefined
if [ "${1:-}" = tsan ]; then sanitizers=thread; fi
c++ -std=c++17 -Wall -Wextra -Werror -pthread -fsanitize="$sanitizers" \
    -I"$test_dir" -I"$native_dir" \
    "$test_dir/NativeOwnerTest.cpp" "$native_dir/AudioRecorder.cpp" \
    "$native_dir/AudioPlayer.cpp" "$native_dir/AsyncAudioFile.cpp" "$native_dir"/filter/*.cpp \
    -o "$test_output/test"
"$test_output/test" "$test_output"
