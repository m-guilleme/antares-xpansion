
#include "antares-xpansion/balancing/SettingsConfigReader.h"

namespace balancing
{

void SettingsConfigReader::emplaceAllElements()
{
    emplaceCommonElements();
    elements_.emplace(maxIterationsKey, YAMLElement(maxIterationsKey, 30));
}

} // namespace balancing
