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

#include <velk/string.h>

namespace velk {

/** @brief Returns the directory containing the velk shared library (with trailing separator). */
inline string get_module_directory()
{
    string result;
#ifdef _WIN32
    HMODULE hm = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&get_module_directory),
                       &hm);
    if (hm) {
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(hm, buf, MAX_PATH);
        if (len > 0) {
            // Strip filename to get directory
            while (len > 0 && buf[len - 1] != '\\' && buf[len - 1] != '/') {
                --len;
            }
            result = string(buf, len);
        }
    }
#else
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&get_module_directory), &info) && info.dli_fname) {
        const char* path = info.dli_fname;
        const char* last_sep = nullptr;
        for (const char* p = path; *p; ++p) {
            if (*p == '/') {
                last_sep = p;
            }
        }
        if (last_sep) {
            result = string(path, static_cast<size_t>(last_sep - path + 1));
        }
    }
#endif
    return result;
}

} // namespace velk

#endif // VELK_PLATFORM_H
