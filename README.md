# ConfigParser

C++로 만든 간단한 커스텀 설정 파일 파서입니다.

`ConfigParser`는 섹션과 키-값 구조를 가진 텍스트 파일을 읽어서 메모리에 저장합니다. 값은 내부적으로 문자열로 저장하고, 사용할 때 `ValueRef::As<T>()`를 통해 원하는 타입으로 변환해서 가져옵니다.

## 개발 환경

- 개발 도구: Visual Studio / MSVC
- 플랫폼 도구 집합: v145
- C++ 표준: C++20

## 주요 기능

- 섹션 기반 설정 파일 문법
- 중첩 섹션 지원
- `key: value;` 형태의 키-값 저장
- `//` 줄 주석 지원
- `/* ... */` 범위 주석 지원
- 공백이 포함된 문자열 값 지원
- 문자열 안의 `:`, `{`, `}`, `;` 같은 문법 문자 보호
- `\"` 이스케이프 따옴표 처리
- `std::optional<T>` 기반의 안전한 값 접근
- 기본값 반환 API 제공
- 파싱된 구조를 확인하는 `DebugPrint()` 제공

## 설정 파일 문법

```txt
Config:
{
    server_ip: "127.0.0.1";
    port: 7777;
}

section1:
{
    key: value;
    key2: "https://www.naver.com/";
    key3: 123;
    key4: true;
}

section2:
{
    key: value;
    key2: "string value : {\"Test\"};";
    key3: 123;
    key4: true;
}
```

## 문법 규칙

- 일반 값은 `key: value;` 형태로 작성합니다.
- 섹션은 `sectionName: { ... }` 형태로 작성합니다.
- 일반 값은 반드시 `;`로 끝나야 합니다.
- 섹션은 반드시 `}`로 닫아야 합니다.
- 문자열 밖의 공백과 탭은 무시됩니다.
- 문자열은 `"..."`로 감쌀 수 있습니다.
- 문자열 안에서는 공백과 `:`, `{`, `}`, `;` 같은 문자를 값으로 사용할 수 있습니다.
- 문자열 안에서 따옴표를 넣고 싶으면 `\"`를 사용합니다.
- 같은 섹션 안에서 중복 키는 허용하지 않습니다.

## 주석

줄 주석:

```txt
// comment
key: value; // comment
```

범위 주석:

```txt
section1: /* block comment */
{
    key: value;
}
```

문자열 안의 주석 기호는 주석으로 처리하지 않습니다.

```txt
url: "https://example.com";
text: "this is not /* a comment */";
```

## 기본 사용법

```cpp
#include "ConfigParser.hpp"
#include <iostream>

int main()
{
    ConfigParser::Config conf("config.conf");

    if (!conf.IsValid())
    {
        std::cout << "Invalid config\n";
        return 1;
    }

    std::string ip = conf["Config"]["server_ip"].As<std::string>("127.0.0.1");
    int port = conf["Config"]["port"].As<int>(7777);
    bool enabled = conf["section1"]["key4"].As<bool>(false);

    std::cout << ip << ':' << port << '\n';
}
```

## API

### 파싱 성공 여부 확인

```cpp
ConfigParser::Config conf("config.conf");

if (conf.IsValid())
{
    // 파싱 성공
}
```

### 중첩 값 접근

```cpp
auto value = conf["section1"]["key"];
```

없는 키에 접근하거나, 섹션이 아닌 값에 `operator[]`를 호출하면 invalid `ValueRef`가 반환됩니다.

```cpp
if (!conf["section1"]["missing"].IsValid())
{
    // 존재하지 않는 값
}
```

### optional 기반 변환

```cpp
auto port = conf["Config"]["port"].As<int>();

if (port.has_value())
{
    std::cout << *port << '\n';
}
```

### 기본값 사용

```cpp
int port = conf["Config"]["port"].As<int>(7777);
std::string ip = conf["Config"]["server_ip"].As<std::string>("0.0.0.0");
bool flag = conf["section1"]["key4"].As<bool>(false);
```

값이 없거나 타입 변환에 실패하면 전달한 기본값을 반환합니다.

## 지원 타입

현재 `ValueRef::As<T>()`가 지원하는 타입입니다.

- `std::string`
- `int`
- `float`
- `double`
- `bool`

현재 bool 값은 아래 문자열을 지원합니다.

```txt
true
True
false
False
```

## 디버그 출력

```cpp
conf.DebugPrint();
```

파싱된 섹션 트리를 `std::cout`으로 출력합니다.

## 메모리 소유권

이 프로젝트는 의도적으로 스마트 포인터 대신 raw pointer를 사용합니다.

- `ConfigParser::Config`는 루트 `Section`을 소유합니다.
- 각 `Section`은 자신의 자식 `Section`을 소유합니다.
- `Section::parent`는 소유하지 않는 포인터입니다.
- `Section::~Section()`은 자식 섹션을 재귀적으로 삭제합니다.
- `ConfigParser::Config`와 `Section`은 복사를 금지합니다.

## 현재 제한 사항

- 배열 파싱은 아직 구현하지 않았습니다.
- `Value`에는 `std::vector<std::string>` 타입이 준비되어 있지만 아직 API로 사용하지 않습니다.
- 파싱 실패 시 상세 에러 메시지는 아직 제공하지 않습니다.
- 숫자 변환은 `std::stoi`, `std::stof`, `std::stod`를 사용합니다.
- `123abc` 같은 부분 변환을 엄격하게 막는 검사는 아직 없습니다.

## 앞으로 추가하면 좋은 기능

- 배열 문법과 `AsArray<T>()`
- `ValueConverter<T>` 기반 타입 변환 분리
- `GetError()` 또는 에러 메시지 저장
- 줄/열 번호를 포함한 파싱 오류 보고
- const 접근 API
- bool 값 별칭 추가: `1`, `0`, `yes`, `no`, `on`, `off`


