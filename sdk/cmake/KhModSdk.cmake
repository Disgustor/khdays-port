# The KH Mod SDK for CMake: include() it (or add the SDK with add_subdirectory) and make a plugin with
#   kh_add_plugin(<target> <sources>...)
# A plugin is a shared library named <target>.dll on Windows and <target>.so on Linux (no "lib" prefix), built
# against the SDK's headers, exporting only kh_plugin_query (everything else hidden). On Windows the C/C++ runtime
# goes inside the plugin, so players need nothing installed for it.
set(KH_MOD_SDK_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/../include")

function(kh_add_plugin target)
    add_library(${target} MODULE ${ARGN})
    target_include_directories(${target} PRIVATE "${KH_MOD_SDK_INCLUDE}")
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /EHsc /utf-8)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()
