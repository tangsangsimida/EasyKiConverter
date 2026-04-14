# Network module

add_library(EasyKiConverterNetwork STATIC
    AsyncNetworkRequest.cpp
    NetworkClient.cpp
)

target_include_directories(EasyKiConverterNetwork
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterNetwork
    PUBLIC
        Qt6::Core
        Qt6::Network
        EasyKiConverterUtils
)
