# MTON

<strong>MTON(My Text Object Notation)</strong>은 C++에서 사용할 수 있는 간단한 텍스트 기반 오브젝트 표기 형식입니다.

`MTON`은 섹션과 키-값 구조를 가진 텍스트 파일을 읽어서 메모리에 저장합니다. 값은 내부적으로 문자열로 저장하고, 사용할 때 `ValueRef::As<T>()`를 통해 원하는 타입으로 변환해서 가져옵니다.

## 개발 환경

- 개발 도구: Visual Studio / MSVC
- 플랫폼 도구 집합: v145
- C++ 표준: C++20
- 배포 형태: `mton.hpp` 단일 헤더

## 주요 기능

- 섹션 기반 텍스트 오브젝트 문법
- 중첩 섹션 지원
- `Key: Value;` 형태의 키-값 저장
- `//` 줄 주석 지원
- `/* ... */` 범위 주석 지원
- UTF-8 with BOM 파일 처리
- 공백이 포함된 문자열 값 지원
- 문자열 안의 `:`, `{`, `}`, `;` 같은 문법 문자 보호
- `\"` 이스케이프 따옴표 처리
- `std::optional<T>` 기반의 안전한 값 접근
- 기본값 반환 API 제공
- 파싱된 구조를 확인하는 `DebugPrint()` 제공

## MTON 문법 예시

```txt
URL: "http://google.com";

ServerConfig:
{
    IpAddress: "127.0.0.1";
    Port: 7777;
}

PlayerConfig:
{
    Name: "Alice";
    MaxHp: 100;
    MoveSpeed: 3.5;
    IsEnabled: true;

    NestedSection:
    {
        Key: value;
        Message: "string value : {\"Test\"};";
    }
}

GraphicsConfig:
{
    ScreenRatio: 1.777;
}
```

## 문법 규칙

- 일반 값은 `Key: Value;` 형태로 작성합니다.
- 섹션은 `SectionName: { ... }` 형태로 작성합니다.
- 일반 값은 반드시 `;`로 끝나야 합니다.
- 섹션은 반드시 `}`로 닫아야 합니다.
- 문자열 밖의 공백과 탭은 무시됩니다.
- 문자열은 `"..."`로 감쌀 수 있습니다.
- 문자열 안에서는 공백과 `:`, `{`, `}`, `;` 같은 문자를 값으로 사용할 수 있습니다.
- 문자열 안에서 따옴표를 넣고 싶으면 `\"`를 사용합니다.
- 같은 섹션 안에서 중복 키는 허용하지 않습니다.
- 섹션 이름과 키 이름은 `PascalCase`를 권장합니다.

## 주석

줄 주석:

```txt
// comment
Key: value; // comment
```

범위 주석:

```txt
/*
    block
    comment
*/
SampleSection: /* block comment */
{
    Key: value;
}
```

문자열 안의 주석 기호는 주석으로 처리하지 않습니다.

```txt
URL: "https://example.com";
Text: "this is not /* a comment */";
```

## 기본 사용법

```cpp
#include "mton.hpp"
#include <iostream>

int main()
{
    mton::Object sample("Sample.mton");

#ifdef _DEBUG
    std::cout << "---------- Debug Print ----------" << std::endl;
    sample.DebugPrint();
    std::cout << "---------- Debug Print ----------" << std::endl << std::endl;
#endif

    if (!sample.IsValid())
    {
        std::cout << "Invalid MTON file\n";
        return 1;
    }

    std::string ip = sample["ServerConfig"]["IpAddress"].As<std::string>("127.0.0.1");
    int port = sample["ServerConfig"]["Port"].As<int>(7777);
    bool enabled = sample["PlayerConfig"]["IsEnabled"].As<bool>(false);
    float speed = sample["PlayerConfig"]["MoveSpeed"].As<float>(1.0f);
    double ratio = sample["GraphicsConfig"]["ScreenRatio"].As<double>(1.0);

    std::optional<int> maybeHp = sample["PlayerConfig"]["MaxHp"].As<int>();
    if (maybeHp.has_value())
    {
        std::cout << "hp: " << *maybeHp << '\n';
    }

    std::cout << ip << ':' << port << '\n';
}
```

## API

### 파싱 성공 여부 확인

```cpp
mton::Object sample("Sample.mton");

if (sample.IsValid())
{
    // 파싱 성공
}
```

### 중첩 값 접근

```cpp
auto value = sample["PlayerConfig"]["Name"];
```

없는 키에 접근하거나, 섹션이 아닌 값에 `operator[]`를 호출하면 invalid `ValueRef`가 반환됩니다.

```cpp
if (!sample["PlayerConfig"]["Missing"].IsValid())
{
    // 존재하지 않는 값
}
```

### optional 기반 변환

```cpp
auto port = sample["ServerConfig"]["Port"].As<int>();

if (port.has_value())
{
    std::cout << *port << '\n';
}
```

### 기본값 사용

```cpp
int port = sample["ServerConfig"]["Port"].As<int>(7777);
std::string ip = sample["ServerConfig"]["IpAddress"].As<std::string>("0.0.0.0");
bool flag = sample["PlayerConfig"]["IsEnabled"].As<bool>(false);
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
sample.DebugPrint();
```

파싱된 섹션 트리를 `std::cout`으로 출력합니다.

## 메모리 소유권

이 프로젝트는 의도적으로 스마트 포인터 대신 raw pointer를 사용합니다.

- `mton::Object`는 루트 `Section`을 소유합니다.
- 각 `Section`은 자신의 자식 `Section`을 소유합니다.
- `Section::parent`는 소유하지 않는 포인터입니다.
- `Section::~Section()`은 자식 섹션을 재귀적으로 삭제합니다.
- `mton::Object`와 `Section`은 복사를 금지합니다.

## 현재 제한 사항

- 배열 파싱은 아직 구현하지 않았습니다.
- `Value`에는 `std::vector<std::string>` 타입이 준비되어 있지만 아직 API로 사용하지 않습니다.
- 파싱 실패 시 상세 에러 메시지는 아직 제공하지 않습니다.
- 저장/직렬화 기능은 아직 구현하지 않았습니다.

## 앞으로 추가하면 좋은 기능

- 배열 문법과 `AsArray<T>()`
- `Parser`와 `Serializer` 역할 분리
- `ValueConverter<T>` 기반 타입 변환 분리
- `GetError()` 또는 에러 메시지 저장
- 줄/열 번호를 포함한 파싱 오류 보고
- const 접근 API
- bool 값 별칭 추가: `1`, `0`, `yes`, `no`, `on`, `off`
