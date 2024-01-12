// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Data.h"
#include "Spreadsheet.h"

Spreadsheet* CreateSpreadsheet(
    const std::vector<Session>& sessions,
    const std::map<std::string,Node>& nodes,
    const std::vector<ColumnType>& columns, bool detailed,
    bool fullDate,
    bool showLogs);

void
SortSessions(
    const std::map<std::string,Session>& sessions,
    const std::map<std::string,Node>& nodes,
    std::vector<Session>& sortedSessions,
    const std::vector<ColumnType>& columns,
    const std::vector<bool>& directions);
