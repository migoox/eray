include_guard(GLOBAL)
include(FetchContent)

if(NOT PATCHES_DIR)
  message(FATAL_ERROR "No patches dir set")
endif()

macro(loader_begin name)
  set(EXPORT_COMPILE_COMMANDS_TEMP ${CMAKE_EXPORT_COMPILE_COMMANDS})
  set(BUILD_SHARED_LIBS_TEMP ${BUILD_SHARED_LIBS})

  set(CMAKE_EXPORT_COMPILE_COMMANDS OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

  message(CHECK_START "Fetching ${name}")
  list(APPEND CMAKE_MESSAGE_INDENT "  ")
endmacro()

macro(loader_end)
  list(POP_BACK CMAKE_MESSAGE_INDENT)
  message(CHECK_PASS "fetched")

  set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_TEMP} CACHE BOOL "" FORCE)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ${EXPORT_COMPILE_COMMANDS_TEMP} CACHE BOOL "" FORCE)
endmacro()

# =============================================================================
# GLAD
# =============================================================================
function(fetch_glad)
  if(NOT TARGET glad)
    loader_begin("GLAD")

    FetchContent_Declare(
      glad
      GIT_REPOSITORY "https://github.com/Dav1dde/glad.git"
      GIT_TAG "v2.0.7"
      PATCH_COMMAND git apply --ignore-whitespace
                    "${PATCHES_DIR}/glad.patch"
      UPDATE_DISCONNECTED 1)
    FetchContent_MakeAvailable(glad)
    FetchContent_GetProperties(glad)

    add_subdirectory("${glad_SOURCE_DIR}/cmake" glad_cmake)
    glad_add_library(
      glad
      STATIC
      REPRODUCIBLE
      EXCLUDE_FROM_ALL
      LOADER
      API
      gl:core=4.6)

    loader_end()
  endif()
endfunction()

# =============================================================================
# GLFW
# =============================================================================
function(fetch_glfw)
  if(NOT TARGET glfw)
    loader_begin("GLFW")

    set(GLFW_BUILD_EXAMPLES
        OFF
        CACHE BOOL "Do not build GLFW example programs" FORCE)
    set(GLFW_BUILD_TESTS
        OFF
        CACHE BOOL "Do not build GLFW test programs" FORCE)
    set(GLFW_BUILD_DOCS
        OFF
        CACHE BOOL "Do not build GLFW documentation" FORCE)
    set(GLFW_INSTALL
        OFF
        CACHE BOOL "Do not install the GLFW" FORCE)

    FetchContent_Declare(
      glfw
      GIT_REPOSITORY "https://github.com/glfw/glfw.git"
      GIT_TAG "3.4"
      PATCH_COMMAND git apply --ignore-whitespace
                    "${PATCHES_DIR}/glfw.patch"
      UPDATE_DISCONNECTED 1)
    FetchContent_MakeAvailable(glfw)

    loader_end()
  endif()
endfunction()

# =============================================================================
# ImGui
# =============================================================================
function(fetch_imgui)
  if(NOT TARGET imgui)
    loader_begin("ImGui")

    FetchContent_Declare(
      imgui
      GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
      GIT_TAG "v1.91.1-docking"
      PATCH_COMMAND git apply --ignore-whitespace
                    "${PATCHES_DIR}/imgui.patch"
      UPDATE_DISCONNECTED 1)
    FetchContent_MakeAvailable(imgui)

    loader_end()
  endif()
endfunction()

# =============================================================================
# Native File Dialog Extended 
# =============================================================================
function(fetch_nfd)
  if(NOT TARGET nfd)
    loader_begin("nativefiledialog-extended")

    # IMPORTANT: The xdg-desktop-portal allows to display the actual native file dialog
    # on linux systems (by default the nfd uses gtk). However there might be some issues 
    # on DEs that are not directly supporting the portals. The rule of thumb is that everywhere 
    # you can run file dialog from your flatpak app, the portals should work seamlessly.
    set(NFD_PORTAL
        ON
        CACHE BOOL "Use portal to use native file dialog instead of the gtk" FORCE)
    
    FetchContent_Declare(
      nfd 
      GIT_REPOSITORY "https://github.com/btzy/nativefiledialog-extended.git"
      GIT_TAG "v1.2.1"
      PATCH_COMMAND git apply --ignore-whitespace
                    "${PATCHES_DIR}/nfd.patch"
      UPDATE_DISCONNECTED 1)
    FetchContent_MakeAvailable(nfd)

    loader_end()
  endif()
endfunction()

# =============================================================================
# Tracy
# =============================================================================
function(fetch_tracy)
  if(NOT TARGET TracyClient)
    loader_begin("Tracy")

    FetchContent_Declare(
      Tracy
      GIT_REPOSITORY "https://github.com/wolfpld/tracy"
      GIT_TAG "v0.11.1"
      UPDATE_DISCONNECTED 1)
    FetchContent_MakeAvailable(Tracy)

    loader_end()
  endif()
endfunction()

# =============================================================================
# GoogleTest
# =============================================================================
function(fetch_googletest)
  if(NOT TARGET gtest)
    loader_begin("GoogleTest")

    FetchContent_Declare(
      googletest
      GIT_REPOSITORY "https://github.com/google/googletest.git"
      GIT_TAG "v1.15.2"
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    loader_end()
  endif()
endfunction()

# =============================================================================
# fastgltf 
# =============================================================================
function(fetch_fastgltf)
  if(NOT TARGET fastgltf)
    loader_begin("fastgltf")

    FetchContent_Declare(
      fastgltf 
      GIT_REPOSITORY "https://github.com/spnda/fastgltf.git"
      GIT_TAG "v0.9.0"
      UPDATE_DISCONNECTED 1)
    FetchContent_MakeAvailable(fastgltf)

    loader_end()
  endif()
endfunction()

