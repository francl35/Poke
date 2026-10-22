# The reused kernel is process-scoped; isolate independent sessions accordingly.
foreach(mode ge-init ge ge-callback ge-options ge-invalid-list ge-blocking ge-draw-blocking ge-head services deadline dispatch module-low module-end module-unmapped display-mode display-width display-height trace translation-invalid translation-negative)
    execute_process(COMMAND "${TEST_EXECUTABLE}" "${mode}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "MHP1 ${mode} failed (${result}):\n${output}\n${errors}")
    endif()
    message(STATUS "MHP1 ${mode} passed")
endforeach()

# Exercise the complete GE -> reference rasterizer -> framebuffer capture path.
set(captures "${CMAKE_CURRENT_BINARY_DIR}/mhp1-video-captures")
file(REMOVE_RECURSE "${captures}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MHP1_RENDER_DIR=${captures}"
    "${TEST_EXECUTABLE}" ge-render RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 10)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "MHP1 ge-render failed (${result}):\n${output}\n${errors}")
endif()
if(NOT EXISTS "${captures}/draw_0.bmp" OR NOT EXISTS "${captures}/display_1.bmp")
    message(FATAL_ERROR "MHP1 GE/display captures were not produced")
endif()
file(REMOVE_RECURSE "${captures}")
