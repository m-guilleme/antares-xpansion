#pragma once
#include <filesystem>
#include <string>

#include "antares-xpansion/config_reader/ConfigReader.h"

// Settings shared by every module (solver, keep_mps, problem_format, verbosity, cache_problems).
// Derived readers call load() from their constructor and may override emplaceAllElements()
// to add module-specific keys (they must call emplaceCommonElements() first).
class CommonSettingsConfigReader: protected ConfigReader
{
public:
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

protected:
    CommonSettingsConfigReader() = default;
    ~CommonSettingsConfigReader() = default;

    // Must be called from the most-derived constructor so that the virtual
    // emplaceAllElements() resolves to the derived override.
    void load(const std::filesystem::path& pathToYamlConfigFile);

    void emplaceAllElements() override
    {
        emplaceCommonElements();
    }

    void emplaceCommonElements();

private:
    void validateSolver() const;

    // keys from YAML file:
    inline static const std::string solverKey = "solver";
    inline static const std::string keepMpsKey = "keep_mps";
    inline static const std::string problemFormatKey = "problem_format";
    inline static const std::string verbosityKey = "verbosity";
    inline static const std::string cacheProblemsKey = "cache_problems";
};
