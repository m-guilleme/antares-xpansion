# ===========================================================================
# CMake configuration
# ===========================================================================

# ===========================================================================
# Targets
# ===========================================================================

find_package(yaml-cpp CONFIG REQUIRED)

add_library(config_reader_lib STATIC)

target_sources(config_reader_lib PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ConfigReader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/antares-xpansion/config_reader/ConfigReader.h
)

target_link_libraries(config_reader_lib
        PUBLIC
        yaml-cpp::yaml-cpp
)

target_include_directories(config_reader_lib
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_library(${PROJECT_NAME}::config_reader_lib ALIAS config_reader_lib)
install(DIRECTORY include/antares-xpansion
        DESTINATION "include"
)
