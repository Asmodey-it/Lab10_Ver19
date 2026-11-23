#include "BuildWorkflowService.h"
#include "Database.h"
#include <stdexcept>

BuildWorkflowService::BuildWorkflowService(std::string dbPath)
    : dbPath_(std::move(dbPath)) {
}

namespace {

    template <typename TransitionFn>
    Build applyAtomicTransition(const std::string& dbPath,
        int buildId,
        const std::string& changedAt,
        TransitionFn&& transitionFn,
        bool simulateHistoryError = false) {
        auto storage = createStorage(dbPath);
        storage.on_open = [](sqlite3* db) {
            if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK) {
                throw std::runtime_error("Failed to enable PRAGMA foreign_keys");
            }
            };
        storage.sync_schema();

        auto existing = storage.get_optional<Build>(buildId);
        if (!existing) {
            throw std::runtime_error("Build not found with ID: " + std::to_string(buildId));
        }

        BuildContext ctx(*existing);
        transitionFn(ctx);

        if (!ctx.hasPendingTransition()) {
            throw std::logic_error("Transition did not produce a history record.");
        }

        const auto& tr = ctx.pendingTransition();

        bool committed = storage.transaction([&]() {
            storage.update(ctx.build());

            BuildStateHistory historyEntry{
                0,
                simulateHistoryError ? -1 : ctx.build().id, 
                tr.oldStatus,
                tr.newStatus,
                changedAt,
                tr.reason
            };
            storage.insert(historyEntry);
            return true;
            });

        if (!committed) {
            throw std::runtime_error("Transaction was not committed.");
        }

        ctx.clearPendingTransition();
        return ctx.build();
    }

} 

Build BuildWorkflowService::startBuild(int buildId, const std::string& changedAt) {
    return applyAtomicTransition(dbPath_, buildId, changedAt, [](BuildContext& ctx) {
        ctx.start();
        });
}

Build BuildWorkflowService::completeBuild(int buildId,
    double finalDurationSeconds,
    const std::string& changedAt) {
    return applyAtomicTransition(dbPath_, buildId, changedAt, [&](BuildContext& ctx) {
        ctx.complete(finalDurationSeconds);
        });
}

Build BuildWorkflowService::failBuild(int buildId,
    const std::string& reason,
    const std::string& changedAt,
    bool simulateHistoryError) {
    return applyAtomicTransition(
        dbPath_,
        buildId,
        changedAt,
        [&](BuildContext& ctx) { ctx.fail(reason); },
        simulateHistoryError
    );
}