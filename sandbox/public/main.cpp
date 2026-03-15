#include <GLFW/glfw3.h>
#include <imgui.h>
#include <nfd.h>
#include <stb_image/stb_image.h>
#include <testlib/pub.hpp>
#include <build/config.hpp>

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

  test::init();
  std::println("abs path: {}", ERAY_ABSOLUTE_BUILD_PATH);
  std::println("eray version: {}", ERAY_VERSION);
  
  return 0;
}
