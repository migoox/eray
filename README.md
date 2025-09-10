# eray

`eray` is a cross-platform 3D rendering collection of libraries written in C++23, which consists of the following modules:
- `os`: provides rendering api agnostic operating system abstraction, allows for window creation and it's management (currently, only GLFW is supported), provides a compile-time window event system,  
- `math`: vectors, matrices, quaternions and more, greatly influenced by [glm](https://github.com/g-truc/glm),
- `util`: utilities used among the codebase, e.g. logger, containers 
- `res`: assets system that integrates libraries like stbi_image and assimp,
- `glren`: OpenGL abstraction layer,
- `vkren`: Vulkan abstraction layer and window integration.

The main goal of the library is to serve me as a framework for building computer graphics applications while also providing me an opportunity to learn about engine programming.

## Developer environment

The following dependencies must be installed on the host machine to develop eray:

- git
- cmake
- `gcc14`|`clang19`|`msvc-latest`
- ninja
- vulkan-sdk ([LunarG](https://vulkan.lunarg.com/)), use (`setup-env.sh`|`setup-env.bat`) script for sourcing the `VULKAN_SDK` environmental variable.


