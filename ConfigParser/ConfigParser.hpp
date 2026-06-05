#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ConfigParser
{
    class Section;
    class ValueRef;
    class Config;

    using Value = std::variant<std::monostate, std::string, std::vector<std::string>, class Section*>;

	class Config
	{
	public:
		Config(const char* path);
		Config(const std::filesystem::path& path);

		Config(const Config&) = delete;
		Config& operator=(const Config&) = delete;

		~Config();

		inline bool IsValid() const { return _isValid; }

		void DebugPrint() const;

		ValueRef operator[](const std::string& key);

	private:
		void Init();
		void Clear();

		bool ParseText();
		void SkipUtf8Bom();

		bool RemoveComments(std::string& output);

		static void Trim(std::string& str);
		static std::string NormalizeText(const std::string& in);

	private:
		std::ifstream _file;
		bool _isValid;

		Section* _root;
	};

	class Section
	{
		friend class Config;
		friend class ValueRef;
	private:
		Section() = default;
		~Section();

		Section(const Section&) = delete;
		Section& operator=(const Section&) = delete;

	private:
		void DebugPrint(int indentation) const;

	private:
		Section* parent = nullptr;
		std::unordered_map<std::string, Value> map;
	};

	class ValueRef
	{
	public:
		ValueRef(Value* ref);
		~ValueRef() = default;

	public:
		ValueRef operator[](const std::string& key);

		template <typename T>
		std::optional<T> As() const;

		template <typename T>
		T As(const T& defaultVal) const;

		inline bool IsValid() const { return _value != nullptr; }

	private:
		Value* _value;
	};
}


namespace ConfigParser
{
	// ----------------- Config ----------------- //
	inline Config::Config(const char* path) : _file(path)
	{
		Init();
	}

	inline Config::Config(const std::filesystem::path& path) : _file(path)
	{
		Init();
	}

	inline Config::~Config()
	{
		Clear();
	}

	inline void Config::DebugPrint() const
	{
		if (!_isValid) return;
		_root->DebugPrint(0);
	}

	inline ValueRef Config::operator[](const std::string& key)
	{
		if (!IsValid() || !_root->map.contains(key))
			return ValueRef(nullptr);

		return ValueRef(&_root->map[key]);
	}

	inline void Config::Init()
	{
		_root = new Section();
		_isValid = ParseText();
		if (!_isValid)
		{
			Clear();
		}
	}

	inline void Config::Clear()
	{
		if (_file.is_open())
		{
			_file.close();
		}

		if (_root == nullptr) return;

		delete _root;
		_root = nullptr;
	}

	inline bool Config::ParseText()
	{
		if (!_file.is_open())
			return false;

		// conf 파일 인코딩이 UTF-8 With BOM인 경우
		SkipUtf8Bom();

		std::string output;
		bool res = RemoveComments(output);
		if (!res) return false;

		output = NormalizeText(output);

		//std::cout << output;

		bool isKey = true;
		bool hasValueType = false;
		bool inString = false;

		Section* curSection = _root;

		std::string key;
		std::string value;

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

			if (isKey)
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

		if (curSection != _root)
		{
			return false;
		}

		if (!isKey || inString || !key.empty() || !value.empty())
			return false;

		return true;
	}

	inline void Config::SkipUtf8Bom()
	{
		char bom[3];
		_file.read(bom, 3);
		if (_file.gcount() == 3 &&
			static_cast<unsigned char>(bom[0]) == 0xEF &&
			static_cast<unsigned char>(bom[1]) == 0xBB &&
			static_cast<unsigned char>(bom[2]) == 0xBF)
		{
			return;
		}

		_file.clear();
		_file.seekg(0, std::ios::beg);
	}

	inline bool Config::RemoveComments(std::string& output)
	{
		bool inString = false;
		bool inCommentBlock = false;

		std::string line;
		while (std::getline(_file, line))
		{
			Trim(line);
			if (line.empty()) continue;

			std::string cleanedLine;
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

				// std::string 모드
				if (inString)
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
			return false;
		}
		if (inCommentBlock)
		{
			return false;
		}

		return true;
	}

	inline void Config::Trim(std::string& str)
	{
		auto start = str.find_first_not_of(" \t");
		if (start == std::string::npos)
		{
			str = "";
			return;
		}

		auto end = str.find_last_not_of(" \t");
		str = str.substr(start, end - start + 1);
	}

	inline std::string Config::NormalizeText(const std::string& in)
	{
		std::string out;

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
	// ----------------- Config ----------------- //


	// ----------------- Section ----------------- //
	inline Section::~Section()
	{
		for (auto& [_, value] : map)
		{
			if (holds_alternative<Section*>(value))
			{
				delete std::get<Section*>(value);
			}
		}
	}

	inline void Section::DebugPrint(int indentation) const
	{
		const char* indent = "  ";

		for (const auto& [key, value] : map)
		{
			for (int i = 0; i < indentation; i++)
			{
				std::cout << indent;
			}

			std::cout << key << ":";

			if (std::holds_alternative<Section*>(value))
			{
				std::cout << "\n";

				for (int i = 0; i < indentation; ++i)
				{
					std::cout << indent;
				}

				std::cout << "{\n";

				std::get<Section*>(value)->DebugPrint(indentation + 1);

				for (int i = 0; i < indentation; ++i)
				{
					std::cout << indent;
				}

				std::cout << "}\n";
			}
			else if (std::holds_alternative<std::string>(value))
			{
				std::cout << std::get<std::string>(value) << "\n";
			}
		}
	}
	// ----------------- Section ----------------- //


	// ----------------- ValueRef ----------------- //
	inline ValueRef::ValueRef(Value* value) : _value(value)
	{

	}

	inline ValueRef ValueRef::operator[](const std::string& key)
	{
		if (!IsValid()) return ValueRef(nullptr);

		Section** sectionPtr = get_if<Section*>(_value);
		if (!sectionPtr) return ValueRef(nullptr);

		Section* section = *sectionPtr;
		if (!section->map.contains(key)) return ValueRef(nullptr);

		return ValueRef(&section->map[key]);
	}

	template<typename T>
	inline std::optional<T> ValueRef::As() const
	{
		if (!IsValid()) return std::nullopt;

		// Section, Array, Monostate일 때
		std::string* strPtr = std::get_if<std::string>(_value);
		if (strPtr == nullptr)
		{
			return std::nullopt;
		}

		const std::string& str = *strPtr;
		if constexpr (std::is_same_v<T, std::string>)
		{
			return str;
		}
		else if constexpr (std::is_same_v<T, int>)
		{
			try
			{
				return std::stoi(str);
			}
			catch (...)
			{
				return std::nullopt;
			}
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			try
			{
				return std::stof(str);
			}
			catch (...)
			{
				return std::nullopt;
			}
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			try
			{
				return std::stod(str);
			}
			catch (...)
			{
				return std::nullopt;
			}
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			if (str == "true" || str == "True") return true;
			else if (str == "False" || str == "false") return false;
			return std::nullopt;
		}

		return std::nullopt;
	}

	template<typename T>
	inline T ValueRef::As(const T& defaultVal) const
	{
		return As<T>().value_or(defaultVal);
	}
	// ----------------- ValueRef ----------------- //
}