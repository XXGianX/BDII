cmake_minimum_required(VERSION 3.16)
project(MiniSGBD CXX)
set(CMAKE_CXX_STANDARD 17)

set(STORAGE_SRC
    src/storage/disk_manager.cpp
)

add_executable(mini_sgbd main.cpp ${STORAGE_SRC})

add_executable(test_storage
    tests/test_storage.cpp
    ${STORAGE_SRC}
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)