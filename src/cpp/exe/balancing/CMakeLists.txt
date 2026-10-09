# ===========================================================================
# CMake configuration
# ===========================================================================

# ===========================================================================
# Targets
# ===========================================================================

# ---------------------------------------------------------------------------
# Balancing Exe
# ---------------------------------------------------------------------------

add_executable (balancing
	${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_link_libraries (balancing
		PRIVATE
		${PROJECT_NAME}::lp_namer_input_reader
		${PROJECT_NAME}::lp_namer_problem_modifier
		${PROJECT_NAME}::problem_generation_main
		${PROJECT_NAME}::lp_namer_helper
		${PROJECT_NAME}::evaluator_lib
		${PROJECT_NAME}::balancing_lib
		${PROJECT_NAME}::exe_options_lib
		${PROJECT_NAME}::factories
)

install(TARGETS balancing DESTINATION bin)
