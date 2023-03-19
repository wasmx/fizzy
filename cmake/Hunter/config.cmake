# Hunter local configuration.

#hunter_config(
#    GTest
#    VERSION 1.11.0
#    URL https://github.com/google/googletest/archive/release-1.11.0.tar.gz
#    SHA1 7b100bb68db8df1060e178c495f3cbe941c9b058
#    CMAKE_ARGS
#    HUNTER_INSTALL_LICENSE_FILES=LICENSE
#    gtest_force_shared_crt=TRUE
#)

hunter_config(
    benchmark
    VERSION 1.6.0
    URL https://github.com/google/benchmark/archive/v1.6.0.tar.gz
    SHA1 c4d1a9135e779c5507015ccc8c428cb4aca69cef
)
