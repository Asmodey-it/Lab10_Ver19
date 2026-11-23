#pragma once
#include <memory>
#include <string>
#include <vector>
#include "BuildDurationStrategy.h"
#include "BuildObserver.h"
#include "BuildWorkflowService.h"

class IBuildCommand {
public:
    virtual ~IBuildCommand() = default;
    virtual std::string name() const = 0;
    virtual void execute() = 0;
};

class StartBuildCommand final : public IBuildCommand {
public:
    StartBuildCommand(BuildWorkflowService& workflow,
        BuildEventPublisher& publisher,
        int buildId,
        std::string timestamp);

    std::string name() const override;
    void execute() override;

private:
    BuildWorkflowService& workflow_;
    BuildEventPublisher& publisher_;
    int buildId_;
    std::string timestamp_;
};

class CompleteBuildCommand final : public IBuildCommand {
public:
    CompleteBuildCommand(BuildWorkflowService& workflow,
        BuildEventPublisher& publisher,
        const BuildDurationEvaluator* evaluator,
        BuildDurationMetrics metrics,
        int buildId,
        std::string timestamp);

    std::string name() const override;
    void execute() override;

private:
    BuildWorkflowService& workflow_;
    BuildEventPublisher& publisher_;
    const BuildDurationEvaluator* evaluator_;
    BuildDurationMetrics metrics_;
    int buildId_;
    std::string timestamp_;
    double lastEvaluationScore_ = 0.0;
};

class FailBuildCommand final : public IBuildCommand {
public:
    FailBuildCommand(BuildWorkflowService& workflow,
        BuildEventPublisher& publisher,
        int buildId,
        std::string reason,
        std::string timestamp,
        bool simulateHistoryError = false);

    std::string name() const override;
    void execute() override;

private:
    BuildWorkflowService& workflow_;
    BuildEventPublisher& publisher_;
    int buildId_;
    std::string reason_;
    std::string timestamp_;
    bool simulateHistoryError_;
};

class BuildCommandExecutor {
public:
    void run(std::unique_ptr<IBuildCommand> command);
    const std::vector<std::string>& executedCommands() const noexcept;

private:
    std::vector<std::string> executedCommands_;
};