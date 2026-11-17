#pragma once
#include <filesystem>
#include <string>

#include "antares-xpansion/config_reader/ConfigReader.h"
#include "antares-xpansion/multisolver_interface/SolverAbstract.h"
#include "antares-xpansion/multisolver_interface/SolverConfig.h"

class SettingsConfigReader: ConfigReader
{
public:
    SettingsConfigReader(const std::filesystem::path& pathToYamlConfigFile = "")
    {
        emplaceAllElements(); // this method must be defined in the derived class
        initializeAllElements(pathToYamlConfigFile); // this method comes from the base class
        std::string solverName = getSolver();
        if (!(SolverConfig("xpress") == solverName || SolverConfig("CBC") == solverName))
        {
            throw std::runtime_error(
              "Unrecognized solver name. Update settings.yml with a correct solver.");
        }
    }

    // these getters are still useful to enforce types
    std::string getSolver() const
    {
        return getValueFromKey<std::string>(solverKey, elements_);
    }

    bool getKeepMps() const
    {
        return getValueFromKey<bool>(keepMpsKey, elements_);
    }

    std::string getProblemFormat() const
    {
        return getValueFromKey<std::string>(problemFormatKey, elements_);
    }

    std::string getVerbosity() const
    {
        return getValueFromKey<std::string>(verbosityKey, elements_);
    }

    bool getCacheProblems() const
    {
        return getValueFromKey<bool>(cacheProblemsKey, elements_);
    }

private:
    void emplaceAllElements() override;

    // keys from YAML file:
    inline static const std::string solverKey = "solver", keepMpsKey = "keep_mps",
                                    problemFormatKey = "problem_format", verbosityKey = "verbosity",
                                    cacheProblemsKey = "cache_problems";
};
