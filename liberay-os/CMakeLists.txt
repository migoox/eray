configure_library(
    NAME liberay-os 
    LIBS glfw imgui nfd liberay-util liberay-math liberay-driver
    # GLFW_INCLUDE_NONE makes the GLFW header not include any OpenGL or OpenGL ES
    # API header. This is useful in combination with an extension loading library
    # (GLAD in our case).
    COMPILE_DEFINITIONS GLFW_INCLUDE_NONE
)
