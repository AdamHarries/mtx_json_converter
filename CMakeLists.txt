cmake_minimum_required(VERSION 2.8)
project(ICFJsonGeneration)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")

# include_directories(common)
# include_directories(include)


# I couldn't make it work with INTERFACE_INCLUDE_DIRECTORIES
include_directories("inc")

set(SOURCE_FILES
    src/mmio.cpp
    src/sparse_matrix.cpp)
add_library(SparseMatrix
    ${SOURCE_FILES}
    )

add_executable(mtjc apps/mtjc.cpp)
target_link_libraries(mtjc SparseMatrix)

 # target_link_libraries(sparse)
