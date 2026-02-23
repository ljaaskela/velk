#ifndef VELK_LIBRARY_HANDLE_H
#define VELK_LIBRARY_HANDLE_H

#include "platform.h" // IWYU pragma: keep (platform-specific defines)

#include <velk/common.h>

namespace velk {

/**
 * @brief Platform-abstracted shared library handle.
 *
 * Wraps LoadLibrary/dlopen for loading shared libraries at runtime.
 * Move-only; closing is explicit via close().
 */
class LibraryHandle : NoCopy
{
public:
    LibraryHandle() = default;
    ~LibraryHandle() = default;

    LibraryHandle(LibraryHandle&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }
    LibraryHandle& operator=(LibraryHandle&& o) noexcept
    {
        if (this != &o) {
            handle_ = o.handle_;
            o.handle_ = nullptr;
        }
        return *this;
    }

    /** @brief Opens a shared library from the given path. */
    static LibraryHandle open(const char* path)
    {
        LibraryHandle h;
#ifdef _WIN32
        h.handle_ = static_cast<void*>(LoadLibraryA(path));
#else
        h.handle_ = dlopen(path, RTLD_NOW);
#endif
        return h;
    }

    /** @brief Closes the library handle if open. */
    void close()
    {
        if (handle_) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle_));
#else
            dlclose(handle_);
#endif
            handle_ = nullptr;
        }
    }

    /** @brief Resolves a symbol by name from the loaded library. */
    void* symbol(const char* name) const
    {
        if (!handle_) {
            return nullptr;
        }
#ifdef _WIN32
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
        return dlsym(handle_, name);
#endif
    }

    explicit operator bool() const { return handle_ != nullptr; }

private:
    void* handle_ = nullptr;
};

} // namespace velk

#endif // VELK_LIBRARY_HANDLE_H
