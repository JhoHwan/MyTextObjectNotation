# MTON

<strong>MTON(My Text Object Notation)</strong>은 C++20에서 사용할 수 있는 간단한 텍스트 기반 오브젝트 표기 형식입니다.

`MTON`은 섹션, 키-값, 배열 구조를 가진 텍스트 파일을 읽고 쓸 수 있습니다. 값과 배열 원소는 내부적으로 문자열로 저장되며, 사용할 때 `ValueRef::As<T>()`, `ValueRef::As<std::vector<T>>()`, 또는 `ValueRef::AsArray<T>()`로 원하는 타입으로 변환합니다.

## 개발 환경

- 개발 도구: Visual Studio / MSVC
- 플랫폼 도구 집합: v145
- C++ 표준: C++20
- 배포 형태: `mton.hpp` 단일 헤더

## 주요 기능

- `Key: Value;` 형태의 키-값 문법
- `SectionName: { ... }` 형태의 중첩 섹션
- `[A, B, C]` 형태의 배열
- `//` 줄 주석과 `/* ... */` 범위 주석
- UTF-8 with BOM 파일 처리
- 문자열 내부 문법 문자 보호와 `\"` 이스케이프 처리
- `std::optional<T>` 기반 값 접근
- `As<std::vector<T>>()`와 `AsArray<T>()` 배열 접근
- 전역/static `FromMton` 기반 사용자 타입 읽기
- member/static/free `ToMton` 기반 사용자 타입 저장
- line 기반 파싱 오류 메시지
- `DebugPrint()`로 파싱 결과 확인

## MTON 문법 예시

```txt
RootKey: 1;

EmptyArray: [];
StringArray: ["Apple", "Banana", Cook];
IntArray: [1, 2, 3];
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
        Message: "string value : {\"Test\"};";
    }
}
```

## 문법 규칙

- 일반 값과 배열은 반드시 `;`로 끝납니다.
- 섹션은 반드시 `}`로 닫습니다.
- 문자열 밖에서 Key와 Value 내부 공백은 허용하지 않습니다.
- 문자열은 `"..."`로 감쌀 수 있습니다.
- 문자열 안에서는 공백과 `:`, `{`, `}`, `;`, `,`, `[`, `]` 같은 문자를 값으로 사용할 수 있습니다.
- 문자열 안에서 따옴표를 넣고 싶으면 `\"`를 사용합니다.
- 빈 배열 `[]`과 빈 문자열 원소 `""`을 사용할 수 있습니다.
- 같은 섹션 안에서 중복 키는 허용하지 않습니다.
- 섹션 이름과 키 이름은 `PascalCase`를 권장합니다.

## 기본 사용법

```cpp
mton::Object sample = mton::Parser::ParseFile("Sample.mton");
if (!sample.IsValid())
{
    std::string error;
    if (mton::Parser::GetError(error))
        std::cout << error << '\n';
    return 1;
}

std::string ip = sample["ServerConfig"]["IpAddress"].As<std::string>("127.0.0.1");
int port = sample["ServerConfig"]["Port"].As<int>(7777);
bool enabled = sample["PlayerConfig"]["IsEnabled"].As<bool>(false);
float speed = sample["PlayerConfig"]["MoveSpeed"].As<float>(1.0f);
```

## 배열 읽기

```cpp
auto names = sample["StringArray"].As<std::vector<std::string>>();
auto numbers = sample["IntArray"].As<std::vector<int>>();

std::vector<int> fallback = { 1, 2, 3 };
std::vector<int> ports = sample["MissingArray"].As<std::vector<int>>(fallback);
```

명시적으로 배열 API를 사용하고 싶으면 `AsArray<T>()`를 사용할 수 있습니다.

```cpp
auto names = sample["StringArray"].AsArray<std::string>();
auto numbers = sample["IntArray"].AsArray<int>();
```

배열이 아니거나 배열 원소 중 하나라도 변환에 실패하면 `std::nullopt`를 반환합니다.

## 사용자 타입 읽기

섹션을 구조체나 클래스로 직접 변환하려면 `FromMton`을 제공합니다.

```cpp
struct ServerConfig
{
    std::string IpAddress;
    int Port = 0;

    static bool FromMton(const mton::ValueRef& ref, ServerConfig& out)
    {
        auto ip = ref["IpAddress"].As<std::string>();
        auto port = ref["Port"].As<int>();

        if (!ip || !port)
            return false;

        out.IpAddress = *ip;
        out.Port = *port;
        return true;
    }
};

ServerConfig server = sample["ServerConfig"].As<ServerConfig>({});
```

전역 함수로도 정의할 수 있습니다.

```cpp
bool FromMton(const mton::ValueRef& ref, ServerConfig& out);
```

사용자 타입에 `FromMton`이 없으면 `As<T>()` 호출 시 컴파일 에러가 발생합니다.

## 사용자 타입 저장

저장은 `mton::Serializer::SaveFile()`과 `ToMton()`으로 처리합니다. 사용자는 `Section`을 직접 만들 필요 없이 `Writer`에 값만 추가하면 됩니다.

```cpp
struct ServerConfig
{
    std::string IpAddress;
    int Port = 0;

    void ToMton(mton::Writer& writer) const
    {
        writer.Value("IpAddress", IpAddress);
        writer.Value("Port", Port);
    }
};

ServerConfig server{ "127.0.0.1", 7777 };
mton::Serializer::SaveFile("ServerConfig.mton", server);
```

중첩 섹션과 배열도 `Writer`로 작성합니다.

```cpp
struct GameConfig
{
    ServerConfig Server;
    std::vector<std::string> Tags;
    std::vector<int> Ports;

    void ToMton(mton::Writer& writer) const
    {
        writer.Section("ServerConfig", Server);
        writer.Array("Tags", Tags);
        writer.Array("Ports", Ports);
    }
};
```

`ToMton`은 member, static, 전역 함수 방식으로 사용할 수 있습니다.

```cpp
void ToMton(mton::Writer& writer) const;
static void ToMton(mton::Writer& writer, const MyType& object);
void ToMton(mton::Writer& writer, const MyType& object);
```

Serializer는 문자열 값을 항상 따옴표로 감싸는 형태로 저장합니다.

## 오류 메시지

파싱 실패 시 `Parser::GetError()`로 마지막 오류 메시지를 가져올 수 있습니다.

```cpp
mton::Object obj = mton::Parser::ParseFile("Broken.mton");
if (!obj.IsValid())
{
    std::string error;
    if (mton::Parser::GetError(error))
        std::cout << error << '\n';
}
```

예시:

```txt
Line 12: Expected ';' after value.
```

## 지원 타입

`As<T>()`, `As<std::vector<T>>()`, `AsArray<T>()`, `Writer::Value()`, `Writer::Array()`가 기본 지원하는 타입입니다.

- `std::string`
- `int`
- `float`
- `double`
- `bool`
- 위 타입의 `std::vector<T>` 배열
- `FromMton` 또는 `ToMton`을 제공하는 사용자 타입

사용자 타입 배열은 지원하지 않습니다. 배열은 단순 값 배열만 지원합니다.

## 디버그 출력

```cpp
sample.DebugPrint();
```

파싱된 데이터를 MTON 형태로 `std::cout`에 출력합니다.

## 메모리 소유권

이 프로젝트는 의도적으로 스마트 포인터 대신 raw pointer를 사용합니다.

- `mton::Object`는 루트 `Section`을 소유합니다.
- 각 `Section`은 자신의 자식 `Section`을 소유합니다.
- `Section::parent`는 소유하지 않는 포인터입니다.
- `Section::~Section()`은 자식 섹션을 재귀적으로 삭제합니다.
- `mton::Object`와 `Section`은 복사를 금지합니다.
- `mton::Object`는 이동 생성과 이동 대입을 지원합니다.
- `mton::Object`와 `mton::ValueRef`는 읽기 전용 API만 제공합니다.

## 현재 제한 사항

- 오류 위치는 line만 제공합니다. column은 제공하지 않습니다.
- `std::wstring`은 지원하지 않습니다. UTF-8 기반 `std::string` 사용을 권장합니다.
- 사용자 타입 배열과 섹션 배열은 지원하지 않습니다.
- 저장 출력 순서는 내부 `std::unordered_map` 순서의 영향을 받을 수 있습니다.
