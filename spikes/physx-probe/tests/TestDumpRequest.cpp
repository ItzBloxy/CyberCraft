#include "Harness.h"

#include "DumpRequest.h"

TEST(BlankRequestAsksForAPlainDump)
{
	const auto empty = cpmc::ParseDumpRequest("");
	CHECK(empty && !empty->physxOrigin);
	const auto blank = cpmc::ParseDumpRequest(" \r\n\t");
	CHECK(blank && !blank->physxOrigin);
}

TEST(RequestCanSayWherePhysxsOriginIs)
{
	const auto request = cpmc::ParseDumpRequest("origin -1200.5 3400 12.25\r\n");
	CHECK(request && request->physxOrigin);
	if (request && request->physxOrigin)
	{
		CHECK_EQ(request->physxOrigin->x, -1200.5);
		CHECK_EQ(request->physxOrigin->y, 3400.0);
		CHECK_EQ(request->physxOrigin->z, 12.25);
	}
}

TEST(RequestCanStartAndStopTheOriginWatch)
{
	const auto dump = cpmc::ParseDumpRequest("");
	CHECK(dump && dump->action == cpmc::DumpRequest::Action::Dump);
	const auto watch = cpmc::ParseDumpRequest("watch\r\n");
	CHECK(watch && watch->action == cpmc::DumpRequest::Action::StartWatch && !watch->physxOrigin);
	const auto stop = cpmc::ParseDumpRequest("stop");
	CHECK(stop && stop->action == cpmc::DumpRequest::Action::StopWatch);
	CHECK(!cpmc::ParseDumpRequest("watch now"));
}

TEST(MalformedRequestsAreRejected)
{
	CHECK(!cpmc::ParseDumpRequest("origin 1 2"));
	CHECK(!cpmc::ParseDumpRequest("origin 1 2 3 4"));
	CHECK(!cpmc::ParseDumpRequest("origin 1 2 3x"));
	CHECK(!cpmc::ParseDumpRequest("origin 1 2 nan"));
	CHECK(!cpmc::ParseDumpRequest("offset 1 2 3"));
	CHECK(!cpmc::ParseDumpRequest("dump"));
}
