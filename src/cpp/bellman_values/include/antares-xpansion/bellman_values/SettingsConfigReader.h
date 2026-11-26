
#pragma once
#include <filesystem>

#include "antares-xpansion/config_reader/CommonSettingsConfigReader.h"

namespace bellman_values
{

class SettingsConfigReader final: public CommonSettingsConfigReader
{
public:
    explicit SettingsConfigReader(const std::filesystem::path& pathToYamlConfigFile = "")
    {
        load(pathToYamlConfigFile);
    }

private:
    void emplaceAllElements() override;
};

} // namespace bellman_values
