#include "Harness.h"

#include "Json.h"

#include <limits>

using cpmc::EscapeJson;
using cpmc::JsonWriter;

TEST(EscapesQuotesBackslashesAndControlCharacters)
{
	CHECK_STR(EscapeJson("say \"hi\""), "say \\\"hi\\\"");
	CHECK_STR(EscapeJson("C:\\Games"), "C:\\\\Games");
	CHECK_STR(EscapeJson("a\nb\tc"), "a\\nb\\tc");
	CHECK_STR(EscapeJson(std::string("x\x01y", 3)), "x\\u0001y");
	CHECK_STR(EscapeJson("Ñight City"), "Ñight City");  // UTF-8 passes through
}

TEST(CompactOutputHasNoWhitespace)
{
	JsonWriter json(false);
	json.BeginObject();
	json.Key("a").Int(1);
	json.Key("b").BeginArray().Bool(true).Null().Number(1.5).EndArray();
	json.Key("c").BeginObject().EndObject();
	json.Key("d").String("x");
	json.EndObject();
	CHECK_STR(json.Str(), R"({"a":1,"b":[true,null,1.5],"c":{},"d":"x"})");
}

TEST(PrettyOutputIndentsByTwoSpaces)
{
	JsonWriter json;
	json.BeginObject();
	json.Key("a").Int(1);
	json.Key("b").BeginArray().UInt(2).EndArray();
	json.EndObject();
	CHECK_STR(json.Str(), "{\n  \"a\": 1,\n  \"b\": [\n    2\n  ]\n}");
}

TEST(NonFiniteNumbersBecomeNull)
{
	JsonWriter json(false);
	json.BeginArray().Number(std::numeric_limits<double>::quiet_NaN()).Number(std::numeric_limits<double>::infinity()).EndArray();
	CHECK_STR(json.Str(), "[null,null]");
}
