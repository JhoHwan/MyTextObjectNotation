#include <iostream>

#include "ConfigParser.hpp"

using ConfigParser::Config;

int main()
{
    Config config("Config.conf");

    config.DebugPrint();
}
