#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// Escapes quotes, backslashes and control characters; other bytes (UTF-8) pass through.
std::string EscapeJson(std::string_view aText);

// A small JSON writer. Pretty output indents by two spaces; compact output has no whitespace at all.
class JsonWriter
{
public:
	explicit JsonWriter(bool aPretty = true);

	JsonWriter& BeginObject();
	JsonWriter& EndObject();
	JsonWriter& BeginArray();
	JsonWriter& EndArray();
	JsonWriter& Key(std::string_view aKey);
	JsonWriter& String(std::string_view aValue);
	JsonWriter& Number(double aValue);  // NaN and infinities are written as null
	JsonWriter& Int(int64_t aValue);
	JsonWriter& UInt(uint64_t aValue);
	JsonWriter& Bool(bool aValue);
	JsonWriter& Null();

	const std::string& Str() const { return out_; }

private:
	void BeforeItem();
	void Open(char aBracket);
	void Close(char aBracket);
	void Newline();

	std::string       out_;
	std::vector<bool> scopeHasItems_;
	bool              afterKey_ = false;
	bool              pretty_;
};
}
