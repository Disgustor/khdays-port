# The KH Mod SDK: the headers a mod's native plugin is built against (include/kh/) and its examples. Built alone:
#   cmake -S sdk -B build/sdk && cmake --build build/sdk --config Release
# docs/README.md says how to make and install a plugin.
cmake_minimum_required(VERSION 3.20)
project(KhModSdk LANGUAGES C CXX)

include(cmake/KhModSdk.cmake)

add_subdirectory(examples/HelloWorld)
add_subdirectory(examples/AdvancedNativeMod)
