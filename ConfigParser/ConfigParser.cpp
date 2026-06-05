#include "ConfigParser.h"
#include <iostream>
#include <algorithm>
#include <stack>

using namespace std;

ConfigParser::ConfigParser(const char* path) : _file(path)
{
	Init();
}

ConfigParser::ConfigParser(const std::filesystem::path& path) : _file(path)
{
	Init();
}

ConfigParser::~ConfigParser()
{
	Clear();
}

void ConfigParser::DebugPrint() const
{
	if (!_isValid) return;
	_headSection->DebugPrint(0);
}

ValueRef ConfigParser::operator[](const string& key)
{
	if(!IsValid() || !_headSection->map.contains(key))
		return ValueRef(nullptr);
	
	return ValueRef(&_headSection->map[key]);
}

void ConfigParser::Init()
{
	_headSection = new Section();
	_isValid = ParseText();
	if (!_isValid)
	{
		Clear();
	}
}

void ConfigParser::Clear()
{
	if (_file.is_open())
	{
		_file.close();
	}

	if (_headSection == nullptr) return;
	
	delete _headSection;
	_headSection = nullptr;
}

bool ConfigParser::ParseText()
{
	if(!_file.is_open())
		return false;

	string output;
	bool res = RemoveComments(output);
	if (!res) return false;

	output = NormalizeText(output);

//	cout << output;

	bool isKey = true;
	bool hasValueType = false;
	bool inString = false;

	Section* curSection = _headSection;

	string key;
	string value;

	for (size_t i = 0; i < output.size(); i++)
	{
		char c = output[i];
		char next = i + 1 < output.size() ? output[i + 1] : '\0';

		if (c == '\n') continue;

		if (inString)
		{
			if (c == '\\' && next == '"')
			{
				value += '"';
				++i;
				continue;
			}

			if (c == '"')
			{
				inString = false;
				continue;
			}

			value += c;
			continue;
		}

		if (!isKey && c == '"')
		{
			inString = true;
			continue;
		}

		if (c == '}')
		{
			if (!isKey) return false;
			curSection = curSection->parent;

			if (curSection == nullptr) return false;

			isKey = true;
			hasValueType = false;

			value.clear();
			key.clear();

			continue;
		}


		if (c == ':')
		{
			if (!isKey || curSection->map.contains(key))
			{
				return false;
			}

			isKey = false;
			hasValueType = false;
			continue;
		}
		
		if(isKey)
		{
			key += c;
			continue;
		}

		if (!hasValueType)	
		{
			if (c == '{')
			{
				Section* newSection = new Section();
				curSection->map[key] = newSection;
				newSection->parent = curSection;
				curSection = newSection;

				key.clear();

				isKey = true;
				continue;
			}

			hasValueType = true;
		}

		if (c == ';')
		{
			curSection->map[key] = value;
			isKey = true;
			hasValueType = false;

			value.clear();
			key.clear();

			continue;
		}
		value += c;
		
	}

	if (curSection != _headSection)
	{
		return false;
	}

	if (!isKey || inString || !key.empty() || !value.empty())
		return false;

	return true;
}

bool ConfigParser::RemoveComments(std::string& output)
{
	bool inString = false;
	bool inCommentBlock = false;

	string line;
	while (std::getline(_file, line))
	{
		Trim(line);
		if (line.empty()) continue;

		string cleanedLine;
		for (size_t i = 0; i < line.size(); i++)
		{
			char curChar = line[i];
			char nextChar = i + 1 == line.size() ? '\0' : line[i + 1];

			// 범위 주석 모드 일 경우 */ 찾을 때 까지 스킵
			if (inCommentBlock)
			{
				if (curChar == '*' && nextChar == '/')
				{
					inCommentBlock = false;
					i++;
				}
				continue;
			}

			// String 모드
			if(inString)
			{
				if (curChar == '\\' && nextChar == '"')
				{
					i++;
					cleanedLine += "\\\"";
					continue;
				}

				if (curChar == '"')
				{
					inString = false;
				}

				cleanedLine += curChar;
				continue;
			}
			
			// 일반 상태
			if (curChar == '"')
			{
				inString = true;
				cleanedLine += '"';
				continue;
			}

			// 범위 주석 모드 시작
			if (curChar == '/' && nextChar == '*')
			{
				inCommentBlock = true;
				i++;
				continue;
			}

			// 줄 주석 발견 시 현재 줄 스킵
			if (curChar == '/' && nextChar == '/') break;
			

			cleanedLine += curChar;
		}

		if (cleanedLine.empty()) continue;
		output += cleanedLine;
		output += '\n';
	}

	if (inString)
	{
		cout << "\"가 닫히지 않음" << endl;
		return false;
	}
	if (inCommentBlock)
	{
		cout << "/*가 닫히지 않음" << endl;
		return false;
	}

	return true;
}

void ConfigParser::Trim(std::string& str)
{
	auto start = str.find_first_not_of(" \t");
	if (start == string::npos)
	{
		str = "";
		return;
	}

	auto end = str.find_last_not_of(" \t");
	str = str.substr(start, end - start + 1);
}

std::string ConfigParser::NormalizeText(const std::string& in)
{
	string out;

	bool inString = false;

	for (size_t i = 0; i < in.size(); i++)
	{		
		char cur = in[i];
		char next = i + 1 == in.size() ? '\0' : in[i + 1];

		if (cur == '\\' && next == '"')
		{
			out += "\\\"";
			i++;
			continue;
		}

		if (cur == '"')
		{
			inString = !inString;
		}

		if (inString)
		{
			out += in[i];
			continue;
		}

		if (in[i] == ' ' || in[i] == '\t') continue;
		
		out += in[i];
	}

	return out;
}

Section::~Section()
{
	for (auto& [_, value] : map)
	{
		if (holds_alternative<Section*>(value))
		{
			delete std::get<Section*>(value);
		}
	}
}

void Section::DebugPrint(int indentation) const
{
	for (auto& [key, value] : map)
	{
		for (int i = 0; i < indentation; i++)
		{
			cout << "  ";
		}
		cout << key <<":";

		if (holds_alternative<Section*>(value))
		{
			for (int i = 0; i < indentation; i++)
			{
				cout << "  ";
			}
			cout << "\n{\n";

			std::get<Section*>(value)->DebugPrint(indentation+1);
		}
		else if(holds_alternative<std::string>(value))
		{
			cout << std::get<string>(value) << '\n';
		}
	}

	for (int i = 0; i < indentation - 1; i++)
	{
		cout << "  ";
	}

	if(indentation != 0)
		cout << "}\n";
}

ValueRef::ValueRef(Value* value) : _value(value)
{

}

ValueRef ValueRef::operator[](const std::string& key)
{
	if(!IsValid()) return ValueRef(nullptr);

	Section** sectionPtr = get_if<Section*>(_value);
	if (!sectionPtr) return ValueRef(nullptr);

	Section* section = *sectionPtr;
	if (!section->map.contains(key)) return ValueRef(nullptr);

	return ValueRef(&section->map[key]);
}
