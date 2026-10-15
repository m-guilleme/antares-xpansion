#pragma once
#include <filesystem>

#include "antares-xpansion/helpers/OptionsParser.h"

class CommonExeOptions: public OptionsParser
{
protected:
    std::filesystem::path studyPath_;
    int nbThreads_;

public:
    explicit CommonExeOptions();
    explicit CommonExeOptions(const std::string& description);
    virtual ~CommonExeOptions() = default;

    std::filesystem::path StudyPath() const
    {
        return studyPath_;
    }

    int NbThreads() const
    {
        return nbThreads_;
    }
};
