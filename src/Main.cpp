#include "apps/Application.hpp"

// std library
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        App app{};

        app.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
