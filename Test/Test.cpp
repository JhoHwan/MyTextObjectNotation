#include <iostream>

#include "ConfigParser.h"

int main()
{
    ConfigParser config("Config.conf");

    config.DebugPrint();
}
