include(../../../cmake/CPM.cmake)

CPMAddPackage(
  NAME glfw 
  GITHUB_REPOSITORY glfw/glfw
  GIT_TAG 3.4 
  PATCHES "glfw.patch"
  SYSTEM YES

  OPTIONS 
  "GLFW_BUILD_EXAMPLES OFF"
  "GLFW_BUILD_TESTS OFF"
  "GLFW_BUILD_DOCS OFF"
  "GLFW_INSTALL OFF"
)