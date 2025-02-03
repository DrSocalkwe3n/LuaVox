cmake_minimum_required(VERSION 3.17)

project (TOS_Lib
	VERSION 1.0
	DESCRIPTION "TOS Lib")


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

add_library(${PROJECT_NAME} STATIC)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

find_package(ICU REQUIRED COMPONENTS i18n uc)
target_include_directories(${PROJECT_NAME} PUBLIC ${ICU_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ${ICU_LIBRARIES})

find_package(OpenSSL REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${OPENSSL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ${OPENSSL_LIBRARIES})

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS timer)
target_include_directories(${PROJECT_NAME} PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC ${Boost_LIBRARIES})


file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Src/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Src)

