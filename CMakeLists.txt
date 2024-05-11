include(./.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
message(STATUS "Local .env.cmake: ${LOCAL_ENV}")

cmake_minimum_required(VERSION 3.11.0)

set(NAME VulkanRenderer) # program name 

# MinGW paths, if its environment is in using.
message(STATUS "using ${CMAKE_GENERATOR}")
if (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    if (NOT MINGW_PATH) 
        message(FATAL_ERROR "MINGW_PATH not set in .env.cmake")
    endif()
    set(USE_MINGW "True")
    set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/gcc.exe)
    set(CMAKE_CXX_COMPILER  ${MINGW_PATH}/bin/g++.exe)
endif()

project(${NAME} VERSION 0.0.1)

# Vulkan. Set VULKAN_SDK_PATH in .env.cmake to target specific Vulkan version.
if (DEFINED VULKAN_SDK_PATH)
    set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include")
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib")
    set(Vulkan_DLLS "${VULKAN_SDK_PATH}/Bin")
    set(Vulkan_FOUND "True")
else()
    message("VULKAN_SDK_PATH is not specified in .env.cmake, trying to find it automatically")
    find_package(Vulkan REQUIRED)
    message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
    set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
    set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include")
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib")
    set(Vulkan_DLLS "${VULKAN_SDK_PATH}/Bin")
endif()
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library!")
else()
    message(STATUS "Using vulkan lib at: ${Vulkan_LIBRARIES}")
    message(STATUS "Using vulkan dlls at: ${Vulkan_DLLS}")
endif()

# GLFW. Set GLFW_PATH in .env.cmake to target specific glfw.
if (DEFINED GLFW_PATH)
    message(STATUS "Using GLFW path specified in .env")
    set(GLFW_INCLUDE_DIRS "${GLFW_PATH}/include")
    if (MSVC)
        message(STATUS "USING MSVC")
        set(GLFW_LIB "${GLFW_PATH}/lib-vc2022")
    elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
        message(STATUS "USING MINGW")
        set(GLFW_LIB "${GLFW_PATH}/lib-mingw-w64")
    endif()
else()
    message("GLFW_PATH is not specified in .env.cmake, using external/glfw")
    set(GLFW_PATH external/glfw)
    set(GLFW_INCLUDE_DIRS "${GLFW_PATH}/include")
    if (MSVC)
        set(GLFW_LIB "${GLFW_PATH}/lib-vc2022")
    elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
        set(GLFW_LIB "${GLFW_PATH}/lib-mingw-w64")
    endif()
    message(STATUS "Using glfw lib at: ${GLFW_LIB}")
endif()

if (NOT GLM_PATH)
    message(STATUS "TINYOBJ_PATH not specified in .env.cmake, using external/glm-master")
    set(GLM_PATH external/glm-master)
endif()

if (NOT TINYOBJ_PATH)
    message(STATUS "TINYOBJ_PATH not specified in .env.cmake, using external/tinyobjloader")
    set(TINYOBJ_PATH external/tinyobjloader)
endif()

if (NOT STB_IMAGE_PATH)
    message(STATUS "STB_IMAGE_PATH not specified in .env.cmake, using external/stb_image")
    set(STB_IMAGE_PATH external/stb_image)
endif()

if (NOT IMGUI_PATH)
    message(STATUS "IMGUI_PATH not specified in .env.cmake, using external/imgui")
    set(IMGUI_PATH external/imgui-docking)
endif()

if (NOT IMGUIZMO_PATH)
    message(STATUS "IMGUIZMO_PATH not specified in .env.cmake, using external/imguizmo")
    set(IMGUIZMO_PATH external/imguizmo)
endif()

include_directories(external)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp
                          ${PROJECT_SOURCE_DIR}/external/*.cpp
                          ${PROJECT_SOURCE_DIR}/apps/*cpp)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/ZI>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/ZI>")

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20) # C++ standard version

target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES) # predefined preprocessor defines

# VS debugger working directory
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# Includes, libs, dlls
if (WIN32)
    message(STATUS "CREATING BUILD FOR WINDOWS")

    if (USE_MINGW)
        target_include_directories(${PROJECT_NAME} PUBLIC
            ${MINGW_PATH}/include
        )
        target_link_directories(${PROJECT_NAME} PUBLIC
            ${MINGW_PATH}/lib
        )
    endif()

    target_include_directories(${PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${Vulkan_INCLUDE_DIRS}
        ${TINYOBJ_PATH}
        ${GLFW_INCLUDE_DIRS}
        ${GLM_PATH}
        ${STB_IMAGE_PATH}
        ${IMGUI_PATH}
        ${IMGUI_PATH}/backends
        ${IMGUIZMO_PATH}
    )

    target_link_directories(${PROJECT_NAME} PUBLIC
        ${Vulkan_LIBRARIES}
        ${Vulkan_DLLS}
        ${GLFW_LIB}
    )

    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
	add_subdirectory(external/volk-master)

    target_link_libraries(${PROJECT_NAME}
        shaderc_shared
        glfw3
        volk
    )
elseif (UNIX)
    message(STATUS "CREATING BUILD FOR UNIX")

    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)

    target_include_directories(${PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${TINYOBJ_PATH}
    )
    target_link_libraries(${PROJECT_NAME} glfw ${Vulkan_LIBRARIES})
endif()
