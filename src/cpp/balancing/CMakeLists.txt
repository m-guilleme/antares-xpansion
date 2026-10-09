# ===========================================================================
# CMake configuration
# ===========================================================================

# ===========================================================================
# Targets
# ===========================================================================

find_package(yaml-cpp CONFIG REQUIRED)

add_library(balancing_lib STATIC)

target_sources(balancing_lib PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/BalancingParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/SettingsConfigReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/antares-xpansion/balancing/BalancingParser.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/antares-xpansion/balancing/SettingsConfigReader.h
)

target_link_libraries(balancing_lib
        PUBLIC
        antaresXpansion::evaluator_lib
        antaresXpansion::helpers
        antaresXpansion::config_reader_lib
)

target_include_directories(balancing_lib
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_library(${PROJECT_NAME}::balancing_lib ALIAS balancing_lib)
install(DIRECTORY include/antares-xpansion
        DESTINATION "include"
)
