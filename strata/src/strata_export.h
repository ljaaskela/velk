
#ifndef STRATA_EXPORT_H
#define STRATA_EXPORT_H

#ifdef STRATA_STATIC_DEFINE
#define STRATA_EXPORT
#define STRATA_NO_EXPORT
#else
#ifndef STRATA_EXPORT
#ifdef STRATA_EXPORTS
/* We are building this library */
#  ifdef _MSC_VER
#    define STRATA_EXPORT __declspec(dllexport)
#  else
#    define STRATA_EXPORT __attribute__((visibility("default")))
#  endif
#    else
        /* We are using this library */
#  ifdef _MSC_VER
#    define STRATA_EXPORT __declspec(dllimport)
#  else
#    define STRATA_EXPORT __attribute__((visibility("default")))
#  endif
#    endif
#  endif

#ifndef STRATA_NO_EXPORT
#define STRATA_NO_EXPORT
#  endif
#endif

#ifndef STRATA_DEPRECATED
#define STRATA_DEPRECATED __attribute__((__deprecated__))
#endif

#ifndef STRATA_DEPRECATED_EXPORT
#define STRATA_DEPRECATED_EXPORT STRATA_EXPORT STRATA_DEPRECATED
#endif

#ifndef STRATA_DEPRECATED_NO_EXPORT
#define STRATA_DEPRECATED_NO_EXPORT STRATA_NO_EXPORT STRATA_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#ifndef STRATA_NO_DEPRECATED
#define STRATA_NO_DEPRECATED
#  endif
#endif

#endif /* STRATA_EXPORT_H */
