#include "antares-xpansion/config_reader/ProblemGenerationConfigOptions.h"

#include <utility>

ProblemGenerationConfigOptions::ProblemGenerationConfigOptions(std::filesystem::path studyPath,
                                                               ProblemFormat format):
    studyPath_(std::move(studyPath)),
    format_(format)
{
}

std::filesystem::path ProblemGenerationConfigOptions::StudyPath() const
{
    return studyPath_;
}

ProblemFormat ProblemGenerationConfigOptions::Format() const
{
    return format_;
}

// Empty values below make ConfigurationManager::Mode() resolve to ANTARES_API

std::filesystem::path ProblemGenerationConfigOptions::XpansionOutputDir() const
{
    return {};
}

std::filesystem::path ProblemGenerationConfigOptions::ArchivePath() const
{
    return {};
}

std::string ProblemGenerationConfigOptions::MasterFormulation() const
{
    return "relaxed";
}

std::string ProblemGenerationConfigOptions::AdditionalConstraintsFilename() const
{
    return {};
}

std::filesystem::path ProblemGenerationConfigOptions::WeightsFile() const
{
    return {};
}

std::vector<int> ProblemGenerationConfigOptions::ActiveYears() const
{
    return {};
}

bool ProblemGenerationConfigOptions::UnnamedProblems() const
{
    return false;
}

std::filesystem::path ProblemGenerationConfigOptions::deduceXpansionDirIfEmpty(
  std::filesystem::path xpansion_output_dir,
  const std::filesystem::path& /*archive_path*/) const
{
    return xpansion_output_dir;
}
