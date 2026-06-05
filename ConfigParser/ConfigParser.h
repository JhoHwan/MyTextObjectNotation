#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <variant>
#include <optional>
#include <vector>
#include <type_traits>

using Value = std::variant<std::monostate, std::string, std::vector<std::string> ,class Section*>;

class Section
{
	friend class ConfigParser;
	friend class ValueRef;
private:
	Section() = default;
	~Section();

	Section(const Section&) = delete;
	Section& operator=(const Section&) = delete;

	void DebugPrint(int indentation) const;

private:
	Section* parent = nullptr;
	std::unordered_map<std::string, Value> map;
};

class ValueRef
{
public:
	ValueRef(Value* ref);
	ValueRef operator[](const std::string& key);
	
	template <typename T>
	std::optional<T> As() const;

	template <typename T>
	T As(const T& defaultVal) const;

	inline bool IsValid() const { return _value != nullptr; }


private:
	Value* _value;
};

class ConfigParser
{
public:
	ConfigParser(const char* path);
	ConfigParser(const std::filesystem::path& path);

	ConfigParser(const ConfigParser&) = delete;
	ConfigParser& operator=(const ConfigParser&) = delete;

	~ConfigParser();

	inline bool IsValid() const { return _isValid; }

	void DebugPrint() const;

	ValueRef operator[](const std::string& key);

private:
	void Init();
	void Clear();

	bool ParseText();

	bool RemoveComments(std::string& output);

	static void Trim(std::string& str);
	static std::string NormalizeText(const std::string& in);
private:
	std::ifstream _file;
	bool _isValid;

	Section* _headSection;
};

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
