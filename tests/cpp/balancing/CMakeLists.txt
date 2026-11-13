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