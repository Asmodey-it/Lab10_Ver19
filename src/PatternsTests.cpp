#include "PatternsTests.h"
#include "BuildCommand.h"
#include "BuildDurationStrategy.h"
#include "BuildObserver.h"
#include "BuildState.h"
#include "BuildWorkflowService.h"
#include "Database.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

    bool approxEqual(double a, double b, double eps = 1e-4) {
        return std::abs(a - b) <= eps;
    }

    void assertTrue(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error("Test assertion failed: " + message);
        }
    }

    void testStateMachineAndAtomicRollback() {
        const std::string testDb = "lab10_state_test.db";
        std::filesystem::remove(testDb);

        auto storage = createStorage(testDb);
        storage.on_open = [](sqlite3* db) {
            sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
            };
        storage.sync_schema();

        int projectId = storage.insert(Project{ 0, "CI Runner", "https://example.com/ci", "active" });
        int buildId = storage.insert(Build{ 0, projectId, "2026-11-23", "Created", 0.0 });

        BuildWorkflowService service(testDb);

        bool threwInvalidTransition = false;
        try {
            service.completeBuild(buildId, 120.0, "2026-11-23 10:00:00");
        }
        catch (const std::logic_error&) {
            threwInvalidTransition = true;
        }
        assertTrue(threwInvalidTransition, "Completing a Created build must throw std::logic_error");

        Build runningBuild = service.startBuild(buildId, "2026-11-23 10:01:00");
        assertTrue(runningBuild.status == "Running", "Build should transition to Running");

        Build completedBuild = service.completeBuild(buildId, 145.5, "2026-11-23 10:03:25");
        assertTrue(completedBuild.status == "Completed", "Build should transition to Completed");
        assertTrue(approxEqual(completedBuild.durationSeconds, 145.5), "Duration should be updated to 145.5");

        auto historyAfterSuccess = storage.get_all<BuildStateHistory>(
            sqlite_orm::where(sqlite_orm::c(&BuildStateHistory::buildId) == buildId)
        );
        assertTrue(historyAfterSuccess.size() == 2, "History should contain 2 transitions after start & complete");

        int buildId2 = storage.insert(Build{ 0, projectId, "2026-11-23", "Created", 0.0 });
        service.startBuild(buildId2, "2026-11-23 10:05:00");
        const std::size_t historyCountBeforeFail = storage.count<BuildStateHistory>();

        bool threwRollbackError = false;
        try {
            service.failBuild(buildId2, "Compiler timeout", "2026-11-23 10:06:00", true);
        }
        catch (const std::exception&) {
            threwRollbackError = true;
        }
        assertTrue(threwRollbackError, "Simulated FK failure in history must throw exception");

        auto buildAfterRollback = storage.get_optional<Build>(buildId2);
        assertTrue(buildAfterRollback.has_value(), "Build must still exist after rollback");
        assertTrue(buildAfterRollback->status == "Running",
            "Build status must remain Running after atomic transaction rollback");
        assertTrue(storage.count<BuildStateHistory>() == historyCountBeforeFail,
            "History count must not change after transaction rollback");

        std::filesystem::remove(testDb);
    }

    void testDurationStrategies() {
        BuildDurationMetrics metrics{ 150.0, 100.0, 25.0 };

        BuildDurationEvaluator evaluator(std::make_unique<AbsoluteDurationDeviationStrategy>());
        assertTrue(approxEqual(evaluator.evaluate(metrics), 50.0), "Absolute deviation should be 50.0");

        evaluator.setStrategy(std::make_unique<RelativeDurationDeviationStrategy>());
        assertTrue(approxEqual(evaluator.evaluate(metrics), 0.5), "Relative deviation should be 0.5");

        evaluator.setStrategy(std::make_unique<NormalizedDurationDeviationStrategy>());
        assertTrue(approxEqual(evaluator.evaluate(metrics), 2.0), "Normalized deviation should be 2.0");
    }

    void testObserverSubscriptionAndLifetime() {
        BuildEventPublisher publisher;
        auto logger = std::make_shared<ProjectBuildLogObserver>();
        auto stats = std::make_shared<BuildExecutionStatsObserver>();

        publisher.subscribe(logger);
        publisher.subscribe(stats);
        assertTrue(publisher.activeObserverCount() == 2, "Expected 2 active observers");

        publisher.notify(BuildEvent{
            BuildEventType::BuildStarted, 1, 10, "Running", 0.0, "2026-11-23 11:00:00"
            });
        publisher.notify(BuildEvent{
            BuildEventType::BuildCompleted, 1, 10, "Completed", 95.0, "2026-11-23 11:01:35"
            });

        assertTrue(stats->startedCount() == 1, "Stats should record 1 started build");
        assertTrue(stats->completedCount() == 1, "Stats should record 1 completed build");
        assertTrue(stats->successfulCount() == 1, "Stats should record 1 successful build");
        assertTrue(approxEqual(stats->totalCompletedDuration(), 95.0), "Total duration should be 95.0");
        assertTrue(logger->entries().size() == 2, "Logger should record 2 events");

        publisher.unsubscribe(logger);
        stats.reset();

        publisher.notify(BuildEvent{
            BuildEventType::BuildStarted, 1, 11, "Running", 0.0, "2026-11-23 11:05:00"
            });

        assertTrue(logger->entries().size() == 2, "Unsubscribed logger must not receive new events");
        assertTrue(publisher.activeObserverCount() == 0, "Expired/unsubscribed observers should be inactive");
    }

    void testCommandPattern() {
        const std::string cmdDb = "lab10_cmd_test.db";
        std::filesystem::remove(cmdDb);

        auto storage = createStorage(cmdDb);
        storage.on_open = [](sqlite3* db) {
            sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
            };
        storage.sync_schema();

        int projectId = storage.insert(Project{ 0, "Backend CI", "https://example.com/backend", "active" });
        int buildId = storage.insert(Build{ 0, projectId, "2026-11-23", "Created", 0.0 });

        BuildWorkflowService workflow(cmdDb);
        BuildEventPublisher publisher;
        auto logger = std::make_shared<ProjectBuildLogObserver>();
        auto stats = std::make_shared<BuildExecutionStatsObserver>();
        publisher.subscribe(logger);
        publisher.subscribe(stats);

        BuildDurationEvaluator evaluator(std::make_unique<NormalizedDurationDeviationStrategy>());
        BuildCommandExecutor executor;

        executor.run(std::make_unique<StartBuildCommand>(workflow, publisher, buildId, "2026-11-23 12:00:00"));
        executor.run(std::make_unique<CompleteBuildCommand>(
            workflow, publisher, &evaluator, BuildDurationMetrics{ 120.0, 100.0, 20.0 }, buildId, "2026-11-23 12:02:00"
        ));

        assertTrue(executor.executedCommands().size() == 2, "Executor should record 2 executed commands");
        assertTrue(stats->successfulCount() == 1, "Stats should show 1 successful build via Command");

        std::filesystem::remove(cmdDb);
    }

} 

void runAllPatternTests() {
    std::cout << "\n=== LAB 10 PATTERN UNIT TESTS ===\n";
    testStateMachineAndAtomicRollback();
    std::cout << "[PASS] State pattern + BuildStateHistory + atomic ROLLBACK\n";

    testDurationStrategies();
    std::cout << "[PASS] Strategy pattern (Absolute, Relative, Normalized)\n";

    testObserverSubscriptionAndLifetime();
    std::cout << "[PASS] Observer pattern (subscribe, unsubscribe, weak_ptr lifetime)\n";

    testCommandPattern();
    std::cout << "[PASS] Command pattern (StartBuildCommand, CompleteBuildCommand)\n";
    std::cout << "=== ALL LAB 10 TESTS PASSED ===\n";
}