#!/bin/sh
set -eu
test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
native_dir="$test_dir/../../main/cpp"
test_output=$(mktemp -d /tmp/oboe-jni-lifecycle.XXXXXX)
trap 'rm -rf "$test_output"' EXIT
test_jdk=${JAVA_HOME:?Set JAVA_HOME to a JDK with javac and JNI headers}
case $(uname -s) in
    Darwin) jni_platform=darwin; library_name=libnative-lib.dylib ;;
    *) jni_platform=linux; library_name=libnative-lib.so ;;
esac
c++ -std=c++17 -Wall -Wextra -Werror -pthread -shared -fPIC \
    -I"$test_dir" -I"$native_dir" -I"$test_jdk/include" -I"$test_jdk/include/$jni_platform" \
    "$test_dir/JniLifecycleHooks.cpp" "$native_dir/native-lib.cpp" \
    "$native_dir/AudioRecorder.cpp" "$native_dir/AudioPlayer.cpp" \
    "$native_dir/AsyncAudioFile.cpp" "$native_dir"/filter/*.cpp \
    -o "$test_output/$library_name"
python3 "$test_dir/compile-engine.py" "$test_jdk" "$test_output"
"$test_jdk/bin/java" -ea -Xcheck:jni -Djava.library.path="$test_output" \
    -cp "$test_output/classes:$test_output/kotlin-stdlib.jar:$test_output/annotation.jar" \
    com.example.oboesample.AudioEngineLifecycleTest "$test_output"
