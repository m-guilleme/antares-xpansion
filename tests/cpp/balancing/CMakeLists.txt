add_executable(balancing_test
        balancing_test.cpp)

target_link_libraries(balancing_test
        PRIVATE
        antaresXpansion::evaluator_lib
        antaresXpansion::balancing_lib
        antaresXpansion::output_core
        antaresXpansion::logger_lib
        antaresXpansion::problem_generation_main
        antaresXpansion::solvers
        GTest::gtest_main
        tests_utils)

add_test(NAME balancing COMMAND balancing_test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_property(TEST balancing PROPERTY LABELS unit)

add_executable(balancing_test_end_to_end
        balancing_test_end_to_end.cpp)

target_link_libraries(balancing_test_end_to_end
        PRIVATE
        antaresXpansion::evaluator_lib
        antaresXpansion::balancing_lib
        antaresXpansion::output_core
        antaresXpansion::logger_lib
        antaresXpansion::problem_generation_main
        antaresXpansion::solvers
        GTest::gtest_main
        tests_utils)

add_test(NAME balancing_end_to_end COMMAND balancing_test_end_to_end WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_property(TEST balancing_end_to_end PROPERTY LABELS unit)