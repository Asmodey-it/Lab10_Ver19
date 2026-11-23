#pragma once
#include <string>
#include <stdexcept>
#include <sqlite3.h>
#include <sqlite_orm/sqlite_orm.h>
#include "Project.h"
#include "Build.h"
#include "BuildStateHistory.h"

inline auto createStorage(const std::string& filename) {
    using namespace sqlite_orm;
    return make_storage(
        filename,
        make_table(
            "Projects",
            make_column("id", &Project::id, primary_key().autoincrement()),
            make_column("name", &Project::name, not_null()),
            make_column("repositoryUrl", &Project::repositoryUrl, not_null()),
            make_column("status", &Project::status, not_null())
        ),
        make_table(
            "Builds",
            make_column("id", &Build::id, primary_key().autoincrement()),
            make_column("projectId", &Build::projectId),
            make_column("buildDate", &Build::buildDate, not_null()),
            make_column("status", &Build::status, not_null()),
            make_column("durationSeconds", &Build::durationSeconds),
            foreign_key(&Build::projectId).references(&Project::id)
        ),
        make_table(
            "BuildStateHistory",
            make_column("id", &BuildStateHistory::id, primary_key().autoincrement()),
            make_column("buildId", &BuildStateHistory::buildId),
            make_column("oldStatus", &BuildStateHistory::oldStatus, not_null()),
            make_column("newStatus", &BuildStateHistory::newStatus, not_null()),
            make_column("changedAt", &BuildStateHistory::changedAt, not_null()),
            make_column("reason", &BuildStateHistory::reason, not_null()),
            foreign_key(&BuildStateHistory::buildId).references(&Build::id)
        )
    );
}