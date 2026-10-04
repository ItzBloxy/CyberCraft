#include "Harness.h"

#include "Requests.h"

using cpmc::ParseRequest;
using cpmc::Request;

namespace
{
std::optional<Request> Parse(std::string_view aText)
{
	std::string error;
	return ParseRequest(aText, error);
}

std::string ErrorOf(std::string_view aText)
{
	std::string error;
	ParseRequest(aText, error);
	return error;
}
}

TEST(RequestsParseEachCommand)
{
	CHECK(Parse("log")->kind == Request::Kind::Log);
	CHECK(Parse("clear")->kind == Request::Kind::Clear);
	CHECK(Parse("reload")->kind == Request::Kind::Reload);

	const auto dump = Parse("dump gbuffer");
	CHECK(dump->kind == Request::Kind::Dump);
	CHECK_STR(dump->what, "gbuffer");

	const auto mode = Parse("mode present");
	CHECK(mode->kind == Request::Kind::Mode);
	CHECK_STR(mode->what, "present");

	const auto cube = Parse("place cube");
	CHECK(cube->kind == Request::Kind::Place);
	CHECK_STR(cube->what, "cube");

	const auto field = Parse("place field 100000");
	CHECK(field->kind == Request::Kind::Place);
	CHECK_STR(field->what, "field");
	CHECK_EQ(field->number, 100000.0);

	const auto set = Parse("set exposure 0.25");
	CHECK(set->kind == Request::Kind::Set);
	CHECK_STR(set->what, "exposure");
	CHECK_EQ(set->number, 0.25);
}

TEST(RequestsIgnoreSurroundingWhitespace)
{
	const auto mode = Parse("  mode\toff \r\n");
	CHECK(mode.has_value());
	CHECK_STR(mode->what, "off");
}

TEST(RequestsRejectBadInput)
{
	CHECK(!Parse(""));
	CHECK(!Parse("   \n"));
	CHECK(!Parse("log now"));
	CHECK(!Parse("dump everything"));
	CHECK(!Parse("mode on"));
	CHECK(!Parse("place tower"));
	CHECK(!Parse("place field"));
	CHECK(!Parse("place field 0"));
	CHECK(!Parse("place field 2.5"));
	CHECK(!Parse("place field 1000001"));
	CHECK(!Parse("set Exposure 1"));
	CHECK(!Parse("set exposure"));
	CHECK(!Parse("set exposure nan"));
	CHECK(!Parse("set exposure 1x"));
	CHECK(!Parse("Log"));
}

TEST(RequestsSayWhatWentWrong)
{
	CHECK_STR(ErrorOf(""), "the request is empty");
	CHECK_STR(ErrorOf("fly"), "unknown command \"fly\"");
	CHECK_STR(ErrorOf("mode on"), "mode takes one of: off, present, predlss, gbuffer");
	CHECK_STR(ErrorOf("place field 0"), "place field takes a whole number of blocks from 1 to 1000000");
}

TEST(SettingsReadDredAndDebugLayer)
{
	const cpmc::PluginSettings none = cpmc::ParseSettings("");
	CHECK(!none.dred && !none.debugLayer);

	const cpmc::PluginSettings both = cpmc::ParseSettings("# chasing a GPU crash\r\ndred = 1\r\ndebuglayer=1  # slow\r\n");
	CHECK(both.dred && both.debugLayer);

	const cpmc::PluginSettings off = cpmc::ParseSettings("dred = 0\ndebuglayer = yes\nother = 1\ndred\n= 1\n");
	CHECK(!off.dred && !off.debugLayer);
}

TEST(RequestsForTheLink)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("link on\n", error);
	CHECK(on.has_value());
	CHECK(on->kind == cpmc::Request::Kind::Link);
	CHECK_STR(on->what, "on");
	CHECK_STR(cpmc::ParseRequest("link off", error)->what, "off");
	CHECK(!cpmc::ParseRequest("link", error));
	CHECK(!cpmc::ParseRequest("link maybe", error));
	CHECK(cpmc::ParseRequest("floor", error)->kind == cpmc::Request::Kind::Floor);
	CHECK(!cpmc::ParseRequest("floor now", error));
}

TEST(RequestsForBuildMode)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("build on\n", error);
	CHECK(on.has_value());
	CHECK(on->kind == cpmc::Request::Kind::Build);
	CHECK_STR(on->what, "on");
	CHECK_STR(cpmc::ParseRequest("build off", error).value_or(cpmc::Request{}).what, "off");
	CHECK(!cpmc::ParseRequest("build", error));
	CHECK_STR(error, "build takes on or off");
	CHECK(!cpmc::ParseRequest("build on now", error));
}

TEST(RequestsForTheDriveLog)
{
	std::string error;
	const auto  ten = cpmc::ParseRequest("drivelog 10\n", error);
	CHECK(ten.has_value());
	CHECK(ten.value_or(cpmc::Request{}).kind == cpmc::Request::Kind::DriveLog);
	CHECK_EQ(ten.value_or(cpmc::Request{}).number, 10.0);
	CHECK_EQ(cpmc::ParseRequest("drivelog 2.5", error).value_or(cpmc::Request{}).number, 2.5);
	CHECK(cpmc::ParseRequest("drivelog 1", error).has_value());
	CHECK(cpmc::ParseRequest("drivelog 60", error).has_value());
	CHECK(!cpmc::ParseRequest("drivelog 0.5", error));
	CHECK(!cpmc::ParseRequest("drivelog 61", error));
	CHECK(!cpmc::ParseRequest("drivelog 10 now", error));
	CHECK(!cpmc::ParseRequest("drivelog", error));
	CHECK_STR(error, "drivelog takes a number of seconds from 1 to 60");
}

TEST(RequestsTypeIntoMinecraft)
{
	std::string error;
	const auto  set = cpmc::ParseRequest("mc tnt-cube\n", error);
	CHECK(set.has_value());
	CHECK(set->kind == cpmc::Request::Kind::Mc);
	CHECK_STR(set->what, "tnt-cube");
	// A command, verbatim to the end of the line, but for the spaces around it.
	const auto command = cpmc::ParseRequest("  mc /execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~ ~ stone \r\n", error);
	CHECK_STR(command->what, "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~ ~ stone");
	CHECK(!cpmc::ParseRequest("mc", error));
	CHECK(!cpmc::ParseRequest("mc explode", error));
	CHECK(error.find("neither a command nor a set") != std::string::npos);
	CHECK(!cpmc::ParseRequest("mc /say a\nb", error));  // one line only
	CHECK(error.find("printable ASCII") != std::string::npos);
}

TEST(RequestsForTheSceneWatch)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("scenewatch on", error);
	CHECK(on && on->kind == cpmc::Request::Kind::SceneWatch && on->what == "on");
	const auto off = cpmc::ParseRequest("scenewatch off\n", error);
	CHECK(off && off->kind == cpmc::Request::Kind::SceneWatch && off->what == "off");
	CHECK(!cpmc::ParseRequest("scenewatch", error));
	CHECK_STR(error, "scenewatch takes on or off");
}

TEST(RequestsForTheBodies)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("box on", error);
	CHECK(on && on->kind == cpmc::Request::Kind::Box && on->what == "on");
	const auto off = cpmc::ParseRequest("box off", error);
	CHECK(off && off->kind == cpmc::Request::Kind::Box && off->what == "off");
	CHECK(!cpmc::ParseRequest("box", error));
	CHECK_STR(error, "box takes on or off");
	const auto blocks = cpmc::ParseRequest("set blocks 1", error);
	CHECK(blocks && blocks->kind == cpmc::Request::Kind::Set && blocks->what == "blocks" && blocks->number == 1.0);
}

TEST(RequestsForTheGround)
{
	std::string error;
	const auto  city = cpmc::ParseRequest("set ground city", error);
	CHECK(city && city->kind == cpmc::Request::Kind::Set && city->what == "ground" && city->number == 1.0);
	const auto floor = cpmc::ParseRequest("set ground floor", error);
	CHECK(floor && floor->what == "ground" && floor->number == 0.0);
	CHECK(!cpmc::ParseRequest("set ground 1", error));
	CHECK_STR(error, "set ground takes floor or city");
	const auto budget = cpmc::ParseRequest("set exportbudget 2.5", error);
	CHECK(budget && budget->kind == cpmc::Request::Kind::Set && budget->what == "exportbudget" && budget->number == 2.5);
	const auto shapes = cpmc::ParseRequest("shapes", error);
	CHECK(shapes && shapes->kind == cpmc::Request::Kind::Shapes);
	CHECK(!cpmc::ParseRequest("shapes all", error));
}
