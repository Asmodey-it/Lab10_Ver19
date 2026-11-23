#include "BuildCommand.h"
#include <iostream>
#include <stdexcept>
#include <utility>

StartBuildCommand::StartBuildCommand(BuildWorkflowService& workflow,
    BuildEventPublisher& publisher,
    int buildId,
    std::string timestamp)
    : workflow_(workflow),
    publisher_(publisher),
    buildId_(buildId),
    timestamp_(std::move(timestamp)) {
}

std::string StartBuildCommand::name() const {
    return "StartBuildCommand";
}

void StartBuildCommand::execute() {
    Build updated = workflow_.startBuild(buildId_, timestamp_);
    publisher_.notify(BuildEvent{
        BuildEventType::BuildStarted,
        updated.projectId,
        updated.id,
        updated.status,
        updated.durationSeconds,
        timestamp_
        });
}

CompleteBuildCommand::CompleteBuildCommand(BuildWorkflowService& workflow,
    BuildEventPublisher& publisher,
    const BuildDurationEvaluator* evaluator,
    BuildDurationMetrics metrics,
    int buildId,
    std::string timestamp)
    : workflow_(workflow),
    publisher_(publisher),
    evaluator_(evaluator),
    metrics_(metrics),
    buildId_(buildId),
    timestamp_(std::move(timestamp)) {
}

std::string CompleteBuildCommand::name() const {
    return "CompleteBuildCommand";
}

void CompleteBuildCommand::execute() {
    Build updated = workflow_.completeBuild(buildId_, metrics_.actualDuration, timestamp_);
    if (evaluator_) {
        lastEvaluationScore_ = evaluator_->evaluate(metrics_);
        std::cout << "[DurationStrategy: " << evaluator_->strategyName()
            << "] Build #" << updated.id
            << " score = " << lastEvaluationScore_ << '\n';
    }
    publisher_.notify(BuildEvent{
        BuildEventType::BuildCompleted,
        updated.projectId,
        updated.id,
        updated.status,
        updated.durationSeconds,
        timestamp_
        });
}

FailBuildCommand::FailBuildCommand(BuildWorkflowService& workflow,
    BuildEventPublisher& publisher,
    int buildId,
    std::string reason,
    std::string timestamp,
    bool simulateHistoryError)
    : workflow_(workflow),
    publisher_(publisher),
    buildId_(buildId),
    reason_(std::move(reason)),
    timestamp_(std::move(timestamp)),
    simulateHistoryError_(simulateHistoryError) {
}

std::string FailBuildCommand::name() const {
    return "FailBuildCommand";
}

void FailBuildCommand::execute() {
    Build updated = workflow_.failBuild(buildId_, reason_, timestamp_, simulateHistoryError_);
    publisher_.notify(BuildEvent{
        BuildEventType::BuildCompleted,
        updated.projectId,
        updated.id,
        updated.status,
        updated.durationSeconds,
        timestamp_
        });
}

void BuildCommandExecutor::run(std::unique_ptr<IBuildCommand> command) {
    if (!command) {
        throw std::invalid_argument("Command cannot be null.");
    }
    const std::string cmdName = command->name();
    command->execute();
    executedCommands_.push_back(cmdName);
}

const std::vector<std::string>& BuildCommandExecutor::executedCommands() const noexcept {
    return executedCommands_;
}