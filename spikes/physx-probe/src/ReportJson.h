#pragma once

#include "Report.h"

#include <string>

namespace cpmc
{
// report.json for one dump, following the spec's report schema (pretty-printed).
std::string ReportToJson(const DumpReport& aReport);
}
