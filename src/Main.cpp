#include "apps/SceneEditorApp.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        SceneEditorApp app{};

        app.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
