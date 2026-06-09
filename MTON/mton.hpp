#pragma once

#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>
#include <concepts>

namespace mton
{
    class Section;
    class ValueRef;
    class Object;
	class Parser;
	class Serializer;

	template <typename T>
	inline constexpr bool always_false_v = false;

	template <typename T>
	struct is_std_vector : public std::false_type {};

	template <typename T, typename Alloc>
	struct is_std_vector<std::vector<T, Alloc>> : public std::true_type
	{
		using value_type = T;
	};

	template <typename T>
	concept MtonReadable = requires(const ValueRef & ref, T & out)
	{
		{ FromMton(ref, out) } -> std::convertible_to<bool>;
	};

	template <typename T>
	concept MtonStaticReadable = requires(const ValueRef & ref, T & out)
	{
		{ T::FromMton(ref, out) } -> std::convertible_to<bool>;
	};

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

		ValueRef operator[](const std::string& key) const;

	private:
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
		struct Error
		{
			size_t Line = 0;
			std::string Message;
		};

	private:
		Parser() = delete;

	public:
		static Object ParseFile(const std::filesystem::path& path);
		static bool GetError(std::string& errorMsg);
	private:
		static void Trim(std::string& str);
		static std::string NormalizeText(const std::string& in);
		static void SkipUtf8Bom(std::ifstream& file);
		static bool RemoveComments(std::ifstream& file, std::string& output);
		static bool ParseFromString(const std::string& str, Section* root);
		static bool CheckKeyStringIsValid(const std::string& key);
		static void SetError(size_t line, const std::string& message);

	private:
		inline static std::optional<Error> _lastError;
	};

	class ValueRef 
	{
	public:
		ValueRef(const Value* ref);
		~ValueRef() = default;

	public:
		ValueRef operator[](const std::string& key) const;

		template <typename T>
		std::optional<T> As() const;

		template <typename T>
		std::optional<std::vector<T>> AsArray() const;

		template <typename T>
		T As(const T& defaultVal) const;

		template <typename T>
		std::optional<std::vector<T>> AsArray(const std::vector<T>& defaultArr) const;

		inline bool IsValid() const { return _value != nullptr; }

	private:
		const Value* _value;
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

	inline ValueRef Object::operator[](const std::string& key) const
	{
		if (!IsValid()) return ValueRef(nullptr);

		auto it = _root->map.find(key);
		if (it == _root->map.end()) return ValueRef(nullptr);
		return ValueRef(&it->second);
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
			if (std::holds_alternative<Section*>(value))
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
			else if (auto arrPtr = std::get_if<std::vector<std::string>>(&value))
			{
				const std::vector<std::string>& arr = *arrPtr;
				std::cout << '[';
				for (size_t i = 0; i < arr.size(); i++)
				{
					if(arr[i].empty()) std::cout << "\"\"";
					else std::cout << arr[i];

					if (i < arr.size() - 1) std::cout << ", ";
				}
				std::cout << "]\n";
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
		size_t lineNumber = 0;
		size_t stringStartLine = 0;
		size_t commentStartLine = 0;

		std::string line;
		while (std::getline(file, line))
		{
			lineNumber++;
			Trim(line);

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
					stringStartLine = lineNumber;
					cleanedLine += '"';
					continue;
				}

				// 범위 주석 모드 시작
				if (curChar == '/' && nextChar == '*')
				{
					inCommentBlock = true;
					commentStartLine = lineNumber;
					i++;
					continue;
				}

				// 줄 주석 발견 시 현재 줄 스킵
				if (curChar == '/' && nextChar == '/') break;

				cleanedLine += curChar;
			}

			output += cleanedLine;
			output += '\n';
		}

		if (inString)
		{
			SetError(stringStartLine, "Unclosed string literal.");
			return false;
		}
		if (inCommentBlock)
		{
			SetError(commentStartLine, "Unclosed block comment.");
			return false;
		}

		return true;
	}
	inline bool Parser::ParseFromString(const std::string& str, Section* root)
	{
		bool isKey = true;
		bool hasValueType = false;
		bool stringMode = false;
		bool stringModeEnd = false;
		bool keyEndedBySpace = false;
		bool arrayMode = false;
		bool emptyArr = false;

		size_t lineNumber = 1;
		size_t stringStartLine = 0;

		Section* curSection = root;

		std::string key;
		std::string value;
		std::vector<std::string> arr;

		for (size_t i = 0; i < str.size(); i++)
		{
			char c = str[i];
			char next = i + 1 < str.size() ? str[i + 1] : '\0';

			if (c == '\n')
			{
				if (!isKey && hasValueType)
				{
					SetError(lineNumber, "Expected ';' after value.");
					return false;
				}

				lineNumber++;
				continue;
			}

			if (stringMode)
			{
				if (c == '\\' && next == '"')
				{
					value += '"';
					++i;
					continue;
				}

				if (c == '"')
				{
					stringMode = false;
					stringModeEnd = true;
					continue;
				}

				value += c;
				continue;
			}

			if (c == '}')
			{
				if (!isKey)
				{
					SetError(lineNumber, "Unexpected '}' while parsing value.");
					return false;
				}

				curSection = curSection->parent;

				if (curSection == nullptr)
				{
					SetError(lineNumber, "Unexpected closing brace.");
					return false;
				}

				isKey = true;
				hasValueType = false;

				value.clear();
				key.clear();

				continue;
			}

			if (c == ':')
			{
				if (!isKey)
				{
					SetError(lineNumber, "Unexpected ':' while parsing value.");
					return false;
				}
				if (key.empty())
				{
					SetError(lineNumber, "Missing key before ':'.");
					return false;
				}
				if (!CheckKeyStringIsValid(key))
				{
					SetError(lineNumber, "Invalid key. Only letters, digits, and '_' are allowed.");
					return false;
				}
				if (curSection->map.contains(key))
				{
					SetError(lineNumber, "Duplicated key.");
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
					if (!key.empty()) keyEndedBySpace = true;
				}
				else
				{
					if (keyEndedBySpace)
					{
						SetError(lineNumber, "Whitespace is not allowed inside key.");
						return false;
					}
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

				hasValueType = true;

				if (c == '[')
				{
					arrayMode = true;
					continue;
				}

				// String Type
			}

			// String 모드 시작
			if (c == '"')
			{
				stringMode = true;
				stringStartLine = lineNumber;
				continue; // "는 value에 미포함
			}

			if (c == ']' || c == ',')
			{
				if (!arrayMode)
				{
					SetError(lineNumber, "Unexpected array delimiter.");
					return false;
				}

				if (c == ']')
				{
					arrayMode = false;

					// 빈 배열 허용
					if (arr.empty() && value.empty() && !stringModeEnd)
					{
						emptyArr = true;
						continue;
					}
				}

				if (value.empty() && !stringModeEnd)
				{
					SetError(lineNumber, "Array element cannot be empty.");
					return false;
				}

				stringModeEnd = false;
				arr.emplace_back(std::move(value));
				value.clear();

				continue;
			}

			if (c == ';')
			{
				if (!hasValueType)
				{
					SetError(lineNumber, "Missing value before ';'.");
					return false;
				}
				if (arrayMode)
				{
					SetError(lineNumber, "Expected ']' before ';'.");
					return false;
				}

				if (!arr.empty() || emptyArr)
				{
					arrayMode = false;
					emptyArr = false;
					curSection->map[key] = std::move(arr);
					arr.clear();
				}
				else
				{
					curSection->map[key] = value;
				}

				isKey = true;
				hasValueType = false;
				stringModeEnd = false;

				value.clear();
				key.clear();

				continue;
			}

			if (stringModeEnd)
			{
				SetError(lineNumber, "Unexpected character after string literal.");
				return false;
			}

			if (arrayMode)
			{
				if (c == ' ') continue;
			}

			// String모드가 아닌 Value에 공백 불가
			if (c == ' ')
			{
				SetError(lineNumber, "Whitespace is not allowed outside string literal.");
				return false;
			}

			// Array모드 종료 후 값 추가 예외처리
			if (!arrayMode && !arr.empty())
			{
				SetError(lineNumber, "Unexpected character after array.");
				return false;
			}

			value += c;
		}

		if (curSection != root)
		{
			SetError(lineNumber, "Unclosed section.");
			return false;
		}

		if (stringMode)
		{
			SetError(stringStartLine, "Unclosed string literal.");
			return false;
		}
		if (!isKey)
		{
			SetError(lineNumber, "Expected ';' after value.");
			return false;
		}
		if (!key.empty())
		{
			SetError(lineNumber, "Expected ':' after key.");
			return false;
		}
		if (!value.empty())
		{
			SetError(lineNumber, "Expected ';' after value.");
			return false;
		}

		return true;
	}

	inline bool Parser::CheckKeyStringIsValid(const std::string& key)
	{
		for (char c : key)
		{
			if (c == ' ') return false;
			if (c == '_') continue;

			if (!std::isalnum(static_cast<unsigned char>(c))) return false;
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
	inline ValueRef::ValueRef(const Value* value) : _value(value)
	{

	}

	inline ValueRef ValueRef::operator[](const std::string& key) const
	{
		if (!IsValid()) return ValueRef(nullptr);

		Section* const * sectionPtr = get_if<Section*>(_value);
		if (!sectionPtr) return ValueRef(nullptr);

		const Section* section = *sectionPtr;
		if (!section->map.contains(key)) return ValueRef(nullptr);

		return ValueRef(&section->map.at(key));
	}

	template<typename T>
	inline std::optional<T> ValueRef::As() const
	{
		if (!IsValid()) return std::nullopt;

		const std::string* str = std::get_if<std::string>(_value);

		if constexpr (std::is_same_v<T, std::string>)
		{
			if (!str) return std::nullopt;
			return *str;
		}
		else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			if (!str) return std::nullopt;

			T value{};
			auto result = std::from_chars(str->data(), str->data() + str->size(), value);
			if (result.ec != std::errc() || result.ptr != str->data() + str->size())
				return std::nullopt;

			return value;
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			if (!str) return std::nullopt;

			if (*str == "true" || *str == "True") return true;
			if (*str == "false" || *str == "False") return false;

			return std::nullopt;
		}
		else if constexpr (MtonReadable<T>)
		{
			if (!std::holds_alternative<Section*>(*_value))
				return std::nullopt;

			T value{};
			if (FromMton(*this, value))
				return value;

			return std::nullopt;
		}
		else if constexpr (MtonStaticReadable<T>)
		{
			if (!std::holds_alternative<Section*>(*_value))
				return std::nullopt;

			T value{};
			if (T::FromMton(*this, value))
				return value;

			return std::nullopt;
		}
		else if constexpr (is_std_vector<T>::value)
		{
			using ElementType = typename is_std_vector<T>::value_type;
			return AsArray<ElementType>();
		}
		else
		{
			static_assert(
				always_false_v<T>,
				"mton::ValueRef::As<T>() requires FromMton(const mton::ValueRef&, T&) or T::FromMton(const mton::ValueRef&, T&) for custom types."
				);
		}
	}

	template<typename T>
	inline std::optional<std::vector<T>> ValueRef::AsArray() const
	{
		if (!IsValid()) return std::nullopt;

		auto arrPtr = std::get_if<std::vector<std::string>>(_value);
		if (!arrPtr) return std::nullopt;

		const std::vector<std::string>& arr = *arrPtr;
		if constexpr (std::is_same_v<T, std::string>)
		{
			return arr;
		}
		else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			std::vector<T> newArr;
			for (const auto& str : arr)
			{
				T value;
				auto result = std::from_chars(str.data(), str.data() + str.size(), value);
				if (result.ec != std::errc() || result.ptr != str.data() + str.size())
				{
					return std::nullopt;
				}
				newArr.push_back(value);
			}

			return newArr;
		}
		else if constexpr (std::is_same_v<T, bool>)
		{
			std::vector<bool> newArr;

			for (const auto& str : arr)
			{
				if (str == "true" || str == "True") newArr.push_back(true);
				else if (str == "False" || str == "false") newArr.push_back(false);
				else return std::nullopt;
			}

			return newArr;
		}
		return std::nullopt;
	}

	template<typename T>
	inline T ValueRef::As(const T& defaultVal) const
	{
		return As<T>().value_or(defaultVal);
	}
	template<typename T>
	inline std::optional<std::vector<T>> ValueRef::AsArray(const std::vector<T>& defaultArr) const
	{
		return AsArray<T>().value_or(defaultArr);
	}
	// ----------------- ValueRef ----------------- //
}