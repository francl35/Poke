# Optional main AOT corpus; the runtime opt-in remains MHP1_NATIVE_MAIN=1.
option(MHP1_BUILD_NATIVE_MAIN "Link optimized MHP1 main AOT into the boot executable" OFF)

if(TARGET mhp1_boot_host)
    target_sources(mhp1_boot_host PRIVATE host/main_native.cpp)
endif()

if(MHP1_BUILD_NATIVE_MAIN)
    if(NOT TARGET PokeFreedomDXBoot)
        message(FATAL_ERROR "MHP1_BUILD_NATIVE_MAIN requires MHP1_BUILD_BOOT=ON")
    endif()
    if(PSPRECOMP_COVERAGE)
        message(FATAL_ERROR "Private MHP1 generated code must not enter coverage builds")
    endif()
    if(NOT TARGET mhp1_generated)
        file(GLOB main_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.cpp")
        if(NOT main_sources)
            message(FATAL_ERROR "Generate profiles/mhp1/generated before enabling main AOT")
        endif()
        add_library(mhp1_generated OBJECT ${main_sources})
        target_include_directories(mhp1_generated PRIVATE "${PROJECT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/generated")
        set_target_properties(mhp1_generated PROPERTIES JOB_POOL_COMPILE psprecomp_generated)
    endif()
    # Appended options also override the probe's earlier -O0 or /Od options.
    if(MSVC)
        target_compile_options(mhp1_generated PRIVATE /O2 /bigobj)
    else()
        target_compile_options(mhp1_generated PRIVATE -O2 -g0)
    endif()
    target_compile_definitions(mhp1_boot_host PRIVATE MHP1_HAS_MAIN_AOT=1)
    target_sources(PokeFreedomDXBoot PRIVATE $<TARGET_OBJECTS:mhp1_generated>)
    if(TARGET mhp1_bootstrap_tests)
        target_sources(mhp1_bootstrap_tests PRIVATE
            $<TARGET_OBJECTS:mhp1_generated>)
    endif()
    # Public synthetic regression; builds only when requested explicitly.
    add_executable(mhp1_main_native_tests EXCLUDE_FROM_ALL
        tests/main_native_tests.cpp host/main_native.cpp)
    target_include_directories(mhp1_main_native_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/host")
    target_compile_definitions(mhp1_main_native_tests PRIVATE MHP1_HAS_MAIN_AOT=1)
    target_link_libraries(mhp1_main_native_tests PRIVATE psprecomp_core)
    set_target_properties(mhp1_main_native_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# MHP1_FDX_DYNAMIC_LHU_CMAKE
# Reconfigure if regeneration removes the adapted handlers; validation then
# returns to conservative retirement instead of accepting stale native code.
if(MHP1_BUILD_NATIVE_MAIN)
    set(mhp1_dynamic_source "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_0006.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${mhp1_dynamic_source}")
    file(READ "${mhp1_dynamic_source}" mhp1_dynamic_text)
    string(FIND "${mhp1_dynamic_text}" "MHP1_FDX_DYNAMIC_LHU_0881D558" mhp1_dynamic_first)
    string(FIND "${mhp1_dynamic_text}" "MHP1_FDX_DYNAMIC_LHU_0881D560" mhp1_dynamic_second)
    if(NOT mhp1_dynamic_first EQUAL -1 AND NOT mhp1_dynamic_second EQUAL -1)
        set_property(SOURCE host/main_native.cpp APPEND PROPERTY
            COMPILE_DEFINITIONS MHP1_FDX_DYNAMIC_LHU=1)
        message(STATUS "mhp1: both traced FreedomDX instruction variants supported natively")
    endif()
endif()
