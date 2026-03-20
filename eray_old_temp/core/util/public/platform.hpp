#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define IS_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
#define IS_MACOS
#define IS_UNIX
#elif defined(__unix__) || defined(__unix)
#define IS_UNIX
#if defined(__linux__)
#define IS_LINUX
#endif
#endif

#ifndef NDEBUG
#define IS_DEBUG
#endif
