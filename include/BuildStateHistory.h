#pragma once
#include <string>

struct BuildStateHistory {
    int id = 0;
    int buildId = 0;
    std::string oldStatus;
    std::string newStatus;
    std::string changedAt;
    std::string reason;
};