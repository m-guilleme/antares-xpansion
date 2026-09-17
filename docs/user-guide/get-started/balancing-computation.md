# Balancing Computation

This document explains what balancing are and how to compute them for Antares studies using `balancing`. It covers both the business perspective and practical usage. This document is intended for users familiar with Antares studies.

---

## How to use `balancing`

`balancing` is a command-line program that requires the following inputs:

- an **Antares study**
- a mandatory input file **input_balancing.yml** holding parameters of the simulation
- an optional input file **settings.yml** with global technical settings
These files are expected at `<study_root>/user/balancing/`.

### Input files 

#### input_balancing.yml

Here is an example of a **input_balancing.yml** file, that defines parameters of the balancing problem :

```yaml
# reliabilty standard use to compute the upper bound of the balancing criteria
reliability_standard_dead_band_up: 0.5
# reliabilty standard use to compute the lower bound of the balancing criteria
reliability_standard_dead_band_down: 0.5
# which reliability standard indicator to use
reliability_standard_indicator: LOLE # Or NPCAP_HOURS

# Parameters dedicated to each area
areas:
  # name of the area
  area1:
    # reliability standard for the area
    reliability_standard: 3.5
    # decrement value of reference used for decommissioning/recommissioning action
    decommissioning_increment: 1000
    # increment value of reference used for investiment/desinvestment action
    investment_increment: 500
    # maximum number of oscillation between different action per candidate
    max_oscillation: 10
    # type of cluster for decommissioning cluster candidate
    decommissioning_candidates_to_type:
      # name of cluster : type of candidate to apply
      unprofitable_peak: decom_type
    # type of cluster for investment cluster candidate
    investment_candidates_to_type:
      # name of cluster : type of candidate to apply
      invest_semibase1: semibase_type
      invest_peak: peak_type

# Definition of the cluster candidates type

# decommissioning cluster candidate types
decommissioning_candidates_types:
  # name of the decommissioning type
  decom_type:
    # decommmissioning potential that's limit the invested capacity to decommssion
    decommissioning_potential: 0.0
    # initial cost of decommissioning/recommssioning action
    decommissioning_cost: 10000
    # fixed cost of action
    fixed_om_costs: 1000
# investment cluster candidate types
investment_candidates_types:
  # name of the investment type
  semibase_type:
    derating: 0.0
    # expansion potential which limits the invested capacity to investment
    expansion_potential: 4000
    # cost of investment/desinvestment action
    investment_cost: 126000
    # fixed cost of action
    fixed_om_costs: 0
  peak_type:
    derating: 0.0
    expansion_potential: 3000
    investment_cost: 60000
    fixed_om_costs: 0
```

This file is expected to be located at `<study_root>/user/balancing/input_balancing.yml`.

Inputs are to be specified by area and all of them are mandatory if an area is configured. If an area present in the study is not configured then it will be ignored during balancing and no action will be taken on it. The example above covers a case where inputs of `area1` are set.

#### settings.yml

Here is an example of a **settings.yml** file, that defines global technical parameters:

```yaml
# All parameters related to general settings when computing balancing.
# Use ~ to fall back on default values (as implemented in C++ code)
# default values are subject to change

max_iterations : 40
# Set the maximum number of iterations authorized for the balancing resolution

solver : xpress
# default: xpress
# possible values are: xpress, coin

keep_mps : false
# If true, a file (.mps or .svf, see below) for initial problems will be written to disk (location is `<study>/output/<run>/initial_problems`)
# default: false

problem_format : MPS
# Selects the storage format of the generated mathematical problems (master + subproblems):
# - OPTIMIZED (default) : use underlying solver to write problems in an optimized format to reduce disk space usage and I/O time. The underlying format depends on the solver used.
#   - XPRESS : svf format: compressed binary format.
#   - COIN : unsupported. Falls back to MPS.
# - MPS : write the problems in MPS format, which is a standard format for mathematical programming problems.
# default: OPTIMIZED
# possible values are: MPS, OPTIMIZED

verbosity : INFO
# Sets the desired level of verbosity of log messages displayed in the console. 
# Setting the verbosity to a given level will allow messages to appear if their level is higher in the list or equal to the level specified.
# For example setting the verbosity to `WARNING` will filter out all messages at the `TRACE`, `DEBUG` or `INFO` level,
# and will pass along all messages at the `WARNING`, `ERR` or `FATAL` level.
# default: INFO
# possible values are: NONE, TRACE, DEBUG, INFO, WARNING, ERR, FATAL

cache_problems : true
# Sets whether to write and read problems from disk to reduce memory use (will increase computation time) 
# default: false
```

This file is expected to be located at `<study_root>/user/balancing/settings.yml`. It is optional then default values are hard-coded in the program.

## Outputs

The outputs are three files: **final_capacities.csv**, **iterations_values_log.csv**, and **final_criteria.csv**.

Outputted files consist of: 

- a comma-separated values file named `final_capacities.csv`;
  - contains for each candidate cluster of each area their final balanced invested capacity and the capacity change from initial invested capacity ;
- a comma-separated values file named `iterations_values_log.csv`;
  - contains the criteria, action and capacity change of selected cluster candidates at each iteration ;
- a comma-separated values file named `final_criteria.csv`;
  - contains for each area the criterion target, criterion lower bound, criterion upper bound, final criterion value, their status (OK or not) and the total capacity change.

These files will be created in a timecoded folder located at `<study_root>/output/<YYYYMMDD-hhmm>eco/`. This folder will hold all output files produced by the program.

## Command line usage

### Quick start

1. Open a command line prompt in your Antares-Xpansion install directory (by default it is named `antaresXpansion-x.y.z-<platform>` where `x.y.z` is the version number).
    On Windows, you can launch a command line prompt by typing `cmd` in the path.

2. Run `balancing<.exe>` and specify the path to the Antares study with the `--study` parameter:

    ```cmd
    balancing --study data_test/one_node_base
    ```

### Command line parameters

Other command line parameters can be added to the previous line.

#### `-h, --help`

Show a help message and exit.

#### `--study <path>`

Path to the Antares study.

#### `--threads <number>`

Default value: `1`.

Number of threads that will be used to solve the balancing.

