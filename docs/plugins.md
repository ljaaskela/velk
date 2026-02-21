# Plugins

Velk supports plugins as a way to modularly register types and extend the runtime. Plugins can be loaded inline (in-process) or from shared libraries (.dll/.so) at runtime.

## Contents

- [Writing a plugin](#writing-a-plugin)
  - [Plugin metadata](#plugin-metadata)
  - [Dependencies](#dependencies)
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
    velk::ReturnValue initialize(velk::IVelk& velk) override
    {
        return register_type<MyWidget>(velk);
    }

    velk::ReturnValue shutdown(velk::IVelk& velk) override
    {
        return velk::ReturnValue::SUCCESS;
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
    VELK_PLUGIN_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890");  // stable UID
    VELK_PLUGIN_NAME("My Plugin");                               // display name

    // ...
};
```

| Macro | Effect | Default |
|---|---|---|
| `VELK_PLUGIN_UID(str)` | Sets a stable class UID from a UUID string | Auto-generated from class name |
| `VELK_PLUGIN_NAME(str)` | Sets a human-readable display name | Class name |

Both are optional. Without `VELK_PLUGIN_UID`, the UID is derived from the C++ class name via constexpr FNV-1a, which changes if the class is renamed. Use an explicit UID for plugins that need ABI stability across versions.

### Dependencies

Declare dependencies on other plugins with `VELK_PLUGIN_DEPS`:

```cpp
class MyPlugin : public velk::ext::Plugin<MyPlugin>
{
public:
    VELK_PLUGIN_UID("a1b2c3d4-e5f6-7890-abcd-ef1234567890");
    VELK_PLUGIN_DEPS(CorePlugin::class_uid, AudioPlugin::class_uid);

    // ...
};
```

Dependencies are validated at load time. If any dependency is not already loaded, `load_plugin` returns `FAIL` and the plugin is not registered.

## Loading plugins

### Inline plugins

Create a plugin instance in-process and load it directly:

```cpp
auto plugin = velk::ext::make_object<MyPlugin, velk::IPlugin>();
velk::instance().plugin_registry().load_plugin(plugin);
```

`load_plugin` calls the plugin's `initialize(IVelk&)` method, where it can register types and perform setup. If `initialize` returns a failure, the plugin is not stored.

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

    velk::ReturnValue initialize(velk::IVelk& velk) override
    {
        return register_type<MyWidget>(velk);
    }

    velk::ReturnValue shutdown(velk::IVelk& velk) override
    {
        return velk::ReturnValue::SUCCESS;
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

Unloading calls the plugin's `shutdown(IVelk&)` method, then sweeps all types that the plugin registered from the type registry. For library-loaded plugins, the shared library handle is closed after the plugin is released.

## Multi-plugin libraries

A single shared library can contain multiple plugins. One plugin acts as the host and exports the `VELK_PLUGIN` entry point. The host plugin loads additional plugins from the same library during `initialize` and unloads them during `shutdown`:

```cpp
class SubPlugin : public velk::ext::Plugin<SubPlugin>
{
public:
    VELK_PLUGIN_UID("...");

    velk::ReturnValue initialize(velk::IVelk& velk) override { /* ... */ }
    velk::ReturnValue shutdown(velk::IVelk& velk) override { return velk::ReturnValue::SUCCESS; }
};

class HostPlugin : public velk::ext::Plugin<HostPlugin>
{
public:
    VELK_PLUGIN_UID("...");
    VELK_PLUGIN_NAME("My Bundle");

    velk::ReturnValue initialize(velk::IVelk& velk) override
    {
        auto rv = register_type<MyWidget>(velk);
        return failed(rv) ?
                    rv :
                    velk.plugin_registry().load_plugin(velk::ext::make_object<SubPlugin, velk::IPlugin>());
    }

    velk::ReturnValue shutdown(velk::IVelk& velk) override
    {
        velk.plugin_registry().unload_plugin(SubPlugin::class_id());
        return velk::ReturnValue::SUCCESS;
    }
};

VELK_PLUGIN(HostPlugin)
```

The host plugin's `shutdown` must unload sub-plugins before the library handle is closed.

## Plugin info without instantiation

`PluginInfo` is accessible as a static descriptor without creating a plugin instance. This allows inspecting a plugin's UID, name, and dependencies before loading:

```cpp
// From C++ (compile-time access)
const auto& info = MyPlugin::plugin_info();
auto uid = info.uid();
auto name = info.name;
auto deps = info.dependencies;
```

For shared libraries, `velk_plugin_info` returns a `const PluginInfo*`, so the host can inspect metadata before deciding whether to instantiate the plugin.
