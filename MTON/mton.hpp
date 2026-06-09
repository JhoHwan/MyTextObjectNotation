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
	class Writer;

	template <typename T>
	inline constexpr bool always_false_v = false;

	// As<std::vector<T>>() 처리를 위해 std::vector인지 판별하는 타입 특성입니다.
	template <typename T>
	struct is_std_vector : public std::false_type {};

	template <typename T, typename Alloc>
	struct is_std_vector<std::vector<T, Alloc>> : public std::true_type
	{
		using value_type = T;
	};

	// 전역 FromMton(const ValueRef&, T&) 함수가 있으면 사용자 타입으로 읽을 수 있습니다.
	template <typename T>
	concept MtonReadable = requires(const ValueRef & ref, T & out)
	{
		{ FromMton(ref, out) } -> std::convertible_to<bool>;
	};

	// static T::FromMton(const ValueRef&, T&) 함수가 있으면 사용자 타입으로 읽을 수 있습니다.
	template <typename T>
	concept MtonStaticReadable = requires(const ValueRef & ref, T & out)
	{
		{ T::FromMton(ref, out) } -> std::convertible_to<bool>;
	};

	// 전역 ToMton(Writer&, const T&) 함수가 있으면 사용자 타입을 저장할 수 있습니다.
	template <typename T>
	concept MtonWritable = requires(const T & object, Writer & writer)
	{
		{ ToMton(writer, object) };
	};

	// 멤버 object.ToMton(Writer&) 함수가 있으면 사용자 타입을 저장할 수 있습니다.
	template <typename T>
	concept MtonMemberWritable = requires(const T & object, Writer & writer)
	{
		{ object.ToMton(writer) };
	};

	// static T::ToMton(Writer&, const T&) 함수가 있으면 사용자 타입을 저장할 수 있습니다.
	template <typename T>
	concept MtonStaticWritable = requires(const T & object, Writer & writer)
	{
		{ T::ToMton(writer, object) };
	};

	// MTON 기본 숫자 타입입니다. const/reference가 붙어도 같은 숫자 타입으로 취급합니다.
	template <typename T>
	concept Number =
		[]<typename U = std::remove_cvref_t<T>>()
		{
			return std::is_same_v<U, int> || std::is_same_v<U, float> || std::is_same_v<U, double>;
		}();

	// MTON 값 저장 타입입니다. 일반 값과 배열 원소는 문자열로 저장합니다.
	using Value = std::variant<std::monostate, std::string, std::vector<std::string>, class Section*>;

	/// <summary>
	/// Parser가 만든 읽기 전용 MTON 문서입니다.
	/// </summary>
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

		/// <summary>
		/// 파싱에 성공한 Object인지 확인합니다.
		/// </summary>
		/// <returns>루트 Section을 가지고 있으면 true입니다.</returns>
		inline bool IsValid() const { return _root != nullptr; }

		/// <summary>
		/// 현재 Object를 MTON 형태로 std::cout에 출력합니다.
		/// </summary>
		void DebugPrint() const;

		/// <summary>
		/// 루트 Section에서 key에 해당하는 값을 찾습니다.
		/// </summary>
		/// <param name="key">찾을 key입니다.</param>
		/// <returns>값이 없으면 invalid ValueRef를 반환합니다.</returns>
		ValueRef operator[](const std::string& key) const;

	private:
		void Clear();

	private:
		Section* _root;
	};

	/// <summary>
	/// 내부 트리 노드입니다. 사용자는 Section을 직접 만들거나 소유하지 않습니다.
	/// </summary>
	class Section
	{
		friend class Parser;
		friend class Object;
		friend class ValueRef;
		friend class Writer;
		friend class Serializer;

	private:
		Section() = default;
		~Section();

		Section(const Section&) = delete;
		Section& operator=(const Section&) = delete;

	private:
		void SerializeTo(std::ostream& os, int indentation) const;

	private:
		Section* parent = nullptr;
		std::unordered_map<std::string, Value> map;
	};

	/// <summary>
	/// MTON 파일을 Object로 파싱하는 static 전용 클래스입니다.
	/// </summary>
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
		/// <summary>
		/// 파일을 읽어 MTON Object로 파싱합니다.
		/// </summary>
		/// <param name="path">읽을 MTON 파일 경로입니다.</param>
		/// <returns>성공하면 valid Object, 실패하면 invalid Object를 반환합니다.</returns>
		static Object ParseFile(const std::filesystem::path& path);

		/// <summary>
		/// 마지막 파싱 실패 메시지를 가져옵니다.
		/// </summary>
		/// <param name="errorMsg">"Line N: message" 형식의 메시지를 받을 문자열입니다.</param>
		/// <returns>저장된 오류가 있으면 true입니다.</returns>
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

	/// <summary>
	/// Object 내부 값에 대한 읽기 전용 참조입니다.
	/// </summary>
	class ValueRef
	{
	public:
		ValueRef(const Value* ref);
		~ValueRef() = default;

	public:
		/// <summary>
		/// 현재 값이 Section일 때 자식 key에 접근합니다.
		/// </summary>
		/// <param name="key">찾을 key입니다.</param>
		/// <returns>값이 없거나 Section이 아니면 invalid ValueRef를 반환합니다.</returns>
		ValueRef operator[](const std::string& key) const;

		/// <summary>
		/// 값을 원하는 타입으로 변환합니다.
		/// </summary>
		/// <typeparam name="T">std::string, int, float, double, bool, std::vector<T>, 사용자 타입을 지원합니다.</typeparam>
		/// <returns>변환에 성공하면 값, 실패하면 std::nullopt입니다.</returns>
		template <typename T>
		std::optional<T> As() const;

		/// <summary>
		/// 배열 값을 원하는 원소 타입의 vector로 변환합니다.
		/// </summary>
		/// <typeparam name="T">std::string, int, float, double, bool을 지원합니다.</typeparam>
		/// <returns>변환에 성공하면 vector, 실패하면 std::nullopt입니다.</returns>
		template <typename T>
		std::optional<std::vector<T>> AsArray() const;

		/// <summary>
		/// 값을 원하는 타입으로 변환하고, 실패하면 기본값을 반환합니다.
		/// </summary>
		/// <typeparam name="T">변환할 타입입니다.</typeparam>
		/// <param name="defaultVal">값이 없거나 변환 실패 시 반환할 기본값입니다.</param>
		/// <returns>변환 결과 또는 기본값입니다.</returns>
		template <typename T>
		T As(const T& defaultVal) const;

		/// <summary>
		/// 배열 값을 변환하고, 실패하면 기본 배열을 반환합니다.
		/// </summary>
		/// <typeparam name="T">배열 원소 타입입니다.</typeparam>
		/// <param name="defaultArr">값이 없거나 변환 실패 시 반환할 기본 배열입니다.</param>
		/// <returns>변환 결과 또는 기본 배열입니다.</returns>
		template <typename T>
		std::optional<std::vector<T>> AsArray(const std::vector<T>& defaultArr) const;

		/// <summary>
		/// 유효한 값을 가리키는지 확인합니다.
		/// </summary>
		/// <returns>참조하는 값이 있으면 true입니다.</returns>
		inline bool IsValid() const { return _value != nullptr; }

	private:
		const Value* _value;
	};

	/// <summary>
	/// 사용자 타입을 MTON 파일로 저장하는 static 전용 클래스입니다.
	/// </summary>
	class Serializer
	{
	private:
		Serializer() = delete;

	public:
		/// <summary>
		/// object를 MTON 파일로 저장합니다.
		/// </summary>
		/// <typeparam name="T">ToMton을 제공하는 사용자 타입입니다.</typeparam>
		/// <param name="path">저장할 파일 경로입니다.</param>
		/// <param name="object">저장할 객체입니다.</param>
		/// <returns>파일 저장에 성공하면 true입니다.</returns>
		template <typename T>
		static bool SaveFile(const std::filesystem::path& path, const T& object);
	};

	/// <summary>
	/// ToMton 함수 안에서 MTON 값을 작성하는 빌더입니다.
	/// </summary>
	class Writer
	{
		friend class Serializer;

	private:
		Writer(mton::Section* section);

	public:
		/// <summary>
		/// 숫자 값을 작성합니다.
		/// </summary>
		/// <typeparam name="T">int, float, double입니다.</typeparam>
		/// <param name="key">저장할 key입니다.</param>
		/// <param name="value">저장할 숫자 값입니다.</param>
		template <Number T>
		void Value(std::string_view key, T value);

		/// <summary>
		/// bool 값을 작성합니다.
		/// </summary>
		/// <param name="key">저장할 key입니다.</param>
		/// <param name="value">저장할 bool 값입니다.</param>
		void Value(std::string_view key, bool value);

		/// <summary>
		/// 문자열 값을 작성합니다. Serializer 출력 시 따옴표로 저장됩니다.
		/// </summary>
		/// <param name="key">저장할 key입니다.</param>
		/// <param name="value">저장할 문자열 값입니다.</param>
		void Value(std::string_view key, std::string value);

		/// <summary>
		/// 단순 값 배열을 작성합니다.
		/// </summary>
		/// <typeparam name="T">std::string, int, float, double, bool을 지원합니다.</typeparam>
		/// <param name="key">저장할 key입니다.</param>
		/// <param name="array">저장할 배열입니다.</param>
		template <typename T>
		void Array(const std::string& key, const std::vector<T>& array);

		/// <summary>
		/// 중첩 Section을 작성합니다.
		/// </summary>
		/// <typeparam name="T">ToMton을 제공하는 사용자 타입입니다.</typeparam>
		/// <param name="key">저장할 Section key입니다.</param>
		/// <param name="object">저장할 객체입니다.</param>
		template <typename T>
		void Section(const std::string& key, const T& object);

	private:
		mton::Section* _section;
	};
}

namespace mton
{
	// ----------------- Object ----------------- //
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
		_root->SerializeTo(std::cout, 0);
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
	// ----------------- Object ----------------- //

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

	inline void Section::SerializeTo(std::ostream& os, int indentation = 0) const
	{
		const char* indent = "\t";

		for (const auto& [key, value] : map)
		{
			for (int i = 0; i < indentation; i++)
			{
				os << indent;
			}

			os << key << ":";

			if (std::holds_alternative<Section*>(value))
			{
				os << "\n";

				for (int i = 0; i < indentation; ++i)
				{
					os << indent;
				}

				os << "{\n";

				std::get<Section*>(value)->SerializeTo(os, indentation + 1);

				for (int i = 0; i < indentation; ++i)
				{
					os << indent;
				}

				os << "}\n";
			}
			else if (std::holds_alternative<std::string>(value))
			{
				std::string str = std::get<std::string>(value);

				os << "\"" << str << "\";\n";
			}
			else if (auto arrPtr = std::get_if<std::vector<std::string>>(&value))
			{
				const std::vector<std::string>& arr = *arrPtr;
				os << '[';
				for (size_t i = 0; i < arr.size(); i++)
				{
					os << "\"" << arr[i] << "\"";

					if (i < arr.size() - 1) os << ", ";
				}
				os << "];\n";
			}
		}
	}
	// ----------------- Section ----------------- //

	// ----------------- Parser ----------------- //
	inline bool Parser::GetError(std::string& errorMsg)
	{
		if (!_lastError.has_value()) return false;
		errorMsg = std::format("Line {}: {}", _lastError->Line, _lastError->Message);
		return true;
	}

	inline void Parser::SetError(size_t line, const std::string& message)
	{
		_lastError = Error{ line, message };
	}

	inline Object Parser::ParseFile(const std::filesystem::path& path)
	{
		_lastError = std::nullopt;

		std::ifstream file(path);
		if (!file.is_open())
		{
			SetError(0, "Failed to open file.");
			return {};
		}

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
		else if constexpr (Number<T>)
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

	// ----------------- Writer ----------------- //
	inline Writer::Writer(mton::Section* section) : _section(section)
	{

	}

	template <Number T>
	inline void Writer::Value(std::string_view key, T value)
	{
		_section->map[std::string(key)] = std::format("{}", value);
	}


	inline void Writer::Value(std::string_view key, bool value)
	{
		_section->map[std::string(key)] = value ? "true" : "false";
	}

	inline void Writer::Value(std::string_view key, std::string value)
	{
		size_t pos = 0;
		while ((pos = value.find('"', pos)) != std::string::npos)
		{
			value.replace(pos, 1, "\\\"");
			pos += 2;
		}

		_section->map[std::string(key)] = value;
	}

	template<typename T>
	inline void Writer::Array(const std::string& key, const std::vector<T>& array)
	{
	if constexpr (std::is_same_v<T, std::string>)
	{
		std::vector<std::string> strArr;
		strArr.reserve(array.size());

		for (std::string value : array)
		{
			size_t pos = 0;
			while ((pos = value.find('"', pos)) != std::string::npos)
			{
				value.replace(pos, 1, "\\\"");
				pos += 2;
			}

			strArr.push_back(std::move(value));
		}

		_section->map[key] = std::move(strArr);
	}
		else if constexpr (Number<T>)
		{
			std::vector<std::string> strArr;
			strArr.reserve(array.size());

			for (const auto& item : array)
			{
				strArr.push_back(std::format("{}", item));
			}

			_section->map[key] = std::move(strArr);
		}
	}

	template<typename T>
	inline void Writer::Section(const std::string& key, const T& object)
	{
		if constexpr (MtonWritable<T>)
		{
			mton::Section* newSection = new mton::Section();
			_section->map[key] = newSection;

			Writer writer(newSection);
			ToMton(writer, object);
		}
		else if constexpr (MtonMemberWritable<T>)
		{
			mton::Section* newSection = new mton::Section();
			_section->map[key] = newSection;

			Writer writer(newSection);
			object.ToMton(writer);
		}
		else if constexpr (MtonStaticWritable<T>)
		{
			mton::Section* newSection = new mton::Section();
			_section->map[key] = newSection;

			Writer writer(newSection);
			T::ToMton(writer, object);
		}
	}
	// ----------------- Writer ----------------- //

	// ----------------- Serializer ----------------- //
	template <typename T>
	inline bool Serializer::SaveFile(const std::filesystem::path& path, const T& object)
	{
		std::ofstream file(path);
		if (!file.is_open())
		{
			return false;
		}

		Section* root = new Section();
		Writer writer(root);

		if constexpr (MtonWritable<T>)
		{
			ToMton(writer, object);
		}
		else if constexpr (MtonMemberWritable<T>)
		{
			object.ToMton(writer);
		}
		else if constexpr (MtonStaticWritable<T>)
		{
			T::ToMton(writer, object);
		}
		else
		{
			delete root;
			return false;
		}

		root->SerializeTo(file);
		delete root;

		file.flush();
		file.close();

		return true;
	}
}
