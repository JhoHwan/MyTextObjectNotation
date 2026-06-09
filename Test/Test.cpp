#include "mton.hpp"

#include <iostream>
#include <string>
#include <vector>

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

    void ToMton(mton::Writer& writer) const
    {
        writer.Value("IpAddress", IpAddress);
        writer.Value("Port", Port);
    }
};

struct PlayerConfig
{
    std::string Name;
    int MaxHp = 0;
    float MoveSpeed = 0.0f;
    bool IsEnabled = false;
    std::string Message;

    static bool FromMton(const mton::ValueRef& ref, PlayerConfig& out)
    {
        auto name = ref["Name"].As<std::string>();
        auto maxHp = ref["MaxHp"].As<int>();
        auto moveSpeed = ref["MoveSpeed"].As<float>();
        auto isEnabled = ref["IsEnabled"].As<bool>();
        auto message = ref["NestedSection"]["Message"].As<std::string>();

        if (!name || !maxHp || !moveSpeed || !isEnabled || !message)
            return false;

        out.Name = *name;
        out.MaxHp = *maxHp;
        out.MoveSpeed = *moveSpeed;
        out.IsEnabled = *isEnabled;
        out.Message = *message;
        return true;
    }

    void ToMton(mton::Writer& writer) const
    {
        writer.Value("Name", Name);
        writer.Value("MaxHp", MaxHp);
        writer.Value("MoveSpeed", MoveSpeed);
        writer.Value("IsEnabled", IsEnabled);
        writer.Value("Message", Message);
    }
};

struct GraphicsConfig
{
    double ScreenRatio = 0.0;

    static bool FromMton(const mton::ValueRef& ref, GraphicsConfig& out)
    {
        auto ratio = ref["ScreenRatio"].As<double>();
        if (!ratio)
            return false;

        out.ScreenRatio = *ratio;
        return true;
    }

    void ToMton(mton::Writer& writer) const
    {
        writer.Value("ScreenRatio", ScreenRatio);
    }
};

struct GameConfig
{
    ServerConfig Server;
    PlayerConfig Player;
    GraphicsConfig Graphics;
    std::vector<std::string> Tags;
    std::vector<int> Ports;

    void ToMton(mton::Writer& writer) const
    {
        writer.Section("ServerConfig", Server);
        writer.Section("PlayerConfig", Player);
        writer.Section("GraphicsConfig", Graphics);
        writer.Array("Tags", Tags);
        writer.Array("Ports", Ports);
    }
};

int main()
{
    mton::Object sample = mton::Parser::ParseFile("Sample.mton");
    if (!sample.IsValid())
    {
        std::string error;
        if (mton::Parser::GetError(error))
            std::cout << error << '\n';
        else
            std::cout << "Invalid MTON file\n";

        return 1;
    }

#ifdef _DEBUG
    std::cout << "---------- Debug Print ----------\n";
    sample.DebugPrint();
    std::cout << "---------------------------------\n\n";
#endif

    ServerConfig server = sample["ServerConfig"].As<ServerConfig>({});
    PlayerConfig player = sample["PlayerConfig"].As<PlayerConfig>({});
    GraphicsConfig graphics = sample["GraphicsConfig"].As<GraphicsConfig>({});

    std::vector<std::string> fruits = sample["StringArray"].As<std::vector<std::string>>({});
    std::vector<std::string> escapedValues = sample["EscapedStringArray"].As<std::vector<std::string>>({});

    if (!sample["MixedArray"].As<std::vector<int>>())
        std::cout << "MixedArray cannot be converted to vector<int>.\n";

    std::cout << "Server: " << server.IpAddress << ':' << server.Port << '\n';
    std::cout << "Player: " << player.Name << ", HP " << player.MaxHp << ", Speed " << player.MoveSpeed << '\n';
    std::cout << "Screen Ratio: " << graphics.ScreenRatio << '\n';

    std::cout << "Fruits:";
    for (const std::string& fruit : fruits)
        std::cout << ' ' << fruit;
    std::cout << '\n';

    std::cout << "Escaped Values:";
    for (const std::string& value : escapedValues)
        std::cout << " [" << value << ']';
    std::cout << '\n';

    GameConfig saveData;
    saveData.Server = server;
    saveData.Player = player;
    saveData.Graphics = graphics;
    saveData.Tags = { "Generated", "MTON", "Hello \"Writer\"" };
    saveData.Ports = { 7777, 8888, 9999 };

    if (mton::Serializer::SaveFile("Save.mton", saveData))
        std::cout << "Saved Save.mton\n";
    else
        std::cout << "Failed to save Save.mton\n";

    return 0;
}
