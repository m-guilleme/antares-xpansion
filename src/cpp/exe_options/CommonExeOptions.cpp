#include "antares-xpansion/exe_options/CommonExeOptions.h"

namespace po = boost::program_options;

CommonExeOptions::CommonExeOptions():
    CommonExeOptions("Common options for Antares Xpansion executables")
{
}

CommonExeOptions::CommonExeOptions(const std::string& description):
    OptionsParser(description)
{
    AddOptions()("help,h", "produce help message")(
      "study",
      po::value<std::filesystem::path>(&studyPath_)->required(),
      "Path to archive (required)")("threads",
                                    po::value<int>(&nbThreads_)->default_value(1),
                                    "Number of threads to use (optional, default is 1)");
}
