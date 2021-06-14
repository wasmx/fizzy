# Hunter local configuration.

hunter_config(
    GTest
    VERSION 1.11.0
    URL https://github.com/google/googletest/archive/release-1.11.0.tar.gz
    SHA1 7b100bb68db8df1060e178c495f3cbe941c9b058
    CMAKE_ARGS
    HUNTER_INSTALL_LICENSE_FILES=LICENSE
    gtest_force_shared_crt=TRUE
)

hunter_config(
    benchmark
    VERSION 1.5.3
    URL https://github.com/google/benchmark/archive/v1.5.3.tar.gz
    SHA1 32655d8796e708439ac5d4de8aa31f00d9dbda3b
)
