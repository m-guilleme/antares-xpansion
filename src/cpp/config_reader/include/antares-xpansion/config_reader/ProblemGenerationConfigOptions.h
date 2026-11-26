#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "antares-xpansion/core/ProblemFormat.h"
#include "antares-xpansion/lpnamer/main/ProblemGenerationOptions.h"

/**
 * Minimal ProblemGenerationOptions implementation used to build a
 * ConfigurationManager in ANTARES_API mode from a study path, without
 * parsing the command line a second time.
 *
 * Only StudyPath() is non-empty, so ConfigurationManager::Mode() resolves to
 * SimulationInputMode::ANTARES_API and Directories() returns:
 *   - study_dir           = studyPath
 *   - simulation_dir      = <studyPath>/output/<timestamp>eco
 *   - xpansion_output_dir = simulation_dir
 *   - archive_path        = empty
 *
 */
class ProblemGenerationConfigOptions final: public ProblemGenerationOptions
{
public:
    ProblemGenerationConfigOptions(std::filesystem::path studyPath, ProblemFormat format);

    [[nodiscard]] std::filesystem::path StudyPath() const override;
    [[nodiscard]] ProblemFormat Format() const override;

    [[nodiscard]] std::filesystem::path XpansionOutputDir() const override;
    [[nodiscard]] std::filesystem::path ArchivePath() const override;
    [[nodiscard]] std::string MasterFormulation() const override;
    [[nodiscard]] std::string AdditionalConstraintsFilename() const override;
    [[nodiscard]] std::filesystem::path WeightsFile() const override;
    [[nodiscard]] std::vector<int> ActiveYears() const override;
    [[nodiscard]] bool UnnamedProblems() const override;
    [[nodiscard]] std::filesystem::path deduceXpansionDirIfEmpty(
      std::filesystem::path xpansion_output_dir,
      const std::filesystem::path& archive_path) const override;

private:
    std::filesystem::path studyPath_;
    ProblemFormat format_;
};
