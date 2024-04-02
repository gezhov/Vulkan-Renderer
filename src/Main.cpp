#include "apps/SceneEditorApp.hpp"
#include "apps/RMResearchApp.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try
    {
        if (argc > 1) {
            std::string argument_str(argv[1]);
            int argument_number = atoi(argv[2]);

            if (argument_str == "--scene") {
                SceneEditorApp app{argument_number};
                app.run();
            }
            else if (argument_str == "--rmresearch") {
                RMResearchApp app{argument_number};
                app.run();
            }
        }
        else {
            SceneEditorApp app{};
            app.run();
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
