#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include <antares/api/solver.h>

#include "antares-xpansion/balancing/Area.h"
#include "antares-xpansion/balancing/Candidate.h"
#include "antares-xpansion/benders/benders_core/CriterionInputDataReader.h"
#include "antares-xpansion/lpnamer/model/Problem.h"

class InputBalancingParser
{
public:
    /// @brief Constructor of the BalancingParser class
    /// @param pathToYamlConfigFile The path to the YAML configuration file
    InputBalancingParser(const std::filesystem::path& pathToYamlConfigFile = "");
    /// @brief Parse the YAML configuration file and fill the InputBalancingParser attributes with
    /// the parsed data
    std::map<std::string, Area> parse();
    /// @brief Get the reliability standard indicator
    /// @return The reliability standard indicator
    Benders::Criterion::Type getReliabilityStandardIndicator() const;

private:
    std::map<std::string, Area> areas;
    std::filesystem::path pathToYamlConfigFile;
    YAML::Node config;
    double defaultReliabilityStandardDeadBandUp;
    double defaultReliabilityStandardDeadBandDown;
    Benders::Criterion::Type reliabilityStandardIndicator;
    std::map<std::string, std::shared_ptr<DecommissioningCandidateType>>
      decommissioningCandidatesTypes;
    std::map<std::string, std::shared_ptr<InvestmentCandidateType>> investmentCandidatesTypes;
    /// @brief Parse the global settings from the YAML configuration file : reliability standard
    /// dead bands and indicator
    void parseGlobalSettings();
    /// @brief Parse the areas from the YAML configuration file : reliability standard,
    /// decommissioning and investment increments, candidates to type mapping
    void parseAreas();
    /// @brief Parse the decommissioning candidates types from the YAML configuration file : fixed
    /// O&M costs
    void parseDecommissioningCandidatesTypes();
    /// @brief Parse the investment candidates types from the YAML configuration file : derating,
    /// expansion potential, investment cost and fixed O&M costs
    void parseInvestmentCandidatesTypes();
};
