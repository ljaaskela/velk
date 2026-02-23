# Plugins

Velk supports plugins as a way to modularly register types and extend the runtime. Plugins can be loaded inline (in-process) or from shared libraries (.dll/.so) at runtime.

## Contents

- [Writing a plugin](#writing-a-plugin)
  - [Plugin metadata](#plugin-metadata)
  - [Versioning](#versioning)
  - [Dependencies](#dependencies)
  - [Versioned dependencies](#versioned-dependencies)
- [Plugin configuration](#plugin-configuration)
  - [Retaining types on unload](#retaining-types-on-unload)
  - [Update notifications](#update-notifications)
- [Loading plugins](#loading-plugins)
  - [Inline plugins](#inline-plugins)
  - [From a shared library](#from-a-shared-library)
- [Unloading plugins](#unloading-plugins)
- [Multi-plugin libraries](#multi-plugin-libraries)
- [Plugin info without instantiation](#plugin-info-without-instantiation)

## Writing a plugin

Derive from `ext::Plugin<T>` and implement `initialize` and `shutdown`:

```cpp
#include <velk/ext/plugin.h>

class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig& config) override
    {
        return register_type<MyWidget>(velk);
    }

    velk::ReturnValue shutdown(velk::IVelk& velk) override
    {
        return velk::ReturnValue::Success;
    }
};
```

`ext::Plugin<T>` inherits `ext::Object<T, IPlugin>`, so the plugin gets ref counting, a factory, and a class UID automatically.

### Plugin metadata

Plugins support optional static metadata via convenience macros:

```cpp
class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    // Optional: override automatically generated uid and name with fixed ones
    VELK_PLUGIN_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890");  // stable UID
    VELK_PLUGIN_NAME("My Plugin");                            // display name

    // ...
};
```

| Macro | Effect | Default |
|---|---|---|
| `VELK_PLUGIN_UID(str)` | Sets a stable class UID from a UUID string | Auto-generated from class name |
| `VELK_PLUGIN_NAME(str)` | Sets a human-readable display name | Class name |
| `VELK_PLUGIN_VERSION(major, minor, patch)` | Sets the plugin version | 0 (unversioned) |

All are optional. Without `VELK_PLUGIN_UID`, the UID is derived from the C++ class name via constexpr FNV-1a, which changes if the class is renamed. Use an explicit UID for plugins that need ABI stability across versions.

### Versioning

Declare a plugin version with `VELK_PLUGIN_VERSION`:

```cpp
class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    VELK_PLUGIN_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890");
    VELK_PLUGIN_VERSION(2, 1, 0);

    // ...
};
```

The version is packed into a `uint32_t` as `[major:8][minor:8][patch:16]`. Helper functions are provided for packing and unpacking:

```cpp
uint32_t v = velk::make_version(2, 1, 3);
velk::version_major(v);  // 2
velk::version_minor(v);  // 1
velk::version_patch(v);  // 3
```

The version is accessible through `PluginInfo::version` and `IPlugin::get_version()`.

### Dependencies

Declare dependencies on other plugins with `VELK_PLUGIN_DEPS`:

```cpp
class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    VELK_PLUGIN_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890");
    VELK_PLUGIN_DEPS(UiPlugin::class_uid, AudioPlugin::class_uid);

    // ...
};
```

Dependencies are validated at load time. If any dependency is not already loaded, `load_plugin` returns `Fail` and the plugin is not registered.

### Versioned dependencies

Dependencies can specify a minimum version. If the loaded dependency's version is lower, loading fails:

```cpp
class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    VELK_PLUGIN_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890");
    VELK_PLUGIN_DEPS(
        {UiPlugin::class_uid, velk::make_version(2, 0, 0)},   // requires UiPlugin >= 2.0.0
        AudioPlugin::class_uid                                  // any version
    );

    // ...
};
```

A dependency without a version (bare `Uid`) accepts any version.

## Plugin configuration

The `initialize` method receives a mutable `PluginConfig&` that the plugin can modify to opt into system behaviors. The system default-constructs the config before calling `initialize`, stores the result, and consults it during the plugin's lifetime.

```cpp
struct PluginConfig
{
    bool retainTypesOnUnload = false; ///< If true, plugin-owned types are kept after unload.
    bool enableUpdate = false;        ///< If true, update() is called during instance().update().
};
```

### Retaining types on unload

By default, when a plugin is unloaded all types it registered during `initialize` are automatically removed from the type registry. Setting `retainTypesOnUnload = true` keeps them registered:

```cpp
velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig& config) override
{
    config.retainTypesOnUnload = true;
    return register_type<MyWidget>(velk);
}
```

After unloading this plugin, `MyWidget` remains in the type registry and can still be created by UID.

### Update notifications

Plugins can receive periodic update notifications by setting `enableUpdate = true` and overriding `update()`:

```cpp
class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    VELK_PLUGIN_UID("...");

    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig& config) override
    {
        config.enableUpdate = true;
        return velk::ReturnValue::Success;
    }

    velk::ReturnValue shutdown(velk::IVelk&) override { return velk::ReturnValue::Success; }

    void update(const velk::UpdateInfo& info) override
    {
        // info.timeSinceInit        - time since the instance was created
        // info.timeSinceFirstUpdate - time since the first update() call
        // info.timeSinceLastUpdate  - time since the previous update() call
    }
};
```

`ext::Plugin<T>` provides a default no-op `update()`, so plugins that don't need it don't have to override it.

`instance().update()` accepts an optional `Duration` parameter representing the current time in microseconds. When omitted (or zero), the system uses `std::chrono::steady_clock`. When provided, all time deltas are computed from the caller-supplied values:

```cpp
velk::instance().update();                   // auto time from system clock
velk::instance().update({1'000'000});        // explicit: 1 second (microseconds)
```

## Loading plugins

### Inline plugins

Create a plugin instance in-process and load it directly:

```cpp
auto plugin = velk::ext::make_object<MyPlugin, velk::IPlugin>();
velk::instance().plugin_registry().load_plugin(plugin);
```

`load_plugin` calls the plugin's `initialize(IVelk&, PluginConfig&)` method, where it can register types, configure plugin behavior, and perform setup. If `initialize` returns a failure, the plugin is not stored.

### From a shared library

To load a plugin from a .dll or .so, the library must export a `velk_plugin_info` entry point. Use the `VELK_PLUGIN` macro at file scope:

```cpp
// my_plugin.cpp (compiled into my_plugin.dll / my_plugin.so)
#include <velk/ext/plugin.h>

class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    VELK_PLUGIN_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890");
    VELK_PLUGIN_NAME("My Plugin");

    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig&) override
    {
        return register_type<MyWidget>(velk);
    }

    velk::ReturnValue shutdown(velk::IVelk& velk) override
    {
        // Velk keeps track of types registered in initialize() and automatically unregisters them.
        return velk::ReturnValue::Success;
    }
};

VELK_PLUGIN(MyPlugin)
```

The host loads it at runtime:

```cpp
auto rv = velk::instance().plugin_registry().load_plugin_from_path("my_plugin.dll");
```

`load_plugin_from_path` performs the following steps:

1. Opens the shared library (`LoadLibraryA` / `dlopen`)
2. Resolves the `velk_plugin_info` symbol
3. Reads the `PluginInfo` descriptor (UID, name, dependencies, factory)
4. Checks for duplicate plugins and unmet dependencies
5. Creates a plugin instance via the embedded factory
6. Calls `load_plugin` which invokes `initialize`

If any step fails, the library is closed and an error is returned.

## Unloading plugins

Unload by UID or by class type:

```cpp
auto& reg = velk::instance().plugin_registry();

reg.unload_plugin(MyPlugin::class_id());
// or
reg.unload_plugin<MyPlugin>();
```

Unloading calls the plugin's `shutdown(IVelk&)` method, then removes all types that the plugin registered from the type registry (unless the plugin set `retainTypesOnUnload = true` in its config). For library-loaded plugins, the shared library handle is closed after the plugin is released.

## Multi-plugin libraries

A single shared library can contain multiple plugins. One plugin acts as the host and exports the `VELK_PLUGIN` entry point. The host plugin loads additional plugins from the same library during `initialize` and unloads them during `shutdown`:

```cpp
class SubPlugin : public velk::ext::Plugin<SubPlugin>
{
public:
    VELK_PLUGIN_UID("...");

    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig&) override { /* ... */ }
    velk::ReturnValue shutdown(velk::IVelk& velk) override { return velk::ReturnValue::Success; }
};

class HostPlugin : public velk::ext::Plugin<HostPlugin>
{
public:
    VELK_PLUGIN_UID("...");
    VELK_PLUGIN_NAME("My Bundle");

    velk::ReturnValue initialize(velk::IVelk& velk, velk::PluginConfig&) override
    {
        auto rv = register_type<MyWidget>(velk);
        if (failed(rv)) {
            VELK_LOG(E, "Type registration failed [%s]", get_class_name());)
        }
        // Isntantiate SubPlugin
        auto pluginInstance = velk::ext::make_object<SubPlugin, velk::IPlugin>();
        // Load it to Velk
        return velk.plugin_registry().load_plugin(pluginInstance);
    }

    velk::ReturnValue shutdown(velk::IVelk& velk) override
    {
        velk.plugin_registry().unload_plugin(SubPlugin::class_id());
        return velk::ReturnValue::Success;
    }
};

VELK_PLUGIN(HostPlugin)
```

The host plugin's `shutdown` must unload sub-plugins before the library handle is closed.

## Plugin info without instantiation

`PluginInfo` is accessible as a static descriptor without creating a plugin instance. This allows inspecting a plugin's UID, name, version, and dependencies before loading:

```cpp
// From C++ (compile-time access)
const auto& info = MyPlugin::plugin_info();
auto uid     = info.uid();
auto name    = info.name;
auto version = info.version;
auto deps    = info.dependencies;
```

For shared libraries, `velk_plugin_info` returns a `const PluginInfo*`, so the host can inspect metadata before deciding whether to instantiate the plugin.
