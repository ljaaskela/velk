#ifndef VELK_PLATFORM_H
#define VELK_PLATFORM_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#endif

#endif // VELK_PLATFORM_H
