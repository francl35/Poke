# The Vulkan backend is opt-in and required dependencies fail configuration explicitly.
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_program(MHP1_GLSLANG NAMES glslangValidator glslang REQUIRED)
set(shared "${PROJECT_SOURCE_DIR}/profiles/mhp2g")
set(shader_inc "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders/ge_shaders.inc")
add_custom_command(OUTPUT "${shader_inc}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders"
    COMMAND python3 "${shared}/tools/embed_shaders.py" "${MHP1_GLSLANG}" "${shader_inc}"
        "kGeVertexShader=${shared}/host/gpu/shaders/ge.vert"
        "kGeRawVertexShader=GE_RAW_VERTICES@${shared}/host/gpu/shaders/ge.vert"
        "kGeCheckVertexShader=GE_RAW_VERTICES,GE_CHECK_DECODE@${shared}/host/gpu/shaders/ge.vert"
        "kGeFragmentShader=${shared}/host/gpu/shaders/ge.frag"
        "kGeFragmentShaderPlain=GE_NO_SPECIALIZATION@${shared}/host/gpu/shaders/ge.frag"
    DEPENDS "${shared}/tools/embed_shaders.py" "${shared}/host/gpu/shaders/ge.vert" "${shared}/host/gpu/shaders/ge.frag")
target_sources(mhp1_boot_host PRIVATE "${shader_inc}")
foreach(source gpu/descriptor_pools.cpp gpu/device_report.cpp gpu/frame_interpolation.cpp
    gpu/frame_pacing.cpp gpu/replacement_textures.cpp gpu/mmpx.cpp gpu/ui_textures.cpp
    gpu/vulkan_renderer.cpp gpu/texture_decode.cpp gpu/texture_pack.cpp gpu/texture_pack_ktx.cpp
    gpu/texture_pack_import.cpp input/gamepad_devices.cpp input/gamepad_mapping.cpp
    perf/frame_stats.cpp perf/perf_overlay.cpp)
    target_sources(mhp1_boot_host PRIVATE "${shared}/host/${source}")
endforeach()
foreach(source imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp backends/imgui_impl_vulkan.cpp)
    target_sources(mhp1_boot_host PRIVATE "${shared}/third_party/imgui/${source}")
endforeach()
target_include_directories(mhp1_boot_host PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders"
    "${shared}/third_party" "${shared}/third_party/imgui")
target_compile_definitions(mhp1_boot_host PRIVATE MHP1_HAS_VULKAN=1 MHP2G_HAS_SDL_AUDIO=1)
target_link_libraries(mhp1_boot_host PRIVATE SDL3::SDL3 Vulkan::Vulkan ${CMAKE_DL_LIBS})
message(STATUS "mhp1: Vulkan renderer ENABLED (experimental; MHP2G game hooks disabled)")
target_sources(mhp1_boot_host PRIVATE
    "${shared}/host/ui/layer.cpp"
    "${shared}/host/ui/widgets.cpp"
    "${shared}/host/ui/text_input.cpp"
    "${shared}/third_party/imgui/backends/imgui_impl_sdl3.cpp"
)

target_compile_definitions(mhp1_boot_host PRIVATE
    MHP_UI_CORE_ONLY=1
    MHP_HAS_TEXT_INPUT=1
)
target_sources(mhp1_boot_host PRIVATE
    "${shared}/host/settings/settings.cpp"
    "${shared}/host/install/user_data.cpp"
)
target_sources(mhp1_boot_host PRIVATE
    "${shared}/host/ui/menu.cpp"
    "${shared}/host/ui/bindings_editor.cpp"
    "${shared}/host/ui/controllers_screen.cpp"
    "${shared}/host/gpu/screenshot.cpp"
)

set(MHP1_MENU_VERSION_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated_version/poke_version.hpp")

add_custom_target(mhp1_menu_version
    COMMAND ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DOUTPUT=${MHP1_MENU_VERSION_HEADER}"
        -P "${shared}/tools/write_version.cmake"
    BYPRODUCTS "${MHP1_MENU_VERSION_HEADER}"
)

add_dependencies(mhp1_boot_host mhp1_menu_version)

target_include_directories(mhp1_boot_host PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated_version"
)