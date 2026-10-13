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

class BalancingParser
{
public:
    BalancingParser(const std::filesystem::path& pathToYamlConfigFile = "");

    std::map<std::string, Area> parse();

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

    void parseGlobalSettings();
    void parseAreasSettings();
    void parseDecommissioningCandidatesTypes();
    void parseInvestmentCandidatesTypes();
};
