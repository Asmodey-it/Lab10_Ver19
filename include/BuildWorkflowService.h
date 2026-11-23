#pragma once
#include <string>
#include "BuildState.h"

class BuildWorkflowService {
public:
    explicit BuildWorkflowService(std::string dbPath);

    Build startBuild(int buildId, const std::string& changedAt);
    Build completeBuild(int buildId, double finalDurationSeconds, const std::string& changedAt);
    Build failBuild(int buildId, const std::string& reason, const std::string& changedAt,
        bool simulateHistoryError = false);

private:
    std::string dbPath_;
};