cmake_minimum_required(VERSION 3.16)
project(MiniSGBD CXX)
set(CMAKE_CXX_STANDARD 17)

set(STORAGE_SRC
    src/storage/disk_manager.cpp
)
set(BUFFER_SRC
    src/storage/disk_manager.cpp
    src/buffer/buffer_manager.cpp
)
set(BTREE_SRC
    src/storage/disk_manager.cpp
    src/buffer/buffer_manager.cpp
    src/index/btree_node.cpp
    src/index/btree.cpp
)

add_executable(mini_sgbd    main.cpp               ${STORAGE_SRC})
add_executable(test_storage tests/test_storage.cpp ${STORAGE_SRC})
add_executable(test_buffer  tests/test_buffer.cpp  ${BUFFER_SRC})
add_executable(test_btree   tests/test_btree.cpp   ${BTREE_SRC})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)