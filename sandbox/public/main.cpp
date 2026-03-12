#include <GLFW/glfw3.h>
#include <imgui.h>
#include <nfd.h>
#include <stb_image/stb_image.h>

#include <print>

int main() {
  {
    int v1, v2, v3;
    glfwGetVersion(&v1, &v2, &v3);
    std::println("GLFW Version: {}.{}.{}", v1, v2, v3);
  }
  {
    const char* version = ImGui::GetVersion();
    std::println("ImGui Version: {}", version);
  }
  
  return 0;
}
