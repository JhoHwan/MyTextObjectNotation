#pragma once

#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mton
{
    class Section;
    class ValueRef;
    class Object;
	class Parser;

    using Value = std::variant<std::monostate, std::string, std::vector<std::string>, class Section*>;

	class Object
	{
		friend class Parser;

	private:
		Object(Section* rootSection);

		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

	public:
		Object(Object&& other) noexcept;
		Object& operator=(Object&& other) noexcept;

		~Object();

		inline bool IsValid() const { return _root != nullptr; }

		void DebugPrint() const;

		ValueRef operator[](const std::string& key);

		void Clear();

	private:
		Section* _root;
	};

	class Section
	{
		friend class Parser;
		friend class Object;
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

	class Parser
	{
	private:
		Parser() = delete;

	public:
		static Object ParseFile(const std::filesystem::path& path);

	private:
		static void Trim(std::string& str);
		static std::string NormalizeText(const std::string& in);
		static void SkipUtf8Bom(std::ifstream& file);
		static bool RemoveComments(std::ifstream& file, std::string& output);
		static bool ParseFromString(const std::string& str, Section* root);
		static bool CheckKeyStringIsValid(const std::string& key);
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


namespace mton
{
	// ----------------- Config ----------------- //

	inline Object::Object(Section* rootSection = nullptr) : _root(rootSection)
	{}

	inline Object::Object(Object&& other) noexcept : _root(other._root)
	{
		other._root = nullptr;
	}

	inline Object& Object::operator=(Object&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}

		Clear();

		_root = other._root;
		other._root = nullptr;

		return *this;
	}

	inline Object::~Object()
	{
		Clear();
	}

	inline void Object::DebugPrint() const
	{
		if (!IsValid()) return;
		_root->DebugPrint(0);
	}

	inline ValueRef Object::operator[](const std::string& key)
	{
		if (!IsValid() || !_root->map.contains(key))
			return ValueRef(nullptr);

		return ValueRef(&_root->map[key]);
	}

	inline void Object::Clear()
	{
		if (_root == nullptr) return;

		delete _root;
		_root = nullptr;
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

	// ----------------- Parser ----------------- //
	inline Object Parser::ParseFile(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
			return {};

		// conf 파일 인코딩이 UTF-8 With BOM인 경우
		SkipUtf8Bom(file);

		std::string output;
		bool res = RemoveComments(file, output);
		if (!res)
		{
			return{};
		}

		output = NormalizeText(output);

		Section* root = new Section();
		res = ParseFromString(output, root);
		if (!res)
		{
			delete root;
			return {};
		}

		return Object(root);
	}

	inline void Parser::SkipUtf8Bom(std::ifstream& file)
	{
		char bom[3];
		file.read(bom, 3);
		if (file.gcount() == 3 &&
			static_cast<unsigned char>(bom[0]) == 0xEF &&
			static_cast<unsigned char>(bom[1]) == 0xBB &&
			static_cast<unsigned char>(bom[2]) == 0xBF)
		{
			return;
		}

		file.clear();
		file.seekg(0, std::ios::beg);
	}

	inline bool Parser::RemoveComments(std::ifstream& file, std::string& output)
	{
		bool inString = false;
		bool inCommentBlock = false;

		std::string line;
		while (std::getline(file, line))
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

	inline bool Parser::ParseFromString(const std::string& str, Section* root)
	{
		bool isKey = true;
		bool hasValueType = false;
		bool inString = false;
		bool keyEndedBySpace = false;

		Section* curSection = root;

		std::string key;
		std::string value;

		for (size_t i = 0; i < str.size(); i++)
		{
			char c = str[i];
			char next = i + 1 < str.size() ? str[i + 1] : '\0';

			if (c == '\n')
			{
				if (!isKey && hasValueType)
					return false;

				continue;
			}

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
				if (!isKey || key.empty() || !CheckKeyStringIsValid(key) || curSection->map.contains(key))
				{
					return false;
				}

				isKey = false;
				hasValueType = false;
				keyEndedBySpace = false;
				continue;
			}

			if (isKey)
			{
				if (c == ' ')
				{
					if (!key.empty())  keyEndedBySpace = true;
				}
				else
				{
					if (keyEndedBySpace) return false;
					key += c;
				}

				continue;
			}

			if (!hasValueType)
			{
				if (c == ' ') continue;

				// Section Type
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

				// String Type

				// String 모드 시작
				if (c == '"')
				{
					inString = true;
					hasValueType = true;
					continue; // "는 value에 미포함
				}

				hasValueType = true;

			}

			if (c == ';')
			{
				if (!hasValueType) return false;

				curSection->map[key] = value;
				isKey = true;
				hasValueType = false;

				value.clear();
				key.clear();

				continue;
			}

			if (c == ' ')
			{
				return false;
			}
			value += c;
		}

		if (curSection != root)
		{
			return false;
		}

		if (!isKey || inString || !key.empty() || !value.empty())
			return false;

		return true;
	}

	inline bool Parser::CheckKeyStringIsValid(const std::string& key)
	{
		for (char c : key)
		{
			if (c == ' ') return false;
			if (c == '_') continue;

			if (!std::isalnum(c)) return false;
		}

		return true;
	}

	inline void Parser::Trim(std::string& str)
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

	inline std::string Parser::NormalizeText(const std::string& in)
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

			if (in[i] == '\t') out += ' ';
			else out += in[i];
		}

		return out;
	}
	// ----------------- Parser ----------------- //

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

		// Section, Array, Monostate일 때는 값 변환 불가
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
		else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			T value;

			auto result = std::from_chars(str.data(), str.data() + str.size(), value);
			if (result.ec != std::errc() || result.ptr != str.data() + str.size())
			{
				return std::nullopt;
			}

			return value;
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