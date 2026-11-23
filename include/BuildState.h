#pragma once
#include <memory>
#include <string>
#include "Build.h"

class BuildContext;

class IBuildState {
public:
    virtual ~IBuildState() = default;
    virtual std::string name() const = 0;

    virtual void start(BuildContext& ctx) = 0;
    virtual void complete(BuildContext& ctx, double finalDurationSeconds) = 0;
    virtual void fail(BuildContext& ctx, const std::string& reason) = 0;
};

class BuildContext {
public:
    explicit BuildContext(Build build);

    const Build& build() const noexcept;
    Build& mutableBuild() noexcept;
    const IBuildState& currentState() const;

    void setState(std::unique_ptr<IBuildState> nextState);
    void recordTransition(const std::string& fromStatus,
        const std::string& toStatus,
        const std::string& reason);

    void start();
    void complete(double finalDurationSeconds);
    void fail(const std::string& reason);

    struct PendingTransition {
        std::string oldStatus;
        std::string newStatus;
        std::string reason;
    };

    bool hasPendingTransition() const noexcept;
    const PendingTransition& pendingTransition() const;
    void clearPendingTransition() noexcept;

private:
    Build build_;
    std::unique_ptr<IBuildState> state_;
    bool hasPending_ = false;
    PendingTransition pending_{};
};

std::unique_ptr<IBuildState> makeStateFromStatus(const std::string& status);