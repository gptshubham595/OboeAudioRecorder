cmake_minimum_required(VERSION 3.22.1)

# Set the project name
project("oboesample")

# Build your native shared library
add_library(
        native-lib
        SHARED
        native-lib.cpp
        AudioRecorder.cpp
        AudioPlayer.cpp
        ${CMAKE_SOURCE_DIR}/filter/BiquadFilter.cpp
        ${CMAKE_SOURCE_DIR}/filter/NoiseGate.cpp
        ${CMAKE_SOURCE_DIR}/filter/NoiseReduction.cpp
        ${CMAKE_SOURCE_DIR}/filter/EchoCanceller.cpp
)

# Include headers
include_directories(
        ${CMAKE_SOURCE_DIR}/oboe/include
        ${CMAKE_SOURCE_DIR/filter}
        ${CMAKE_SOURCE_DIR}
)

# Fix: Apply linker options to the correct target: native-lib
target_link_options(native-lib PRIVATE "-Wl,-z,max-page-size=16384")


# Find the Oboe package
find_package(oboe REQUIRED CONFIG)

# Specify the libraries which our native library is dependent on, including Oboe
target_link_libraries(native-lib log oboe::oboe)