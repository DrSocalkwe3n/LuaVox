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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # target_compile_options(luavox_common INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  # target_link_options(luavox_common INTERFACE -fsanitize=address,undefined)
  # set(ENV{ASAN_OPTIONS} detect_leaks=0)
endif()

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
set(BOOST_IOSTREAMS_ENABLE_ZLIB ON)
set(BOOST_INCLUDE_LIBRARIES asio thread json iostreams interprocess timer circular_buffer lockfree stacktrace uuid serialization nowide)
FetchContent_Declare(
  Boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.87.0
  GIT_PROGRESS true
  USES_TERMINAL_DOWNLOAD true
)
FetchContent_MakeAvailable(Boost)
target_link_libraries(luavox_common INTERFACE Boost::asio Boost::thread Boost::json Boost::iostreams Boost::interprocess Boost::timer Boost::circular_buffer Boost::lockfree Boost::stacktrace Boost::uuid Boost::serialization Boost::nowide)

# glm
# find_package(glm REQUIRED)
# target_include_directories(${PROJECT_NAME} PUBLIC ${GLM_INCLUDE_DIR})
# target_link_libraries(${PROJECT_NAME} PUBLIC ${GLM_LIBRARY})
FetchContent_Declare(
  luajit
  GIT_REPOSITORY https://luajit.org/git/luajit.git
  GIT_TAG v2.1
  GIT_PROGRESS true
  USES_TERMINAL_DOWNLOAD true
)
FetchContent_MakeAvailable(luajit)

set(LUAJIT_DIR ${luajit_SOURCE_DIR})
set(LUAJIT_ENABLE_LUA52COMPAT ON)
FetchContent_Declare(
  lua_cmake
  GIT_REPOSITORY https://github.com/zhaozg/luajit-cmake.git
  GIT_TAG 300c0b3f472be2be158f5b2e6385579ba5c6c0f9
  GIT_PROGRESS true
  USES_TERMINAL_DOWNLOAD true
)
FetchContent_MakeAvailable(lua_cmake)

target_link_libraries(luavox_common INTERFACE luajit::header luajit::lib)
target_include_directories(luavox_common INTERFACE ${lua_cmake_BINARY_DIR})

FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.5.0
  GIT_PROGRESS true
  USES_TERMINAL_DOWNLOAD true
)
FetchContent_MakeAvailable(sol2)
target_link_libraries(luavox_common INTERFACE sol2::sol2)


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

FetchContent_Declare(
  RectangleBinPack
  GIT_REPOSITORY https://github.com/juj/RectangleBinPack.git
  GIT_TAG 83e7e1132d93777e3732dfaae26b0f3703be2036
)
FetchContent_MakeAvailable(RectangleBinPack)
target_link_libraries(luavox_common INTERFACE RectangleBinPack)

# Static Assets
find_package(Python3 REQUIRED)
set(ASSETS_DIR "${PROJECT_SOURCE_DIR}/assets")
file(GLOB_RECURSE ASSETS_LIST RELATIVE "${ASSETS_DIR}" "${ASSETS_DIR}/*.*")

set(ASSETS_O "${CMAKE_CURRENT_BINARY_DIR}/assets.o")
set(ASSETS_LD_O "${CMAKE_CURRENT_BINARY_DIR}/assets_ld.o")
set(RESOURCES_CPP "${CMAKE_CURRENT_BINARY_DIR}/resources.cpp")

set(ASSETS_ABS)
foreach(asset IN LISTS ASSETS_LIST)
    list(APPEND ASSETS_ABS "${ASSETS_DIR}/${asset}")
endforeach()

add_custom_command(
    OUTPUT ${ASSETS_O} ${RESOURCES_CPP}
    DEPENDS ${ASSETS_ABS}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Src/assets.py
        "${RESOURCES_CPP}"
        ${ASSETS_LIST}
    COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_SOURCE_DIR}/assets" ld -r -b binary -o "${ASSETS_LD_O}" ${ASSETS_LIST}
    COMMAND ${CMAKE_OBJCOPY}
        -O elf64-x86-64
        --rename-section .data=.rodata,alloc,load,readonly,data,contents
        ${ASSETS_LD_O}
        ${ASSETS_O}
    COMMENT "Embedding assets: generating resources.cpp and assets.o"
    VERBATIM
)

set_source_files_properties(${RESOURCES_CPP} PROPERTIES GENERATED true)
set_source_files_properties(${ASSETS_O} PROPERTIES EXTERNAL_OBJECT true GENERATED true)
add_library(assets STATIC ${RESOURCES_CPP} ${ASSETS_O})
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

