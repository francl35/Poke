
# Reuse the existing overlay ABI and module template.
set(MHP1_OVERLAY_SHARED_PROFILE_DIR "${MHP1_SHARED_PROFILE_DIR}")
set(MHP1_OVERLAY_DIR "${CMAKE_BINARY_DIR}/bin/overlays")

if(NOT DEFINED PSPRECOMP_GENERATED_OPT_LEVEL
   OR PSPRECOMP_GENERATED_OPT_LEVEL STREQUAL "")
    set(PSPRECOMP_GENERATED_OPT_LEVEL "2")
endif()

# Overlay modules resolve runtime symbols from this executable.
set_target_properties(PokeFreedomDXBoot PROPERTIES ENABLE_EXPORTS ON)

function(mhp1_add_overlay meta_path)
    get_filename_component(source_dir "${meta_path}" DIRECTORY)
    get_filename_component(MHP2G_OVERLAY_PREFIX "${source_dir}" NAME)
    file(STRINGS "${meta_path}" meta_lines REGEX "^(base|size|code_size|hash|name)=")
    foreach(line IN LISTS meta_lines)
        string(REGEX MATCH "^([a-z_]+)=(.+)$" matched "${line}")
        if(NOT matched)
            continue()
        endif()
        string(TOUPPER "${CMAKE_MATCH_1}" key)
        set(MHP2G_OVERLAY_${key} "${CMAKE_MATCH_2}")
    endforeach()
    if(NOT MHP2G_OVERLAY_BASE OR NOT MHP2G_OVERLAY_SIZE OR NOT MHP2G_OVERLAY_CODE_SIZE
       OR NOT MHP2G_OVERLAY_HASH OR NOT MHP2G_OVERLAY_NAME)
        message(WARNING "mhp2g: incomplete overlay metadata in ${meta_path}")
        return()
    endif()

    file(GLOB sources CONFIGURE_DEPENDS "${source_dir}/*.cpp")
    if(NOT sources)
        message(WARNING "mhp2g: no sources for overlay ${MHP2G_OVERLAY_PREFIX}")
        return()
    endif()
    set(entry_point "${CMAKE_CURRENT_BINARY_DIR}/overlays/${MHP2G_OVERLAY_PREFIX}_module.cpp")
    configure_file("${MHP1_OVERLAY_SHARED_PROFILE_DIR}/host/overlay_module.cpp.in" "${entry_point}" @ONLY)

    set(target "overlay_${MHP2G_OVERLAY_PREFIX}")
    add_library(${target} MODULE ${sources} "${entry_point}")
    # Headers only: linking psprecomp_core would give the module its own copy of
    # the runtime state the host already owns.
    target_include_directories(${target} PRIVATE
        "${source_dir}" "${MHP1_OVERLAY_SHARED_PROFILE_DIR}/host" "${PROJECT_SOURCE_DIR}/include")
    if(WIN32 OR APPLE OR MHP2G_ANDROID_APP)
        # Both linkers want the host binary while linking the module: MSVC for
        # its import library, ld64 as the bundle loader. ELF leaves the host
        # symbols undefined and resolves them when the module is loaded. In an
        # Android app the host is libmain.so, which the app loads privately,
        # so the module names it as a dependency to find its symbols.
        target_link_libraries(${target} PRIVATE PokeFreedomDXBoot)
    else()
        add_dependencies(${target} PokeFreedomDXBoot)
        if(ANDROID)
            # The NDK links every shared library with --no-undefined; these
            # leave the host's symbols to the loader like any other ELF.
            target_link_options(${target} PRIVATE "LINKER:-z,undefs")
        endif()
    endif()
    if(MSVC)
        target_compile_definitions(${target} PRIVATE PSPRECOMP_IMPORT_HOST_SYMBOLS=1)
        if(PSPRECOMP_GENERATED_OPT_LEVEL STREQUAL "0")
            target_compile_options(${target} PRIVATE /Od /bigobj)
        else()
            target_compile_options(${target} PRIVATE /O${PSPRECOMP_GENERATED_OPT_LEVEL} /Ob0 /bigobj)
        endif()
    else()
        target_compile_options(${target} PRIVATE -O${PSPRECOMP_GENERATED_OPT_LEVEL} -g0)
    endif()
    if(MHP2G_RELEASE_LINK_OPTIONS)
        target_link_options(${target} PRIVATE ${MHP2G_RELEASE_LINK_OPTIONS})
    endif()
    set_target_properties(${target} PROPERTIES
        JOB_POOL_COMPILE psprecomp_generated
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        PREFIX ""
        SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
        OUTPUT_NAME "${MHP2G_OVERLAY_PREFIX}"
        LIBRARY_OUTPUT_DIRECTORY "${MHP1_OVERLAY_DIR}")
    if(MHP2G_ANDROID_APP)
        # An APK only carries libraries named lib*.so.
        set_target_properties(${target} PROPERTIES PREFIX "lib")
    endif()
endfunction()

file(GLOB MHP1_OVERLAY_METADATA CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/overlays/*/meta.txt")
foreach(meta IN LISTS MHP1_OVERLAY_METADATA)
    mhp1_add_overlay("${meta}")
endforeach()
