cmake_minimum_required(VERSION 3.13)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -UNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections") # -rdynamic

project (LuaVox VERSION 0.0 DESCRIPTION "LuaVox Description")
add_executable(${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

file(GLOB_RECURSE SOURCES RELATIVE ${PROJECT_SOURCE_DIR} "Src/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/Src")

# Boost
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS thread json)
target_include_directories(${PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC Boost::thread Boost::json)

# glm
find_package(glm REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${GLM_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ${GLM_LIBRARY})

# JPEG
find_package(JPEG REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${JPEG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC JPEG::JPEG)

# PNG
find_package(PNG REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${PNG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC PNG::PNG)

# PNG++
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/Libs/png++")

# GLFW3
find_package(glfw3 REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${GLFW_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

# FreeType
find_package(Freetype REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${GLM_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC Freetype::Freetype)

# ImGui
file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/Libs/imgui/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES} "${PROJECT_SOURCE_DIR}/Libs/imgui/backends/imgui_impl_glfw.cpp" "${PROJECT_SOURCE_DIR}/Libs/imgui/backends/imgui_impl_vulkan.cpp")
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}/Libs/imgui/")

# TOS Lib
add_subdirectory("${PROJECT_SOURCE_DIR}/Libs/TOS Lib")
target_link_libraries(${PROJECT_NAME} PUBLIC TOS_Lib)


# Static Assets
file(GLOB_RECURSE ASSETS RELATIVE "${PROJECT_SOURCE_DIR}/assets" "assets/*.*")
add_custom_command(OUTPUT assets.o resources.cpp INPUT ${ASSETS}
    COMMAND cd ${CMAKE_CURRENT_BINARY_DIR} && ${CMAKE_CURRENT_SOURCE_DIR}/Src/assets.py ${ASSETS}
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/assets" && ld -r -b binary -o '${CMAKE_CURRENT_BINARY_DIR}/assets.o' ${ASSETS}
    COMMAND objcopy --rename-section .data=.rodata,alloc,load,readonly,data,contents ${CMAKE_CURRENT_BINARY_DIR}/assets.o ${CMAKE_CURRENT_BINARY_DIR}/assets.o)

SET_SOURCE_FILES_PROPERTIES(assets.o PROPERTIES EXTERNAL_OBJECT true GENERATED true)
add_library(assets STATIC resources.cpp assets.o)
SET_TARGET_PROPERTIES(assets PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${PROJECT_NAME} PUBLIC assets uring)

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment") 