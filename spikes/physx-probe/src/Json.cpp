#include "Json.h"

#include <cmath>
#include <format>

namespace cpmc
{
std::string EscapeJson(std::string_view aText)
{
	std::string out;
	out.reserve(aText.size());
	for (const char ch : aText)
	{
		const auto c = static_cast<unsigned char>(ch);
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				out += std::format("\\u{:04x}", unsigned(c));
			}
			else
			{
				out.push_back(ch);
			}
			break;
		}
	}
	return out;
}

JsonWriter::JsonWriter(bool aPretty)
	: pretty_(aPretty)
{
}

void JsonWriter::Newline()
{
	if (pretty_)
	{
		out_ += '\n';
		out_.append(scopeHasItems_.size() * 2, ' ');
	}
}

// Separates this item from the previous one in the current object or array.
void JsonWriter::BeforeItem()
{
	if (afterKey_)
	{
		afterKey_ = false;
		return;
	}
	if (!scopeHasItems_.empty())
	{
		if (scopeHasItems_.back())
		{
			out_ += ',';
		}
		scopeHasItems_.back() = true;
		Newline();
	}
}

void JsonWriter::Open(char aBracket)
{
	BeforeItem();
	out_ += aBracket;
	scopeHasItems_.push_back(false);
}

void JsonWriter::Close(char aBracket)
{
	const bool hadItems = scopeHasItems_.back();
	scopeHasItems_.pop_back();
	if (hadItems)
	{
		Newline();
	}
	out_ += aBracket;
}

JsonWriter& JsonWriter::BeginObject()
{
	Open('{');
	return *this;
}

JsonWriter& JsonWriter::EndObject()
{
	Close('}');
	return *this;
}

JsonWriter& JsonWriter::BeginArray()
{
	Open('[');
	return *this;
}

JsonWriter& JsonWriter::EndArray()
{
	Close(']');
	return *this;
}

JsonWriter& JsonWriter::Key(std::string_view aKey)
{
	BeforeItem();
	out_ += '"';
	out_ += EscapeJson(aKey);
	out_ += pretty_ ? "\": " : "\":";
	afterKey_ = true;
	return *this;
}

JsonWriter& JsonWriter::String(std::string_view aValue)
{
	BeforeItem();
	out_ += '"';
	out_ += EscapeJson(aValue);
	out_ += '"';
	return *this;
}

JsonWriter& JsonWriter::Number(double aValue)
{
	if (!std::isfinite(aValue))
	{
		return Null();
	}
	BeforeItem();
	out_ += std::format("{}", aValue);
	return *this;
}

JsonWriter& JsonWriter::Int(int64_t aValue)
{
	BeforeItem();
	out_ += std::format("{}", aValue);
	return *this;
}

JsonWriter& JsonWriter::UInt(uint64_t aValue)
{
	BeforeItem();
	out_ += std::format("{}", aValue);
	return *this;
}

JsonWriter& JsonWriter::Bool(bool aValue)
{
	BeforeItem();
	out_ += aValue ? "true" : "false";
	return *this;
}

JsonWriter& JsonWriter::Null()
{
	BeforeItem();
	out_ += "null";
	return *this;
}
}
