#pragma once
#include <memory>
#include <string>
#include <vector>

enum class BuildEventType {
    BuildStarted,
    BuildCompleted
};

struct BuildEvent {
    BuildEventType type = BuildEventType::BuildStarted;
    int projectId = 0;
    int buildId = 0;
    std::string status;
    double durationSeconds = 0.0;
    std::string timestamp;
};

class IBuildObserver {
public:
    virtual ~IBuildObserver() = default;
    virtual void onBuildEvent(const BuildEvent& event) = 0;
};

class BuildEventPublisher {
public:
    void subscribe(const std::shared_ptr<IBuildObserver>& observer);
    void unsubscribe(const std::shared_ptr<IBuildObserver>& observer);
    void notify(const BuildEvent& event);
    std::size_t activeObserverCount() const;

private:
    std::vector<std::weak_ptr<IBuildObserver>> observers_;
};

class ProjectBuildLogObserver final : public IBuildObserver {
public:
    void onBuildEvent(const BuildEvent& event) override;
    const std::vector<std::string>& entries() const noexcept;

private:
    std::vector<std::string> entries_;
};

class BuildExecutionStatsObserver final : public IBuildObserver {
public:
    void onBuildEvent(const BuildEvent& event) override;

    int startedCount() const noexcept;
    int completedCount() const noexcept;
    int successfulCount() const noexcept;
    int failedCount() const noexcept;
    double totalCompletedDuration() const noexcept;

private:
    int startedCount_ = 0;
    int completedCount_ = 0;
    int successfulCount_ = 0;
    int failedCount_ = 0;
    double totalCompletedDuration_ = 0.0;
};