# ===========================================================================
# CMake configuration
# ===========================================================================

# ===========================================================================
# Targets
# ===========================================================================

add_library(exe_options_lib STATIC)

target_sources(exe_options_lib PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/CommonExeOptions.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/antares-xpansion/exe_options/CommonExeOptions.h
)

target_link_libraries(exe_options_lib
        PUBLIC
        ${PROJECT_NAME}::helpers
)

target_include_directories(exe_options_lib
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_library(${PROJECT_NAME}::exe_options_lib ALIAS exe_options_lib)
install(DIRECTORY include/antares-xpansion
        DESTINATION "include"
)