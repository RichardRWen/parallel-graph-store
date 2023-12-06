set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

set(TEST_FILES
        unit_parlay.cpp
)

foreach(TEST_FILE ${TEST_FILES})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_FILE})
        target_link_libraries(${TEST_NAME} DiskANNLibrary OpenSSL::SSL OpenSSL::Crypto Parlay::parlay)
endforeach()
