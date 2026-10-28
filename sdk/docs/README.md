# KH Mod SDK

The headers and build files for native plugins of the khdays port's mods (docs/MODDING_NATIVE.md in the port's
repository says how players turn them on and how a mod carries one).

## Making a plugin

1. A CMake project that includes `cmake/KhModSdk.cmake` (or adds this folder) and calls
   `kh_add_plugin(my_plugin my_plugin.cpp)`.
2. In the source, a descriptor and the one exported function:

```cpp
#include <kh/kh_mod.h>

namespace {
const KhHost *host = nullptr;

KhResult start(const KhHost *given)
{
    host = given;
    host->log(host->plugin, KH_LOG_INFO, "started");
    return KH_OK;
}

void stop()
{
    host->log(host->plugin, KH_LOG_INFO, "stopped");
}

const KhPluginDescriptor descriptor = KH_PLUGIN_DESCRIPTOR("My plugin", start, stop);
} // namespace

extern "C" KH_PLUGIN_EXPORT const KhPluginDescriptor *kh_plugin_query(void)
{
    return &descriptor;
}
```

3. Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`.
4. Install: the library in `mods/<Mod>/native/<platform>/`, and `mod.ini` with `sdk = 1` and
   `native.<platform> = native/<platform>/my_plugin.dll`.

`examples/HelloWorld` is the same, complete.

## The rules of the boundary (`include/kh/kh_mod.h`)

- Only C crosses it: fixed-size integers, UTF-8 strings, opaque handles, structs of function pointers. Catch every
  C++ exception inside the plugin; pass no standard-library type; use no RTTI across it.
- Version: `KH_ABI_MAJOR` / `KH_ABI_MINOR`. The port loads a plugin built for its own major and a minor not above
  its own. Structs given by the port begin with their size and only grow at their end: read a field only when the
  size covers it.
- Interfaces: `host->find(host->plugin, "<name>", <version>)` returns a struct of functions or NULL when the port
  has not got it. A port keeps every version of an interface it ever gave.
- Threads: `start`, `stop` and callbacks run on the game's thread unless an interface says otherwise.
- After `stop` returns the port calls nothing of the plugin and releases its library: undo nothing yourself that
  the port undoes (it drops your registrations), free what you allocated.
- A plugin is not isolated from the game: a fault in it is a fault of the game.

## Platforms

| Platform | Library | State |
|---|---|---|
| Windows x64 | `.dll` (MSVC or clang-cl; the C/C++ runtime linked in by `kh_add_plugin`) | built and run |
| Linux x64, arm64 (Steam Deck) | `.so` (gcc or clang) | the build files support it; untested until the port runs on Linux |
| Platforms without dynamic loading | the same source registered at the port's build | planned |
