#include "BuildObserver.h"
#include <algorithm>
#include <iostream>
#include <sstream>

void BuildEventPublisher::subscribe(const std::shared_ptr<IBuildObserver>& observer) {
    if (!observer) {
        return;
    }
    observers_.push_back(observer);
}

void BuildEventPublisher::unsubscribe(const std::shared_ptr<IBuildObserver>& observer) {
    if (!observer) {
        return;
    }
    std::erase_if(observers_, [&](const std::weak_ptr<IBuildObserver>& item) {
        auto locked = item.lock();
        return !locked || locked == observer;
        });
}

void BuildEventPublisher::notify(const BuildEvent& event) {
    std::erase_if(observers_, [](const std::weak_ptr<IBuildObserver>& item) {
        return item.expired();
        });

    for (const auto& item : observers_) {
        if (auto locked = item.lock()) {
            locked->onBuildEvent(event);
        }
    }
}

std::size_t BuildEventPublisher::activeObserverCount() const {
    std::size_t count = 0;
    for (const auto& item : observers_) {
        if (!item.expired()) {
            ++count;
        }
    }
    return count;
}

void ProjectBuildLogObserver::onBuildEvent(const BuildEvent& event) {
    std::ostringstream oss;
    oss << "[" << event.timestamp << "] Project #" << event.projectId
        << " | Build #" << event.buildId << " | ";

    if (event.type == BuildEventType::BuildStarted) {
        oss << "STARTED (status=" << event.status << ")";
    }
    else {
        oss << "COMPLETED (status=" << event.status
            << ", duration=" << event.durationSeconds << "s)";
    }

    const std::string line = oss.str();
    entries_.push_back(line);
    std::cout << "[ProjectLog] " << line << '\n';
}

const std::vector<std::string>& ProjectBuildLogObserver::entries() const noexcept {
    return entries_;
}

void BuildExecutionStatsObserver::onBuildEvent(const BuildEvent& event) {
    if (event.type == BuildEventType::BuildStarted) {
        ++startedCount_;
    }
    else if (event.type == BuildEventType::BuildCompleted) {
        ++completedCount_;
        totalCompletedDuration_ += event.durationSeconds;
        if (event.status == "Completed") {
            ++successfulCount_;
        }
        else if (event.status == "Failed") {
            ++failedCount_;
        }
    }
}

int BuildExecutionStatsObserver::startedCount() const noexcept {
    return startedCount_;
}

int BuildExecutionStatsObserver::completedCount() const noexcept {
    return completedCount_;
}

int BuildExecutionStatsObserver::successfulCount() const noexcept {
    return successfulCount_;
}

int BuildExecutionStatsObserver::failedCount() const noexcept {
    return failedCount_;
}

double BuildExecutionStatsObserver::totalCompletedDuration() const noexcept {
    return totalCompletedDuration_;
}