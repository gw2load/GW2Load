#include <d3d11.h>
#include <span>
#include <iostream>
#include <cassert>

#include "api.h"

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cout << "Wrong number of arguments" << std::endl;
        return 1;
    }

    std::cout << "Enumerating addons in directory '" << argv[1] << "'" << std::endl;

    const auto addons = [&] {
        unsigned int count;
        auto* addons = GW2Load_GetAddonsInDirectory(argv[1], &count, nullptr);
        return std::span{ addons, count };
    }();

    for (auto& a : addons)
    {
        std::cout << a.name << ": " << a.path << std::endl;
        bool check = GW2Load_CheckIfAddon(a.path);
        assert(check);
        if (!check)
            std::cout << "\tError: addon check failed" << std::endl;
    }

    return 0;
}