#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "antares-xpansion/xpansion_interfaces/LoggerUtils.h"
#include "yaml-cpp/yaml.h"

namespace Benders::Criterion
{
enum class Type
{
    UnsuppliedEnergy,
    NearPriceCapHours,
};

constexpr std::string_view getPrefix(Type t)
{
    switch (t)
    {
    case Type::UnsuppliedEnergy:
        return "UnsuppliedEnergy::";
    case Type::NearPriceCapHours:
        return "AreaBalance::";
    }
    return "";
}

class CriterionInputFileError: public LogUtils::XpansionError<std::runtime_error>
{
    using LogUtils::XpansionError<std::runtime_error>::XpansionError;
};

class CriterionInputFileIsEmpty: public LogUtils::XpansionError<std::runtime_error>
{
    using LogUtils::XpansionError<std::runtime_error>::XpansionError;
};

class CriterionInputFileNoPatternFound: public LogUtils::XpansionError<std::runtime_error>
{
    using LogUtils::XpansionError<std::runtime_error>::XpansionError;
};

class CriterionInputPatternsShouldBeArray: public LogUtils::XpansionError<std::runtime_error>
{
    using LogUtils::XpansionError<std::runtime_error>::XpansionError;
};

class CouldNotReadAreaField: public LogUtils::XpansionError<std::runtime_error>
{
    using LogUtils::XpansionError<std::runtime_error>::XpansionError;
};

class CouldNotReadCriterionField: public LogUtils::XpansionError<std::runtime_error>
{
    using LogUtils::XpansionError<std::runtime_error>::XpansionError;
};

/// @brief lovely class
class CriterionPattern
{
public:
    explicit CriterionPattern(std::string_view prefix, std::string_view body);
    CriterionPattern() = default;
    [[nodiscard]] std::string Value() const;
    [[nodiscard]] std::string_view GetPrefix() const;
    void SetPrefix(std::string_view prefix);
    [[nodiscard]] std::string_view GetBody() const;
    void SetBody(std::string_view body);

private:
    std::string prefix_;
    std::string body_;
};

/// @brief holds the pattern and the criterion
class CriterionSingleInputData
{
public:
    CriterionSingleInputData() = default;
    /// @brief constructor
    /// @param prefix the prefix in the variable's name
    /// @param body any string that could be in the variable's name
    /// @param criterion the criterion that should be satisfied
    CriterionSingleInputData(std::string_view prefix, std::string_view body, double criterion);

    [[nodiscard]] CriterionPattern Pattern() const;
    [[nodiscard]] double Criterion() const;
    void SetCriterion(double criterion);
    void ResetPattern(std::string_view prefix, std::string_view body);

private:
    CriterionPattern pattern_;
    double criterion_{0};
};

/// @brief this class contains all data read from user input file
class CriterionInputData
{
public:
    CriterionInputData(const Type criterion = Type::UnsuppliedEnergy):
        criterion(criterion)
    {
    }

    [[nodiscard]] const std::vector<CriterionSingleInputData>& Criteria() const;

    [[nodiscard]] std::vector<std::string> PatternBodies() const;
    [[nodiscard]] std::string PatternsPrefix() const;

    void SetCriterionCountThreshold(double count_threshold);
    [[nodiscard]] double CriterionCountThreshold() const;
    void AddSingleData(const CriterionSingleInputData& data);

    Type criterion{Type::UnsuppliedEnergy};

private:
    std::vector<CriterionSingleInputData> criterion_vector_;
    double count_threshold_ = 1;
};

/// @brief this class contains all data read from user input file
class OuterLoopCriterionInputData: public CriterionInputData
{
public:
    OuterLoopCriterionInputData() = default;
    [[nodiscard]] double StoppingThreshold() const;
    void setStoppingThreshold(double stoppingThreshold);

private:
    double stopping_threshold_ = 1e-4;
};

/// @brief Abstract
class ICriterionInputDataReader
{
public:
    virtual OuterLoopCriterionInputData Read(const std::filesystem::path& input_file) = 0;
    virtual ~ICriterionInputDataReader() = default;
};

class CriterionInputFromYaml: public ICriterionInputDataReader
{
public:
    CriterionInputFromYaml() = default;
    OuterLoopCriterionInputData Read(const std::filesystem::path& input_file) override;

private:
    CriterionInputData criterion_input_data_;
};

} // namespace Benders::Criterion
