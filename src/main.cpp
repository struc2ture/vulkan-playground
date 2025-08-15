#include <cstdio>

#include <imgui.h>
#include <GLFW/glfw3.h>

void setup_vulkan(ImVector<const char*> extensions)
{
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1000, 900, "Vulkan Playground", NULL, NULL);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
