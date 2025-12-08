#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "pattern.hpp"
#include "datatype.hpp"
// #include "communicator.hpp"

TEST_CASE( "Pattern class functionality", "[Pattern]" )
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

        constexpr mppi::Pattern<Test, "_a", "_c"> pattern;

        struct Destination
        {
            int a {};
            double c{};
        };

        Destination dest;

        size_t offset = 0;
        pattern.pack(reinterpret_cast<std::byte*>(&dest), &test, offset);

        REQUIRE(1 == dest.a);
        REQUIRE(std::abs(3.0 - dest.c) < 0.0001);
    }

    SECTION("Advanced Test using a pattern")
    {
        std::vector<Test> tests;

        tests.emplace_back(Test(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
        tests.emplace_back(Test(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
        tests.emplace_back(Test(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
        tests.emplace_back(Test(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

        constexpr mppi::Pattern<Test, "_a", "_c", "_d"> pattern;

        std::pmr::monotonic_buffer_resource mem_res {};
        auto view = tests | std::ranges::views::all;
        mppi::Data data(mppi::Send{}, mem_res, tests | mppi::pattern_view(pattern));

        // reset original vector
        tests[0] = Test{};
        tests[1] = Test{};
        tests[2] = Test{};
        tests[3] = Test{};

        data.retrieve_data(tests | mppi::pattern_view(pattern));

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

    SECTION("Advanced Test using multiple patterns")
    {
        std::vector<Test> test0(4);
        test0[0] = Test(1, 2, 3.0, {4.0f, 5.0f, 6.0f});
        test0[1] = Test(2, 3, 4.0, {5.0f, 6.0f, 7.0f});
        test0[2] = Test(3, 4, 5.0, {6.0f, 7.0f, 8.0f});
        test0[3] = Test(4, 5, 6.0, {7.0f, 8.0f, 9.0f});

        std::vector<Test> test1(4);
        test1[0] = Test(1, 2, 3.0, {4.0f, 5.0f, 6.0f});
        test1[1] = Test(2, 3, 4.0, {5.0f, 6.0f, 7.0f});
        test1[2] = Test(3, 4, 5.0, {6.0f, 7.0f, 8.0f});
        test1[3] = Test(4, 5, 6.0, {7.0f, 8.0f, 9.0f});

        constexpr mppi::Pattern<Test, "_a", "_c"> pattern0;
        constexpr mppi::Pattern<Test, "_b", "_d"> pattern1;

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, test0 | mppi::pattern_view(pattern0), 
                                                test1 | mppi::pattern_view(pattern1));

        // reset original vector
        test0[0] = Test{};
        test0[1] = Test{};
        test0[2] = Test{};
        test0[3] = Test{};

        test1[0] = Test{};
        test1[1] = Test{};
        test1[2] = Test{};
        test1[3] = Test{};

        data.retrieve_data(test0 | mppi::pattern_view(pattern0), 
                            test1 | mppi::pattern_view(pattern1));

        REQUIRE(test0[0].get_a() == 1);
        REQUIRE(test0[1].get_a() == 2);
        REQUIRE(test0[2].get_a() == 3);
        REQUIRE(test0[3].get_a() == 4);

        REQUIRE(std::abs(test0[0].get_c() - 3.0) < 0.00001);
        REQUIRE(std::abs(test0[1].get_c() - 4.0) < 0.00001);
        REQUIRE(std::abs(test0[2].get_c() - 5.0) < 0.00001);
        REQUIRE(std::abs(test0[3].get_c() - 6.0) < 0.00001);

        REQUIRE(test1[0].get_b() == 2);
        REQUIRE(test1[1].get_b() == 3);
        REQUIRE(test1[2].get_b() == 4);
        REQUIRE(test1[3].get_b() == 5);

        REQUIRE(std::abs(test1[0].get_d()[0] - 4.0f) < 0.00001f);
        REQUIRE(std::abs(test1[1].get_d()[0] - 5.0f) < 0.00001f);
        REQUIRE(std::abs(test1[2].get_d()[0] - 6.0f) < 0.00001f);
        REQUIRE(std::abs(test1[3].get_d()[0] - 7.0f) < 0.00001f);
    }
}