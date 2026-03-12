include(CPM)

CPMAddPackage(
  NAME glfw 
  GITHUB_REPOSITORY glfw/glfw
  GIT_TAG 3.4 
  SYSTEM YES

  OPTIONS 
  "GLFW_BUILD_EXAMPLES OFF"
  "GLFW_BUILD_TESTS OFF"
  "GLFW_BUILD_DOCS OFF"
  "GLFW_INSTALL OFF"
)

add_library(third_party::glfw ALIAS glfw)
