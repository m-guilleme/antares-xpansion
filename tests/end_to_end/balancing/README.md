# BalancingEndToEnd tests description

## Context

This is a test for the inner optimization part of AntaresXpansion. Those tests focus on the balancing C++ executables

**Input** : Antares studies with the following balancing inputs files :

* input_balancing.yml containing the specific inputs for balancing
* settings.yml containings global parameters for the balancing

**Output** : Three followinf files in the ouput directory of the study :
* final_capacities.csv containing candidates results
* final_criteria.csv" containing criterion and area results
* iterations_values_log.csv" containing iterative logs 

## Launching the test

Python module *pytest* should be installed. The test can be run with the following command, from the bendersEndToEnd folder :
    python3 -m pytest --intallDir=*path_to_executables_folder*

## Test markers

All the tests present in this test have the common marker **optim**.
There are also markers to launch only tests on specific optimization executables :

* unspenerg : launching only tests with UNSP_ENERG reliability standard indicator
* lole : launching only tests with LOLE reliability standard indicator

To run some specific tests, markers can be used as follows :
    python3 -m pytest --intallDir=*path_to_executables_folder* -m *wanted_marker*