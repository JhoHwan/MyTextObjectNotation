#include "ConfigParser.hpp"
#include <iostream>

int main()
{
    ConfigParser::Config conf("Sample.conf");

#ifdef _DEBUG
    std::cout << "---------- Debug Print ----------" << std::endl;
    conf.DebugPrint();
    std::cout << "---------- Debug Print ----------" << std::endl << std::endl;
#endif

    if (!conf.IsValid())
    {
        std::cout << "Invalid config\n";
        return 1;
    }

    // 값이 없거나 변환에 실패하면 기본값을 사용합니다.
    std::string ip = conf["ServerConfig"]["IpAddress"].As<std::string>("127.0.0.1");
    int port = conf["ServerConfig"]["Port"].As<int>(7777);
    bool enabled = conf["PlayerConfig"]["IsEnabled"].As<bool>(false);
    float speed = conf["PlayerConfig"]["MoveSpeed"].As<float>(1.0f);
    double ratio = conf["GraphicsConfig"]["ScreenRatio"].As<double>(1.0);

    // optional로 직접 성공 여부를 확인할 수도 있습니다.
    std::optional<int> maybeHp = conf["PlayerConfig"]["MaxHp"].As<int>();
    if (maybeHp.has_value())
    {
        std::cout << "hp: " << *maybeHp << '\n';
    }

    std::cout << ip << ':' << port << '\n';
}