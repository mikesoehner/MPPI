#include <catch2/catch_test_macros.hpp>
#include <communicator.hpp>
#include <numeric>
#include <iostream>

TEST_CASE( "Send and Recv functionality", "[send_recv]" )
{
    // setup to be communicated vector
    std::vector<int> vec(10, 0);

    Communicator comm;

    SECTION("Sending multiple of the same fundamental type")
    {
        int a{}, b{}, c{};
        
        if (comm.get_rank() == 0)
        {
            a = 4, b = 5, c = 78;
            comm.send(1, Tag(0), a, b, c);
        }
        else
        {
            comm.recv(0, Tag(0), a, b, c);

            REQUIRE(a == 4);
            REQUIRE(b == 5);
            REQUIRE(c == 78);
        }   
    }

    SECTION("Sending multiple of different fundamental type") 
    {
        int a{};
        unsigned long b{};
        float c{};

        if (comm.get_rank() == 0)
        {
            a = 4, b = 5, c = 78.0f;
            comm.send(1, Tag(0), a, b, c);
        }
        else
        {
            comm.recv(0, Tag(0), a, b, c);

            REQUIRE(a == 4);
            REQUIRE(b == 5);
            REQUIRE(c == 78.0f);
        }   
    }

    SECTION("Sending an entire stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(1, Tag(0), vec);

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(0, Tag(0), vec);

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
    }

    SECTION("Sending a fraction of an stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(1, Tag(0), vec | std::ranges::views::take(3));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(0, Tag(0), vec | std::ranges::views::take(3));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,0,0,0});
        }
    }

    SECTION("Sending multiple fractions of an stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(1, Tag(0), vec | std::ranges::views::take(3), vec | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(0, Tag(0), vec | std::ranges::views::take(3), vec | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,7,8,9});
        }
    }

    SECTION("Sending an entire stl container with a DataPattern")
    {
        class TestClass
        {
        public:
            TestClass() = default;
            TestClass(int a, int b, double c, std::array<float, 3> d)
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

        TestClass testpattern;
        DataPattern data_pattern(&testpattern, testpattern.get_a(), testpattern.get_d());

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            comm.send(1, Tag(0), data_pattern, tests_vec);
        }
        else
        {
            tests_vec.resize(4);

            comm.recv(0, Tag(0), data_pattern, tests_vec);

            REQUIRE(tests_vec[0].get_a() == 1);
            REQUIRE(tests_vec[1].get_a() == 2);
            REQUIRE(tests_vec[2].get_a() == 3);
            REQUIRE(tests_vec[3].get_a() == 4);

            REQUIRE(std::abs(tests_vec[0].get_d()[0] - 4.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[1].get_d()[0] - 5.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[2].get_d()[0] - 6.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[0] - 7.0f) < 0.00001f);
        }



    }
}
