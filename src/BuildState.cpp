#include "BuildState.h"
#include <stdexcept>

namespace {

    class CreatedBuildState final : public IBuildState {
    public:
        std::string name() const override { return "Created"; }

        void start(BuildContext& ctx) override;
        void complete(BuildContext&, double) override {
            throw std::logic_error("Cannot complete a build that has not started yet.");
        }
        void fail(BuildContext& ctx, const std::string& reason) override;
    };

    class RunningBuildState final : public IBuildState {
    public:
        std::string name() const override { return "Running"; }

        void start(BuildContext&) override {
            throw std::logic_error("Build is already running.");
        }
        void complete(BuildContext& ctx, double finalDurationSeconds) override;
        void fail(BuildContext& ctx, const std::string& reason) override;
    };

    class CompletedBuildState final : public IBuildState {
    public:
        std::string name() const override { return "Completed"; }

        void start(BuildContext&) override {
            throw std::logic_error("Completed build cannot be restarted.");
        }
        void complete(BuildContext&, double) override {
            throw std::logic_error("Build is already completed.");
        }
        void fail(BuildContext&, const std::string&) override {
            throw std::logic_error("Completed build cannot transition to Failed.");
        }
    };

    class FailedBuildState final : public IBuildState {
    public:
        std::string name() const override { return "Failed"; }

        void start(BuildContext&) override {
            throw std::logic_error("Failed build cannot be started directly.");
        }
        void complete(BuildContext&, double) override {
            throw std::logic_error("Failed build cannot be marked as Completed.");
        }
        void fail(BuildContext&, const std::string&) override {
            throw std::logic_error("Build is already in Failed state.");
        }
    };

    void CreatedBuildState::start(BuildContext& ctx) {
        const std::string prev = ctx.build().status;
        ctx.mutableBuild().status = "Running";
        ctx.recordTransition(prev, "Running", "Build started");
        ctx.setState(std::make_unique<RunningBuildState>());
    }

    void CreatedBuildState::fail(BuildContext& ctx, const std::string& reason) {
        const std::string prev = ctx.build().status;
        ctx.mutableBuild().status = "Failed";
        ctx.recordTransition(prev, "Failed", reason.empty() ? "Failed before start" : reason);
        ctx.setState(std::make_unique<FailedBuildState>());
    }

    void RunningBuildState::complete(BuildContext& ctx, double finalDurationSeconds) {
        if (finalDurationSeconds < 0.0) {
            throw std::invalid_argument("Build duration cannot be negative.");
        }
        const std::string prev = ctx.build().status;
        ctx.mutableBuild().durationSeconds = finalDurationSeconds;
        ctx.mutableBuild().status = "Completed";
        ctx.recordTransition(prev, "Completed", "Build finished successfully");
        ctx.setState(std::make_unique<CompletedBuildState>());
    }

    void RunningBuildState::fail(BuildContext& ctx, const std::string& reason) {
        const std::string prev = ctx.build().status;
        ctx.mutableBuild().status = "Failed";
        ctx.recordTransition(prev, "Failed", reason.empty() ? "Build failed during execution" : reason);
        ctx.setState(std::make_unique<FailedBuildState>());
    }

} 

std::unique_ptr<IBuildState> makeStateFromStatus(const std::string& status) {
    if (status == "Created") {
        return std::make_unique<CreatedBuildState>();
    }
    if (status == "Running") {
        return std::make_unique<RunningBuildState>();
    }
    if (status == "Completed") {
        return std::make_unique<CompletedBuildState>();
    }
    if (status == "Failed") {
        return std::make_unique<FailedBuildState>();
    }
    throw std::invalid_argument("Unknown build status: " + status);
}

BuildContext::BuildContext(Build build)
    : build_(std::move(build)),
    state_(makeStateFromStatus(build_.status.empty() ? "Created" : build_.status)) {
    if (build_.status.empty()) {
        build_.status = "Created";
    }
}

const Build& BuildContext::build() const noexcept {
    return build_;
}

Build& BuildContext::mutableBuild() noexcept {
    return build_;
}

const IBuildState& BuildContext::currentState() const {
    return *state_;
}

void BuildContext::setState(std::unique_ptr<IBuildState> nextState) {
    state_ = std::move(nextState);
}

void BuildContext::recordTransition(const std::string& fromStatus,
    const std::string& toStatus,
    const std::string& reason) {
    hasPending_ = true;
    pending_ = PendingTransition{ fromStatus, toStatus, reason };
}

void BuildContext::start() {
    state_->start(*this);
}

void BuildContext::complete(double finalDurationSeconds) {
    state_->complete(*this, finalDurationSeconds);
}

void BuildContext::fail(const std::string& reason) {
    state_->fail(*this, reason);
}

bool BuildContext::hasPendingTransition() const noexcept {
    return hasPending_;
}

const BuildContext::PendingTransition& BuildContext::pendingTransition() const {
    return pending_;
}

void BuildContext::clearPendingTransition() noexcept {
    hasPending_ = false;
    pending_ = {};
}