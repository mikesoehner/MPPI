#include <catch2/catch_test_macros.hpp>
#include "amodes.hpp"

#define STATIC_REQUIRE_ERROR(Type, ...) \
    { \
        constexpr auto Result = []<typename TestType>() \
        { \
            return requires { __VA_ARGS__; }; \
        }.template operator()<Type>(); \
        STATIC_REQUIRE_FALSE(Result); \
    }

TEST_CASE( "Access Modes functionality", "[amodes]" )
{

    SECTION("AccessMode creation with one Mode")
    {
        AMode<MODE_APPEND> amode;
    }

    SECTION("AccessMode creation with two Modes")
    {
        STATIC_REQUIRE_ERROR(MODE_RDONLY, typename AMode<TestType, MODE_CREATE>);
    }
}