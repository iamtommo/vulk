cmake_minimum_required(VERSION 3.27)
project(vulk C)

set(CMAKE_C_STANDARD 99)

include_directories(lib/glfw/include)
include_directories(include/)

find_package(Vulkan REQUIRED)

link_directories(lib/glfw/bin/win/)

add_executable(vulk main.c)
target_link_libraries(vulk glfw3 ${Vulkan_LIBRARIES})

include_directories(${Vulkan_INCLUDE_DIRS})

