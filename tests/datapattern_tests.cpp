#include <catch2/catch_test_macros.hpp>
#include "datapattern.hpp"
#include "datatype.hpp"
#include <memory>
// #include "communicator.hpp"

TEST_CASE( "DataPattern class functionality", "[DataPattern]" )
{
    class Test
    {
    public:
        Test() = default;
        Test(int a, int b, double c, std::array<float, 3> d)
            : _a(a), _b(b), _c(c), _d(d)
        {}

        int& get_a() { return _a; }
        int& get_b() { return _b; }
        double& get_c() { return _c; }
        std::array<float, 3>& get_d() { return _d; }

    private:
        int _a {};
        int _b {};
        double _c {};
        std::array<float, 3> _d {};
    };

    SECTION("Basic Test")
    {
        Test test(1, 2, 3.0, {4.0f, 5.0f, 6.0f});

        constexpr DataPattern<Test, "_a", "_c"> data_pattern;

        struct Destination
        {
            int a {};
            double c{};
        };

        Destination dest;

        size_t offset = 0;
        data_pattern.store_from_type(reinterpret_cast<std::byte*>(&dest), &test, offset);

        REQUIRE(1 == dest.a);
        REQUIRE(std::abs(3.0 - dest.c) < 0.0001);
    }

    SECTION("Advanced Test")
    {
        std::vector<Test> tests;

        tests.emplace_back(Test(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
        tests.emplace_back(Test(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
        tests.emplace_back(Test(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
        tests.emplace_back(Test(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

        constexpr DataPattern<Test, "_a", "_c", "_d"> data_pattern;

        std::pmr::monotonic_buffer_resource mem_res {};
        auto view = tests | std::ranges::views::all;
        Data data(Send{}, mem_res, data_pattern, tests);

        // reset original vector
        tests[0] = Test{};
        tests[1] = Test{};
        tests[2] = Test{};
        tests[3] = Test{};

        data.retrieve_data(data_pattern, tests);

        REQUIRE(tests[0].get_a() == 1);
        REQUIRE(tests[1].get_a() == 2);
        REQUIRE(tests[2].get_a() == 3);
        REQUIRE(tests[3].get_a() == 4);

        REQUIRE(tests[0].get_b() == 0);
        REQUIRE(tests[1].get_b() == 0);
        REQUIRE(tests[2].get_b() == 0);
        REQUIRE(tests[3].get_b() == 0);

        REQUIRE(std::abs(tests[0].get_c() - 3.0) < 0.00001);
        REQUIRE(std::abs(tests[1].get_c() - 4.0) < 0.00001);
        REQUIRE(std::abs(tests[2].get_c() - 5.0) < 0.00001);
        REQUIRE(std::abs(tests[3].get_c() - 6.0) < 0.00001);

        REQUIRE(std::abs(tests[0].get_d()[0] - 4.0f) < 0.00001f);
        REQUIRE(std::abs(tests[1].get_d()[0] - 5.0f) < 0.00001f);
        REQUIRE(std::abs(tests[2].get_d()[0] - 6.0f) < 0.00001f);
        REQUIRE(std::abs(tests[3].get_d()[0] - 7.0f) < 0.00001f);
    }
}