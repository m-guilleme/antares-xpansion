#include "antares-xpansion/bellman_values/ProblemManager.h"

ProblemManager::ProblemManager(const std::string& solverName,
                               const std::string& problemFormat,
                               bool writePbFiles,
                               bool cacheProblems,
                               std::optional<std::filesystem::path> problemsPath):
    solverName_(solverName),
    writePbFiles_(writePbFiles),
    cacheProblems_(cacheProblems),
    problemsPath_(problemsPath),
    solverLogManager_(),
    solverFactory_()
{
    setProblemFormat(problemsFormatFromString(problemFormat));
    if ((cacheProblems || writePbFiles) && problemsPath == std::nullopt)
    {
        throw std::runtime_error(
          "Error: trying to stream problems from disk without specifying a folder.");
    }
    if ((cacheProblems || writePbFiles) && !std::filesystem::exists(problemsPath.value()))
    {
        std::filesystem::create_directories(problemsPath.value());
    }
}

ProblemManager::ProblemManager(const ProblemManager& problemManagerToCopy):
    ProblemManager(problemManagerToCopy.solverName_,
                   "OPTIMIZED",
                   problemManagerToCopy.writePbFiles_,
                   problemManagerToCopy.cacheProblems_,
                   problemManagerToCopy.problemsPath_)
{
    setProblemFormat(problemManagerToCopy.problemFormat_);
}

ProblemManager::~ProblemManager()
{
    // removing problems if they were not needed
    if (cacheProblems_ && !writePbFiles_)
    {
        for (auto& path: std::filesystem::directory_iterator(problemsPath_.value()))
        {
            std::filesystem::remove_all(path);
        }
    }
}

void ProblemManager::setProblems(
  std::map<Antares::Solver::WeeklyProblemId, std::shared_ptr<Problem>>& problems)
{
    problems_.clear();
    problemIds_.clear();

    for (auto& [pbId, problem]: problems)
    {
        setProblem(pbId, problem);
    }
}

std::shared_ptr<Problem> ProblemManager::readProblemFromDisk(const std::string& problemName) const
{
    if (!cacheProblems_)
    {
        throw std::runtime_error("Error: trying to stream problem from disk.");
    }
    try
    {
        auto problem = std::make_shared<Problem>(solverFactory_.create_solver(
          solverName_ == SolverConfig("xpress")
            ? "xpress"
            : "CBC", // not optimal, but similar to what is done in ProblemGenerationForWaterValues
          solverLogManager_));
        // std::cout << "New problem created\n";
        switch (problemFormat_)
        {
        case ProblemsFormat::MPS_FILE:
            problem->read_prob_mps(problemsPath_.value() / (problemName + ".mps"));
            break;
        case ProblemsFormat::OPTIMIZED:
            problem->restore_prob(problemsPath_.value() / (problemName + ".svf"));
            break;
            // potential errors are handled by
            // problemsFormatFromString called in constructor
        }
        return problem;
    }
    catch (const std::exception& e)
    {
        std::string message = "Error reading problem " + problemName + " from file:\n"
                              + std::string(e.what());
        throw std::runtime_error(message);
    }
}

std::vector<double> ProblemManager::getProblemSolution(const Antares::Solver::WeeklyProblemId& pbId,
                                                       std::shared_ptr<Problem> problem) const
{
    if (cacheProblems_)
    {
        const auto& solution = solutions_.find(pbId);
        if (solution != solutions_.end())
        {
            return solution->second;
        }
        throw std::runtime_error("No solution was found in memory for problem "
                                 + getPbNameFromId(pbId));
    }
    else
    {
        std::vector<double> solution(problem->get_ncols());
        problem->get_lp_sol(solution.data(), NULL, NULL);
        return solution;
    }
}

void ProblemManager::saveProblemToFile(const Antares::Solver::WeeklyProblemId& pbId,
                                       std::shared_ptr<Problem> problem,
                                       std::filesystem::path& folder) const
{
    switch (problemFormat_)
    {
    case ProblemsFormat::MPS_FILE:
        problem->write_prob_mps(folder / (getPbNameFromId(pbId) + ".mps"));
        break;
    case ProblemsFormat::OPTIMIZED:
        problem->save_prob(folder / (getPbNameFromId(pbId) + ".svf"));
        break;
        // potential errors are handled by
        // problemsFormatFromString in constructor
    }
}
