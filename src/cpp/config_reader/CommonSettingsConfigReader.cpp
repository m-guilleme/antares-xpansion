#include "antares-xpansion/config_reader/CommonSettingsConfigReader.h"

#include <stdexcept>

#include "antares-xpansion/multisolver_interface/SolverConfig.h"

void CommonSettingsConfigReader::load(const std::filesystem::path& pathToYamlConfigFile)
{
    emplaceAllElements();                        // virtual: derived override adds its own keys
    initializeAllElements(pathToYamlConfigFile); // from ConfigReader
    checkUnknownKeys();                          // from ConfigReader
    validateSolver();
}

void CommonSettingsConfigReader::emplaceCommonElements()
{
    // key (as in YAML) + default value; type is inferred from the default value
    elements_.emplace(solverKey, YAMLElement(solverKey, "xpress"));
    elements_.emplace(keepMpsKey, YAMLElement(keepMpsKey, false));
    elements_.emplace(problemFormatKey, YAMLElement(problemFormatKey, "OPTIMIZED"));
    elements_.emplace(verbosityKey, YAMLElement(verbosityKey, "INFO"));
    elements_.emplace(cacheProblemsKey, YAMLElement(cacheProblemsKey, false));
}

void CommonSettingsConfigReader::validateSolver() const
{
    static const SolverConfig xpress("xpress");
    static const SolverConfig cbc("CBC");

    const std::string solverName = getSolver();
    if (!(xpress == solverName || cbc == solverName))
    {
        throw std::runtime_error("Unrecognized solver name '" + solverName
                                 + "'. Update settings.yml with a correct solver.");
    }
}
