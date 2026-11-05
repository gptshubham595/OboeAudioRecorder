#pragma once
// Host-side test substitute; production builds use the NDK logging header.
#define ANDROID_LOG_DEBUG 3
#define ANDROID_LOG_ERROR 6
inline int __android_log_print(int, const char*, const char*, ...) { return 0; }
