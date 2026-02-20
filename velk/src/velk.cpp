#include "velk_instance.h"

#include <velk/velk_export.h>

namespace velk {

VELK_EXPORT IVelk &instance()
{
    // Global IVelk& instance
    static VelkInstance r;
    return *(r.get_interface<IVelk>());
}

// Control-block pool
//
// Each thread keeps a free-list of recycled control_blocks to avoid hitting
// the global allocator on every shared_ptr create/destroy. Pooled alloc is
// ~2.5x faster than new/delete in benchmarks.
//
// Use platform TLS APIs (FLS on Windows, pthread_key on POSIX) instead of
// C++ thread_local for two reasons:
//
//   1. DLL unload safety (Windows/Android): with thread_local, if the DLL is
//      unloaded via FreeLibrary while other threads are alive, those threads'
//      thread_local destructors point into unmapped code and crash on thread
//      exit. FLS avoids this: FlsFree invokes the cleanup callback for ALL
//      threads immediately, draining every pool before the DLL is unmapped.
//      On Android, Bionic has the same problem (no DSO refcounting for TLS).
//
//   2. Post-destruction access: during static destruction, a shared_ptr
//      release may access the pool after its thread_local destructor already
//      ran. MSVC silently re-constructs the thread_local but won't run its
//      destructor again, leaking the pool. With FLS/pthread_key, after the
//      key is freed get_pool_ptr() returns nullptr and alloc/dealloc fall
//      through to plain new/delete, which is correct.
//
// VELK_ENABLE_BLOCK_POOL is set by CMake (option VELK_ENABLE_BLOCK_POOL,
// default ON). Disable on toolchains where neither FLS nor pthreads is
// available (e.g. some bare-metal embedded targets).

#if VELK_ENABLE_BLOCK_POOL

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <pthread.h>
#endif

namespace {

// Limit our pool max size to 256 control blocks (4kB at 16B/block)
constexpr int32_t block_pool_max_size = 256;

struct block_pool
{
    control_block* head{nullptr};
    int32_t size{0};
};

void drain_pool(block_pool* pool)
{
    while (pool->head) {
        auto* next = static_cast<control_block*>(pool->head->ptr);
        delete pool->head;
        pool->head = next;
    }
    pool->size = 0;
}

#ifdef _WIN32

// Windows: FLS (Fiber-Local Storage) with cleanup callback.
// On thread/fiber exit, the callback fires for that thread's slot.
// On FlsFree (called from ~fls_init during static destruction or DLL unload),
// the callback fires for EVERY thread that has a value, draining all pools.

// Hot-path cache: a trivially-destructible thread_local raw pointer that
// skips the FlsGetValue API call after the first access. No destructor is
// registered, so DLL unload won't crash. Invalidated by fls_callback (on
// thread exit) and by ~fls_init setting g_fls_index to FLS_OUT_OF_INDEXES.
thread_local block_pool* t_cache = nullptr;

void WINAPI fls_callback(void* data)
{
    // Clear the cache on this thread before freeing the pool
    t_cache = nullptr;
    if (auto* pool = static_cast<block_pool*>(data)) {
        drain_pool(pool);
        delete pool;
    }
}

// Global slot index into each thread's FLS array. All threads share this
// index; the per-thread storage lives inside the OS, accessed via
// FlsGetValue/FlsSetValue.
DWORD g_fls_index = FLS_OUT_OF_INDEXES;

struct fls_init
{
    fls_init()
    {
        g_fls_index = FlsAlloc(fls_callback);
    }
    ~fls_init()
    {
        if (g_fls_index != FLS_OUT_OF_INDEXES) {
            FlsFree(g_fls_index);
            // Invalidate so any thread with a stale t_cache falls through
            // to the g_fls_index check and returns nullptr
            g_fls_index = FLS_OUT_OF_INDEXES;
        }
    }
};

// Static-init object that reserves the FLS slot on DLL load and frees it on
// DLL unload / static destruction. FlsFree invokes fls_callback for every
// thread that has a value, draining all pools before the slot is released.
static fls_init g_fls;

// Returns the calling thread's pool, creating it on first access.
// Uses a thread_local cache to avoid the FlsGetValue call on the hot path.
// Falls back to FLS on first access per thread, or returns nullptr after
// shutdown (g_fls_index == FLS_OUT_OF_INDEXES).
block_pool* get_pool_ptr()
{
    if (t_cache) {
        // We already have a cached block_pool* for this thread
        return t_cache;
    }

    if (g_fls_index == FLS_OUT_OF_INDEXES) {
        // FLS freed (shutdown) or never allocated
        return nullptr;
    }

    // First access on this thread: read from FLS
    auto* pool = static_cast<block_pool*>(FlsGetValue(g_fls_index));
    if (!pool) {
        // Not in FLS, create a new pool
        pool = new block_pool;
        // Store the pointer to FLS
        if (!FlsSetValue(g_fls_index, pool)) {
            delete pool;
            return nullptr;
        }
    }
    // Cache the pointer so that we don't need to access FLS for this thread at next access.
    t_cache = pool;
    return pool;
}

#else // POSIX

// POSIX: pthread_key with destructor callback.
// The callback fires on thread exit for each thread that set a value.
// Note: pthread_key_delete does NOT invoke callbacks for other threads,
// but glibc/Apple keep the DSO mapped while TLS references exist, so
// thread_local destruction crashes are not a concern on those platforms.
// On Android (Bionic), which lacks DSO refcounting, the pthread_key
// approach still works because the callback doesn't depend on DSO mapping.

// Hot-path cache (same idea as t_cache on Windows).
thread_local block_pool* t_cache = nullptr;

void pthread_key_callback(void* data)
{
    t_cache = nullptr;
    if (!data) return;
    auto* pool = static_cast<block_pool*>(data);
    drain_pool(pool);
    delete pool;
}

// Global key shared by all threads. Same role as g_fls_index on Windows.
pthread_key_t g_pool_key;
bool g_key_valid = false;

// Reserves the pthread key on load, releases it on unload.
// Unlike FlsFree, pthread_key_delete does NOT invoke the destructor for
// other threads, but setting g_key_valid = false ensures get_pool_ptr()
// returns nullptr after this point, falling through to new/delete.
struct key_init
{
    key_init()
    {
        if (pthread_key_create(&g_pool_key, pthread_key_callback) == 0)
            g_key_valid = true;
    }
    ~key_init()
    {
        if (g_key_valid) {
            pthread_key_delete(g_pool_key);
            g_key_valid = false;
        }
    }
};

static key_init g_key;

// Same pattern as the FLS version above.
block_pool* get_pool_ptr()
{
    if (t_cache) return t_cache;
    if (!g_key_valid) return nullptr;
    auto* pool = static_cast<block_pool*>(pthread_getspecific(g_pool_key));
    if (!pool) {
        pool = new block_pool;
        if (pthread_setspecific(g_pool_key, pool) != 0) {
            delete pool;
            return nullptr;
        }
    }
    t_cache = pool;
    return pool;
}

#endif // _WIN32

} // anonymous namespace

// alloc/dealloc fall through to plain new/delete when get_pool_ptr() returns
// nullptr (FLS/key freed, or allocation failure). This keeps shared_ptr
// functional during shutdown even after the pool infrastructure is torn down.

VELK_EXPORT control_block* detail::alloc_control_block(bool external)
{
    // External blocks (24 bytes) are not pooled; the pool only holds 16-byte
    // control_blocks. Both paths go through the DLL to avoid cross-module
    // heap mismatches.
    if (external) {
        return new external_control_block{{1, 1, nullptr}, nullptr};
    }
    auto* pool = get_pool_ptr();
    if (pool && pool->head) {
        auto* b = pool->head;
        pool->head = static_cast<control_block*>(b->ptr);
        --pool->size;
        b->strong.store(1, std::memory_order_relaxed);
        b->weak.store(1, std::memory_order_relaxed);
        b->ptr = nullptr;
        return b;
    }
    return new control_block{1, 1, nullptr};
}

VELK_EXPORT void detail::dealloc_control_block(control_block* block, bool external)
{
    if (external) {
        delete static_cast<external_control_block*>(block);
        return;
    }
    auto* pool = get_pool_ptr();
    if (!pool || pool->size >= block_pool_max_size) {
        delete block;
        return;
    }
    block->ptr = pool->head;
    pool->head = block;
    ++pool->size;
}

#else // !VELK_ENABLE_BLOCK_POOL

VELK_EXPORT control_block* detail::alloc_control_block(bool external)
{
    if (external) {
        return new external_control_block{{1, 1, nullptr}, nullptr};
    }
    return new control_block{1, 1, nullptr};
}

VELK_EXPORT void detail::dealloc_control_block(control_block* block, bool external)
{
    if (external) {
        delete static_cast<external_control_block*>(block);
    } else {
        delete block;
    }
}

#endif // VELK_ENABLE_BLOCK_POOL

} // namespace velk
