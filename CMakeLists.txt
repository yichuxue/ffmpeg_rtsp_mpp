cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)


SET(CMAKE_C_COMPILER "gcc")
SET(CMAKE_CXX_COMPILER "g++")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


project(mpp_test)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "-O0 -g")

include_directories(
	${CMAKE_CURRENT_LIST_DIR}/rockchip
	/usr/include/rga
)

link_directories(
	/usr/local/lib
	${CMAKE_CURRENT_LIST_DIR}/firefly_mpplib
)

set(SOURCE_FILES main.cpp
        mpp_rga/MppDecode.cpp
				mpp_rga/MppEncoder.cpp)

add_executable(mpp_test ${SOURCE_FILES})

target_link_libraries(mpp_test
		rockchip_mpp
		utils
		avformat
		avcodec
		avutil
		opencv_core
		opencv_highgui
		opencv_imgproc
		opencv_imgcodecs
		m
		drm
		rga
)
