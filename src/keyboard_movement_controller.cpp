#include "keyboard_movement_controller.hpp"

ENGINE_BEGIN

void KeyboardMovementController::moveInPlaneXZ(GLFWwindow* window, float dt, VgetGameObject& gameObject)
{
	glm::vec3 rotate{0}; // хранит значение введённого поворота для объекта
	// Вектор поворота изменяет своё значение в зависимости от нажатой клавиши.
	// Функция glfwGetKey() возвращает флаг, была ли нажата клавиша в момент последнего опроса glfwPollEvents().
	if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.f;
	if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.f;
	if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.f;
	if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.f;

	if (glfwGetMouseButton(window, keys.mouseCamera) == GLFW_PRESS) {
		auto vgetWindow = reinterpret_cast<VgetWindow*>(glfwGetWindowUserPointer(window));
		halfWidth = static_cast<double>(vgetWindow->getExtent().width);
		halfHeight = static_cast<double>(vgetWindow->getExtent().height);

		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwGetCursorPos(window, &xpos, &ypos);
		glfwSetCursorPos(window, halfWidth, halfHeight);
		rotate.y -= halfWidth - xpos;
		rotate.x += halfHeight - ypos;
	}
	else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	// Проверка на то, чтобы вектора введённого поворота был больше нуля, так как
	// если он равен нулю, то не удастся осуществить нормализацию вектора, потому что
	// там будет браться квадратный корень из нуля.
	if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon())
	{
		// На игровой объект применяется поворот с учётом настройки скорости и временного шага кадра.
		// Вектор поворота нормализуется, чтобы поворот по диагонали (зажаты две кнопки поворота)
		// не был быстрее поворота по одной из осей. Нормализация делает длину любого вектора равной единице.
		gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
	}

	// Ограничение поворота тангажа в пределах примерно +/- 85 градусов
	gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
	// С помощью операции modulus значение поворота рыскания ограничивается значением 2pi, то есть полным оборотом в 360 градусов.
	// Это сделано для того, чтобы постоянное вращение в одном направлении не вызвало переполнение значения.
	gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

	float yaw = gameObject.transform.rotation.y;
	const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)}; // вектор направления "вперёд", в зависимости от того, куда "смотрит" объект
	const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x}; // вектор "вправо" так же в зависимости от того, куда направлен объект
	const glm::vec3 upDir{0.f, -1.f, 0.f}; // направление вверх

	glm::vec3 moveDir{0.f};
	if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir += forwardDir;
	if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir -= forwardDir;
	if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
	if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
	if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
	if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;

	if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon())
	{
		// На игровой объект применяется сдвиг с учётом настройки скорости и временного шага кадра.
		// Нормализация вектора смещения для случая движения сразу по нескольким осям.
		gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
	}
}

ENGINE_END
