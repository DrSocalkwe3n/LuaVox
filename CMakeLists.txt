cmake_minimum_required(VERSION 3.13)

option(BUILD_CLIENT "Build the client" ON)
option(USE_LIBURING "Build with liburing support" ON)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -DGLM_FORCE_DEPTH_ZERO_TO_ONE")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections") # -rdynamic

# gprof
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")

# sanitizer
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment -fsanitize=address -fno-omit-frame-pointer") 
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment -fsanitize=address") 

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")

project(LuaVox VERSION 0.0 DESCRIPTION "LuaVox Description")

add_library(luavox_common INTERFACE)
target_compile_features(luavox_common INTERFACE cxx_std_23)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  target_compile_options(luavox_common INTERFACE -fcoroutines)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  target_compile_options(luavox_common INTERFACE -fcoroutine)
endif()

if(USE_LIBURING)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBURING liburing>=2.0 IMPORTED_TARGET)

    if(LIBURING_FOUND)
        message(STATUS "liburing found, enabling io_uring support")
        target_compile_definitions(luavox_common INTERFACE LUAVOX_HAVE_LIBURING)
        target_link_libraries(luavox_common INTERFACE PkgConfig::LIBURING)
    else()
        message(FATAL_ERROR "liburing >= 2.0 not found but USE_LIBURING is ON")
    endif()
else()
    message(STATUS "liburing support is disabled")
endif()

include(FetchContent)

# Boost
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(Boost_USE_STATIC_LIBS ON)
# find_package(Boost REQUIRED COMPONENTS thread json)
# target_include_directories(${PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIR})
# target_link_libraries(${PROJECT_NAME} PUBLIC Boost::thread Boost::json)

set(BOOST_INCLUDE_LIBRARIES asio thread json)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.7z
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(Boost)
target_link_libraries(luavox_common INTERFACE Boost::asio Boost::thread Boost::json)

# glm
# find_package(glm REQUIRED)
# target_include_directories(${PROJECT_NAME} PUBLIC ${GLM_INCLUDE_DIR})
# target_link_libraries(${PROJECT_NAME} PUBLIC ${GLM_LIBRARY})

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)
target_link_libraries(luavox_common INTERFACE glm)

find_package(ICU REQUIRED COMPONENTS i18n uc)
target_include_directories(luavox_common INTERFACE ${ICU_INCLUDE_DIR})
target_link_libraries(luavox_common INTERFACE ${ICU_LIBRARIES})

find_package(OpenSSL REQUIRED)
target_include_directories(luavox_common INTERFACE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(luavox_common INTERFACE ${OPENSSL_LIBRARIES})

# JPEG
find_package(JPEG REQUIRED)
target_include_directories(luavox_common INTERFACE ${JPEG_INCLUDE_DIRS})
target_link_libraries(luavox_common INTERFACE JPEG::JPEG)

# PNG
find_package(PNG REQUIRED)
target_include_directories(luavox_common INTERFACE ${PNG_INCLUDE_DIRS})
target_link_libraries(luavox_common INTERFACE PNG::PNG)

# PNG++
target_include_directories(luavox_common INTERFACE "${PROJECT_SOURCE_DIR}/Libs/png++")

# sqlite3
FetchContent_Declare(sqlite3 GIT_REPOSITORY https://github.com/sjinks/sqlite3-cmake GIT_TAG v3.49.1)
FetchContent_MakeAvailable(sqlite3)
target_link_libraries(luavox_common INTERFACE SQLite::SQLite3)

# Static Assets
file(GLOB_RECURSE ASSETS RELATIVE "${PROJECT_SOURCE_DIR}/assets" "assets/*.*")
add_custom_command(OUTPUT assets.o resources.cpp INPUT ${ASSETS}
    COMMAND cd ${CMAKE_CURRENT_BINARY_DIR} && ${CMAKE_CURRENT_SOURCE_DIR}/Src/assets.py ${ASSETS}
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}/assets" && ld -r -b binary -o '${CMAKE_CURRENT_BINARY_DIR}/assets.o' ${ASSETS}
    COMMAND objcopy --rename-section .data=.rodata,alloc,load,readonly,data,contents ${CMAKE_CURRENT_BINARY_DIR}/assets.o ${CMAKE_CURRENT_BINARY_DIR}/assets.o)

set_source_files_properties(assets.o PROPERTIES EXTERNAL_OBJECT true GENERATED true)
add_library(assets STATIC resources.cpp assets.o)
set_target_properties(assets PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(luavox_common INTERFACE assets)

if(BUILD_CLIENT)
  add_executable(luavox_client)

  # Common
  target_link_libraries(luavox_client PUBLIC luavox_common)

  # Исходники
  file(GLOB_RECURSE SOURCES RELATIVE ${PROJECT_SOURCE_DIR} "Src/*.cpp")
  target_sources(luavox_client PRIVATE ${SOURCES})
  target_include_directories(luavox_client PUBLIC "${PROJECT_SOURCE_DIR}/Src")

  # GLFW3
  FetchContent_Declare(
      glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG 3.4
  )
  FetchContent_MakeAvailable(glfw)
  target_link_libraries(luavox_client PUBLIC glfw)

  # FreeType
  find_package(Freetype REQUIRED)
  # FetchContent_Declare(
  #   freetype
  #   GIT_REPOSITORY https://github.com/freetype/freetype.git
  #   GIT_TAG freetype
  # )
  # FetchContent_MakeAvailable(freetype)
  target_include_directories(luavox_client PUBLIC ${freetype_INCLUDE_DIRS})
  target_link_libraries(luavox_client PUBLIC Freetype::Freetype)

  # ImGui
  file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/Libs/imgui/*.cpp")
  target_sources(luavox_client PRIVATE ${SOURCES} "${PROJECT_SOURCE_DIR}/Libs/imgui/backends/imgui_impl_glfw.cpp" "${PROJECT_SOURCE_DIR}/Libs/imgui/backends/imgui_impl_vulkan.cpp")
  target_include_directories(luavox_client PUBLIC "${PROJECT_SOURCE_DIR}/Libs/imgui/")
endif()

