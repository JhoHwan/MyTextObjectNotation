#include "mton.hpp"
#include <iostream>

int main()
{
    mton::Object sample = mton::Parser::ParseFile("Sample.mton");

    if (!sample.IsValid())
    {
        std::cout << "Invalid config\n";
        return 1;
    }

#ifdef _DEBUG
    std::cout << "---------- Debug Print ----------" << std::endl;
    sample.DebugPrint();
    std::cout << "---------- Debug Print ----------" << std::endl << std::endl;
#endif

    // 값이 없거나 변환에 실패하면 기본값을 사용합니다.
    std::string ip = sample["ServerConfig"]["IpAddress"].As<std::string>("127.0.0.1");
    int port = sample["ServerConfig"]["Port"].As<int>(7777);
    bool enabled = sample["PlayerConfig"]["IsEnabled"].As<bool>(false);
    float speed = sample["PlayerConfig"]["MoveSpeed"].As<float>(1.0f);
    double ratio = sample["GraphicsConfig"]["ScreenRatio"].As<double>(1.0);

    // optional로 직접 성공 여부를 확인할 수도 있습니다.
    std::optional<int> maybeHp = sample["PlayerConfig"]["MaxHp"].As<int>();
    if (maybeHp.has_value())
    {
        std::cout << "hp: " << *maybeHp << '\n';
    }

    auto arrOpt = sample["IntArray"].AsArray<std::string>();
    if (arrOpt.has_value())
    {
        auto arr = *arrOpt;
        std::cout << "Array: [";
        for (size_t i = 0; i < arr.size(); i++)
        {
            std::cout << arr[i];

            if (i < arr.size() - 1) std::cout << ", ";
        }
        std::cout << "]\n";
    }

    std::cout << ip << ':' << port << '\n';
}