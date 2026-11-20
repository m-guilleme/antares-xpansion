
#pragma once
#include <filesystem>
#include <string>

#include "antares-xpansion/config_reader/CommonSettingsConfigReader.h"

namespace balancing
{

class SettingsConfigReader final: public CommonSettingsConfigReader
{
public:
    explicit SettingsConfigReader(const std::filesystem::path& pathToYamlConfigFile = "")
    {
        load(pathToYamlConfigFile);
    }

    int getMaxIterations() const
    {
        return getValueFromKey<int>(maxIterationsKey, elements_);
    }

private:
    void emplaceAllElements() override;

    inline static const std::string maxIterationsKey = "max_iterations";
};

} // namespace balancing
