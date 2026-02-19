/**
 * @file velk_export.h
 * @brief DLL export/import macros for the Velk shared library.
 *
 * Defines VELK_EXPORT to __declspec(dllexport) when building the DLL
 * (VELK_EXPORTS defined) and __declspec(dllimport) when consuming it.
 * On non-MSVC platforms, uses __attribute__((visibility("default"))).
 * Define VELK_STATIC_DEFINE to build as a static library (no export attributes).
 */
#ifndef VELK_EXPORT_H
#define VELK_EXPORT_H

#ifdef VELK_STATIC_DEFINE
#define VELK_EXPORT
#define VELK_NO_EXPORT
#else
#ifndef VELK_EXPORT
#ifdef VELK_EXPORTS
/* We are building this library */
#  ifdef _MSC_VER
#    define VELK_EXPORT __declspec(dllexport)
#  else
#    define VELK_EXPORT __attribute__((visibility("default")))
#  endif
#    else
        /* We are using this library */
#  ifdef _MSC_VER
#    define VELK_EXPORT __declspec(dllimport)
#  else
#    define VELK_EXPORT __attribute__((visibility("default")))
#  endif
#    endif
#  endif

#ifndef VELK_NO_EXPORT
#define VELK_NO_EXPORT
#  endif
#endif

#ifndef VELK_DEPRECATED
#define VELK_DEPRECATED __attribute__((__deprecated__))
#endif

#ifndef VELK_DEPRECATED_EXPORT
#define VELK_DEPRECATED_EXPORT VELK_EXPORT VELK_DEPRECATED
#endif

#ifndef VELK_DEPRECATED_NO_EXPORT
#define VELK_DEPRECATED_NO_EXPORT VELK_NO_EXPORT VELK_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#ifndef VELK_NO_DEPRECATED
#define VELK_NO_DEPRECATED
#  endif
#endif

#endif /* VELK_EXPORT_H */
