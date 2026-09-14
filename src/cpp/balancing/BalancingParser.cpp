#include "include/antares-xpansion/balancing/BalancingParser.h"

#include <stdexcept>

/// @brief Check if investment is possible for the area
/// @return true if investment is possible, false otherwise
bool AreaSettings::isInvestmentPossible() const
{
    return std::ranges::any_of(investmentCandidates,
                               [](const auto& entry) {
                                   return entry.second.currentCapacity
                                          < entry.second.params->expansionPotential;
                               });
}

/// @brief Check if decommissioning is possible for the area
/// @return true if decommissioning is possible, false otherwise
bool AreaSettings::isDecommissioningPossible() const
{
    return std::ranges::any_of(decommissioningCandidates,
                               [](const auto& entry) {
                                   return entry.second.currentCapacity
                                          > entry.second.params->decommissioningPotential;
                               });
}

/// @brief Check if disinvestment is possible for the area
/// @return true if disinvestment is possible, false otherwise
bool AreaSettings::isDisinvestmentPossible() const
{
    return std::ranges::any_of(investmentCandidates,
                               [](const auto& entry) {
                                   return entry.second.currentCapacity
                                          > entry.second.initialCapacity;
                               });
}

/// @brief Check if recommissioning is possible for the area
/// @return true if recommissioning is possible, false otherwise
bool AreaSettings::isRecommissioningPossible() const
{
    return std::ranges::any_of(decommissioningCandidates,
                               [](const auto& entry) {
                                   return entry.second.currentCapacity
                                          < entry.second.initialCapacity;
                               });
}

/// @brief Constructor of the BalancingParser class
/// @param pathToYamlConfigFile The path to the YAML configuration file
BalancingParser::BalancingParser(const std::filesystem::path& pathToYamlConfigFile):
    pathToYamlConfigFile(pathToYamlConfigFile),
    reliabilityStandardDeadBandUp(0.0),
    reliabilityStandardDeadBandDown(0.0)
{
    if (!pathToYamlConfigFile.empty())
    {
        if (!std::filesystem::exists(pathToYamlConfigFile))
        {
            throw std::runtime_error("YAML config file does not exist: "
                                     + pathToYamlConfigFile.string());
        }
        parse();
    }
}

/// @brief Parse the YAML configuration file and fill the BalancingParser attributes with the parsed
/// data
void BalancingParser::parse()
{
    try
    {
        config = YAML::LoadFile(pathToYamlConfigFile.string());

        parseGlobalSettings();
        parseDecommissioningCandidatesTypes();
        parseInvestmentCandidatesTypes();
        parseAreasSettings();
    }
    catch (const YAML::Exception& e)
    {
        throw std::runtime_error("YAML parsing error: " + std::string(e.what()));
    }
}

/// @brief Throws if a required YAML field is missing, with a descriptive message
static void requireField(const YAML::Node& node,
                         const std::string& field,
                         const std::string& context)
{
    if (!node[field])
    {
        throw std::runtime_error("Missing '" + field + "' for " + context);
    }
}

/// @brief Parse the global settings from the YAML configuration file : reliability standard dead
/// bands and indicator
void BalancingParser::parseGlobalSettings()
{
    if (config["reliability_standard_dead_band_up"])
    {
        reliabilityStandardDeadBandUp = config["reliability_standard_dead_band_up"].as<double>();
    }

    if (config["reliability_standard_dead_band_down"])
    {
        reliabilityStandardDeadBandDown = config["reliability_standard_dead_band_down"]
                                            .as<double>();
    }

    if (config["reliability_standard_indicator"])
    {
        auto criterionStr = config["reliability_standard_indicator"].as<std::string>();
        if (criterionStr == "LOLE")
        {
            reliabilityStandardIndicator = Benders::Criterion::Type::UnsuppliedEnergy;
        }
        else if (criterionStr == "NPCAP_HOURS")
        {
            reliabilityStandardIndicator = Benders::Criterion::Type::NearPriceCapHours;
        }
        else
        {
            throw std::runtime_error(
              "YAML parsing error: " + criterionStr
              + " is not a correct reliabilityStandardIndicator (LOLE or NPCAP_HOURS)");
        }
    }
}

/// @brief Parse the decommissioning candidates types from the YAML configuration file : fixed O&M
/// costs
void BalancingParser::parseDecommissioningCandidatesTypes()
{
    if (!config["decommissioning_candidates_types"])
    {
        return;
    }

    for (const auto& typeNode: config["decommissioning_candidates_types"])
    {
        std::string typeName = typeNode.first.as<std::string>();
        YAML::Node typeData = typeNode.second;
        const std::string context = "decommissioning candidate of type: " + typeName;

        requireField(typeData, "fixed_om_costs", context);
        requireField(typeData, "decommissioning_potential", context);
        requireField(typeData, "decommissioning_cost", context);

        auto type = std::make_shared<Decommissioning>();
        type->fixedOmCosts = typeNode.second["fixed_om_costs"].as<double>();
        type->decommissioningPotential = typeData["decommissioning_potential"].as<double>();
        type->decommissioningCost = typeData["decommissioning_cost"].as<double>();
        decommissioningCandidatesTypes[typeName] = std::move(type);
    }
}

/// @brief Parse the investment candidates types from the YAML configuration file : derating,
/// expansion potential, investment cost and fixed O&M costs
void BalancingParser::parseInvestmentCandidatesTypes()
{
    if (!config["investment_candidates_types"])
    {
        return;
    }

    for (const auto& typeNode: config["investment_candidates_types"])
    {
        std::string typeName = typeNode.first.as<std::string>();
        YAML::Node typeData = typeNode.second;
        const std::string context = "investment candidate of type: " + typeName;

        requireField(typeData, "derating", context);
        requireField(typeData, "expansion_potential", context);
        requireField(typeData, "investment_cost", context);
        requireField(typeData, "fixed_om_costs", context);

        auto type = std::make_shared<Investment>();
        type->derating = typeData["derating"].as<double>();
        type->expansionPotential = typeData["expansion_potential"].as<double>();
        type->investmentCost = typeData["investment_cost"].as<double>();
        type->fixedOmCosts = typeData["fixed_om_costs"].as<double>();

        investmentCandidatesTypes[typeName] = std::move(type);
    }
}

/// @brief Parse a mapping of candidate names to their type from a YAML area node, and populate
///        the target map with the resolved type data.
/// @param areaData    YAML node representing a single area's configuration.
/// @param yamlKey     Key under @p areaData that holds the candidate-to-type mapping.
/// @param candidateTypes Registry mapping type name strings to their shared parameter objects,
///                     used to resolve each candidate's type.
/// @param label       Human-readable category label (e.g. "investment", "decommissioning"),
///                    used in error messages.
/// @param areaName    Name of the area being parsed, used in error messages.
/// @param candidates   Destination map to populate, keyed by candidate name.
///
/// @throws std::runtime_error If a candidate references a type name not found in @p typeRegistry.
static void parseCandidatesToType(const YAML::Node& areaData,
                                  const std::string& yamlKey,
                                  const auto& candidateTypes,
                                  const std::string& label,
                                  const std::string& areaName,
                                  auto& candidates)
{
    const std::string context = "investment candidate of type: " + yamlKey;
    requireField(areaData, yamlKey, context);

    for (const auto& candidate: areaData[yamlKey])
    {
        std::string candidateName = candidate.first.as<std::string>();
        std::replace(candidateName.begin(), candidateName.end(), ' ', '*');
        const std::string typeName = candidate.second.as<std::string>();

        auto it = candidateTypes.find(typeName);
        if (it == candidateTypes.end())
        {
            throw std::runtime_error("Unknown " + label + " type '" + typeName + "' for candidate '"
                                     + candidateName + "' in area '" + areaName + "'");
        }

        candidates[candidateName] = {it->second, 0};
    }
}

/// @brief Parse the areas from the YAML configuration file : reliability standard, decommissioning
/// and investment increments, candidates to type mapping
void BalancingParser::parseAreasSettings()
{
    if (!config["areas"])
    {
        return;
    }

    for (const auto& areaNode: config["areas"])
    {
        std::string areaName = areaNode.first.as<std::string>();
        YAML::Node areaData = areaNode.second;
        const std::string context = "area: " + areaName;

        requireField(areaData, "reliability_standard", context);
        requireField(areaData, "decommissioning_increment", context);
        requireField(areaData, "investment_increment", context);
        requireField(areaData, "max_oscillation", context);

        AreaSettings area;
        area.reliabilityStandard = areaData["reliability_standard"].as<double>();
        area.decommissioningIncrement = areaData["decommissioning_increment"].as<double>();
        area.currentDecommissioningIncrement = area.decommissioningIncrement;
        area.investmentIncrement = areaData["investment_increment"].as<double>();
        area.currentInvestmentIncrement = area.investmentIncrement;
        area.maxOscillation = areaData["max_oscillation"].as<int>();

        area.reliabilityStandardDeadBandUp = areaData["reliability_standard_dead_band_up"]
                                               ? areaData["reliability_standard_dead_band_up"]
                                                   .as<double>()
                                               : reliabilityStandardDeadBandUp;

        area.reliabilityStandardDeadBandDown = areaData["reliability_standard_dead_band_down"]
                                                 ? areaData["reliability_standard_dead_band_down"]
                                                     .as<double>()
                                                 : reliabilityStandardDeadBandDown;

        parseCandidatesToType(areaData,
                              "decommissioning_candidates_to_type",
                              decommissioningCandidatesTypes,
                              "decommissioning",
                              areaName,
                              area.decommissioningCandidates);

        parseCandidatesToType(areaData,
                              "investment_candidates_to_type",
                              investmentCandidatesTypes,
                              "investment",
                              areaName,
                              area.investmentCandidates);

        areaSettings[areaName] = std::move(area);
    }
}

/// @brief Get the reliability standard dead band up value
/// @return The reliability standard dead band up value
double BalancingParser::getReliabilityStandardDeadBandUp() const
{
    return reliabilityStandardDeadBandUp;
}

/// @brief Get the reliability standard dead band down value
/// @return The reliability standard dead band down value
double BalancingParser::getReliabilityStandardDeadBandDown() const
{
    return reliabilityStandardDeadBandDown;
}

/// @brief Get the reliability standard indicator
/// @return The reliability standard indicator
Benders::Criterion::Type BalancingParser::getReliabilityStandardIndicator() const
{
    return reliabilityStandardIndicator;
}
