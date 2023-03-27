#pragma once

#include "vget_game_object.hpp"
#include "vget_window.hpp"

namespace vget
{
    class KeyboardMovementController
    {
    public:
        struct KeyMappings
        {
            int moveLeft = GLFW_KEY_A;
            int moveRight = GLFW_KEY_D;
            int moveForward = GLFW_KEY_W;
            int moveBackward = GLFW_KEY_S;
            int moveUp = GLFW_KEY_E;
            int moveDown = GLFW_KEY_Q;
            int lookLeft = GLFW_KEY_LEFT;
            int lookRight = GLFW_KEY_RIGHT;
            int lookUp = GLFW_KEY_UP;
            int lookDown = GLFW_KEY_DOWN;
            int mouseCamera = GLFW_MOUSE_BUTTON_RIGHT;
        };

        // gameObject - ссылка на контроллируемый объект
        void moveInPlaneXZ(GLFWwindow* window, float dt, VgetGameObject& gameObject);

        KeyMappings keys{};
        double halfWidth;
        double halfHeight;
        double xpos;
        double ypos;
        float moveSpeed{5.f};	// для настройки скорости перемещения
        float lookSpeed{2.f};  // для настройки скорости поворота
    };
}
