# First MHP1/FreedomDX milestone: build/link and inspect, without guest execution.
option(MHP1_BUILD_PROBE "Build the MHP1 executable and generated-registry probe" OFF)

if(MHP1_BUILD_PROBE)
    if(PSPRECOMP_COVERAGE)
        message(FATAL_ERROR "The MHP1 probe requires a private game-derived corpus; disable coverage")
    endif()
    file(GLOB MHP1_GENERATED CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.cpp")
    if(NOT MHP1_GENERATED)
        message(FATAL_ERROR "Generate profiles/mhp1/generated first, or configure with MHP1_BUILD_PROBE=OFF")
    endif()
    add_library(mhp1_generated OBJECT ${MHP1_GENERATED})
    target_include_directories(mhp1_generated PRIVATE "${PROJECT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated")
    set_target_properties(mhp1_generated PROPERTIES JOB_POOL_COMPILE psprecomp_generated)
    if(MSVC)
        target_compile_options(mhp1_generated PRIVATE /Od /bigobj)
    else()
        target_compile_options(mhp1_generated PRIVATE -O0 -g0)
    endif()
    add_executable(PokeFreedomDXProbe host/main.cpp $<TARGET_OBJECTS:mhp1_generated>)
    target_link_libraries(PokeFreedomDXProbe PRIVATE psprecomp_core)
    set_target_properties(PokeFreedomDXProbe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(PSPRECOMP_BUILD_TESTS AND PSPRECOMP_BUILD_PROFILE_TESTS)
    add_executable(mhp1_target_tests tests/target_tests.cpp)
    target_include_directories(mhp1_target_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/host")
    target_link_libraries(mhp1_target_tests PRIVATE psprecomp_core)
    add_test(NAME mhp1_target_tests COMMAND mhp1_target_tests)
    set_tests_properties(mhp1_target_tests PROPERTIES TIMEOUT 30)
endif()

option(MHP1_CODECS "Enable FFmpeg ATRAC music and MPEG movies" OFF)
option(MHP1_VULKAN "Enable the existing SDL3/Vulkan renderer for MHP1" OFF)
option(MHP1_BUILD_BOOT "Build the bounded headless MHP1 interpreter bootstrap" OFF)
if(MHP1_BUILD_BOOT OR (PSPRECOMP_BUILD_TESTS AND PSPRECOMP_BUILD_PROFILE_TESTS))
    # Reuse PSP kernel/I/O services and the headless GE command decoder.
    # No MHP2G executable bootstrap,
    # game address hooks, overlay loader, mods, Vulkan frontend or persistent settings.
    set(MHP1_SHARED_HOST "${PROJECT_SOURCE_DIR}/profiles/mhp2g/host")
    set(MHP1_NIDS_INC "${CMAKE_CURRENT_BINARY_DIR}/generated_nids/nid_table.inc")
    add_custom_command(OUTPUT "${MHP1_NIDS_INC}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${PROJECT_SOURCE_DIR}/configs/nids.csv
            -DOUTPUT=${MHP1_NIDS_INC}
            -P "${PROJECT_SOURCE_DIR}/profiles/mhp2g/tools/embed_nids.cmake"
        DEPENDS "${PROJECT_SOURCE_DIR}/configs/nids.csv"
            "${PROJECT_SOURCE_DIR}/profiles/mhp2g/tools/embed_nids.cmake")
    add_library(mhp1_boot_host STATIC
        host/boot_host.cpp host/video_backend.cpp host/controller.cpp host/audio_services.cpp host/platform_services.cpp host/stock_modules.cpp host/headless_ge.cpp host/software_video.cpp host/diagnostic_adapters.cpp "${MHP1_NIDS_INC}"
        "${MHP1_SHARED_HOST}/gpu/ge_state.cpp"
        "${MHP1_SHARED_HOST}/audio/audio_sink.cpp"
        "${MHP1_SHARED_HOST}/audio/atrac_decoder.cpp"
        "${MHP1_SHARED_HOST}/movie/avc_decoder.cpp"
        "${MHP1_SHARED_HOST}/movie/psmf_demuxer.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_atrac.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_mpeg.cpp"
        "${MHP1_SHARED_HOST}/audio/guest_pcm.cpp"
        "${MHP1_SHARED_HOST}/audio/sas_core.cpp"
        "${MHP1_SHARED_HOST}/fonts/game_font.cpp"
        "${MHP1_SHARED_HOST}/app_paths.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_font.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_savedata.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_utility.cpp"
        "${MHP1_SHARED_HOST}/save_data/savedata_store.cpp"
        "${MHP1_SHARED_HOST}/save_data/savedata_crypto.cpp"
        "${MHP1_SHARED_HOST}/save_data/save_transfer.cpp"
        "${MHP1_SHARED_HOST}/save_data/param_sfo.cpp"
        "${MHP1_SHARED_HOST}/save_data/aes128.cpp"
        "${MHP1_SHARED_HOST}/kernel/kernel.cpp"
        "${MHP1_SHARED_HOST}/kernel/load_trace.cpp"
        "${MHP1_SHARED_HOST}/kernel/iso_image.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_common.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_threadman.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_sysmem.cpp"
        "${MHP1_SHARED_HOST}/hle/hle_io.cpp"
        "${MHP1_SHARED_HOST}/platform/utf8_path.cpp"
        "${MHP1_SHARED_HOST}/input/bindings.cpp"
        "${MHP1_SHARED_HOST}/input/chords.cpp"
        "${MHP1_SHARED_HOST}/input/presets.cpp"
        "${MHP1_SHARED_HOST}/input/touch_action.cpp"
        "${MHP1_SHARED_HOST}/input/touch_controls.cpp")
    target_include_directories(mhp1_boot_host PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/host"
        PRIVATE "${MHP1_SHARED_HOST}" "${CMAKE_CURRENT_BINARY_DIR}/generated_nids")
    set_source_files_properties("${PROJECT_SOURCE_DIR}/profiles/mhp2g/third_party/tiny_aes/aes.c" PROPERTIES LANGUAGE CXX)
    target_sources(mhp1_boot_host PRIVATE "${PROJECT_SOURCE_DIR}/profiles/mhp2g/third_party/tiny_aes/aes.c")
    target_include_directories(mhp1_boot_host PRIVATE "${PROJECT_SOURCE_DIR}/profiles/mhp2g/third_party")
    if(MHP1_CODECS)
        # Match the original profile: pinned bundled codecs by default, not
        # whichever architecture pkg-config happens to find on the host.
        include("${PROJECT_SOURCE_DIR}/profiles/mhp2g/cmake/FFmpeg.cmake")
        if(NOT MHP2G_HAS_FFMPEG)
            message(FATAL_ERROR "MHP1_CODECS=ON requires MHP2G_FFMPEG=bundled or system")
        endif()
        mhp2g_use_ffmpeg(mhp1_boot_host)
        message(STATUS "mhp1: FFmpeg services enabled using the shared ${MHP2G_FFMPEG} build")
    endif()
    if(MHP1_VULKAN)
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake_vulkan.cmake")
    endif()
    target_link_libraries(mhp1_boot_host PUBLIC psprecomp_core)
    find_package(Threads REQUIRED)
    target_link_libraries(mhp1_boot_host PRIVATE Threads::Threads)
    if(WIN32)
        target_link_libraries(mhp1_boot_host PRIVATE shell32)
    endif()
endif()

if(MHP1_BUILD_BOOT)
    add_executable(PokeFreedomDXBoot host/boot_main.cpp)
    target_include_directories(PokeFreedomDXBoot PRIVATE "${MHP1_SHARED_HOST}")
    target_link_libraries(PokeFreedomDXBoot PRIVATE mhp1_boot_host)
    set_target_properties(PokeFreedomDXBoot PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(PSPRECOMP_BUILD_TESTS AND PSPRECOMP_BUILD_PROFILE_TESTS)
    add_executable(mhp1_bootstrap_tests tests/bootstrap_tests.cpp)
    target_include_directories(mhp1_bootstrap_tests PRIVATE "${MHP1_SHARED_HOST}")
    target_link_libraries(mhp1_bootstrap_tests PRIVATE mhp1_boot_host)
    add_test(NAME mhp1_bootstrap_tests COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:mhp1_bootstrap_tests>
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_bootstrap_tests.cmake")
    set_tests_properties(mhp1_bootstrap_tests PROPERTIES TIMEOUT 30)
endif()

if(PSPRECOMP_BUILD_TESTS AND PSPRECOMP_BUILD_PROFILE_TESTS)
    add_executable(mhp1_video_tests tests/video_tests.cpp)
    target_include_directories(mhp1_video_tests PRIVATE "${MHP1_SHARED_HOST}")
    target_link_libraries(mhp1_video_tests PRIVATE mhp1_boot_host)
    add_test(NAME mhp1_video_tests COMMAND mhp1_video_tests)
    set_tests_properties(mhp1_video_tests PROPERTIES TIMEOUT 30)
endif()

if(MHP1_VULKAN)
    add_executable(mhp1_vulkan_smoke tests/vulkan_smoke.cpp)
    target_include_directories(mhp1_vulkan_smoke PRIVATE "${MHP1_SHARED_HOST}")
    target_link_libraries(mhp1_vulkan_smoke PRIVATE mhp1_boot_host)
    set_target_properties(mhp1_vulkan_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(MHP1_BUILD_BOOT)
    add_executable(mhp1_import_audit tests/import_audit.cpp)
    target_link_libraries(mhp1_import_audit PRIVATE mhp1_boot_host)
    set_target_properties(mhp1_import_audit PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(PSPRECOMP_BUILD_TESTS AND PSPRECOMP_BUILD_PROFILE_TESTS)
    add_executable(mhp1_services_tests tests/services_tests.cpp)
    target_include_directories(mhp1_services_tests PRIVATE "${MHP1_SHARED_HOST}")
    target_link_libraries(mhp1_services_tests PRIVATE mhp1_boot_host)
    add_test(NAME mhp1_services_tests COMMAND mhp1_services_tests)
    set_tests_properties(mhp1_services_tests PROPERTIES TIMEOUT 30)
    add_executable(mhp1_savedata_tests "${PROJECT_SOURCE_DIR}/profiles/mhp2g/tests/savedata_tests.cpp")
    target_include_directories(mhp1_savedata_tests PRIVATE "${MHP1_SHARED_HOST}" "${PROJECT_SOURCE_DIR}/profiles/mhp2g/third_party")
    target_link_libraries(mhp1_savedata_tests PRIVATE mhp1_boot_host)
    add_test(NAME mhp1_savedata_tests COMMAND mhp1_savedata_tests)
    set_tests_properties(mhp1_savedata_tests PROPERTIES TIMEOUT 30)
endif()

# The shared helper also supplies executable-relative runtime search paths.
# Apply it to final binaries as well as to the static host library.
if(MHP1_CODECS)
    foreach(codec_target PokeFreedomDXBoot mhp1_import_audit mhp1_vulkan_smoke
        mhp1_bootstrap_tests mhp1_video_tests mhp1_services_tests mhp1_savedata_tests)
        if(TARGET ${codec_target})
            mhp2g_use_ffmpeg(${codec_target})
        endif()
    endforeach()
endif()
if(TARGET PokeFreedomDXBoot)
    target_sources(mhp1_boot_host PRIVATE host/overlays.cpp)
    target_link_libraries(mhp1_boot_host PUBLIC ${CMAKE_DL_LIBS})
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake_overlays.cmake")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake_main_native.cmake")
