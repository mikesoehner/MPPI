#include <catch2/catch_test_macros.hpp>
#include <communicator.hpp>
#include <numeric>
#include <iostream>
#include <list>

TEST_CASE( "Send and Recv functionality", "[send_recv]" )
{
    // setup to be communicated vector
    std::vector<int> vec(10, 0);
    std::list<int> list(10, 0);

    mppi::Communicator comm;

    SECTION("Sending multiple of the same fundamental type")
    {
        int a{}, b{}, c{};
        
        if (comm.get_rank() == 0)
        {
            a = 4, b = 5, c = 78;
            comm.send(1, mppi::Tag(0), a, b, c);
        }
        else
        {
            comm.recv(0, mppi::Tag(0), a, b, c);

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
            comm.send(1, mppi::Tag(0), a, b, c);
        }
        else
        {
            comm.recv(0, mppi::Tag(0), a, b, c);

            REQUIRE(a == 4);
            REQUIRE(b == 5);
            REQUIRE(std::abs(c - 78.0f) < 0.00001f); 
        }
    }

    SECTION("Sending an entire stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(1, mppi::Tag(0), vec);

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(0, mppi::Tag(0), vec);

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
    }

    SECTION("Sending a fraction of an stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(1, mppi::Tag(0), vec | std::ranges::views::take(3));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(0, mppi::Tag(0), vec | std::ranges::views::take(3));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,0,0,0});
        }
    }

    SECTION("Sending multiple fractions of an contiguous stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(1, mppi::Tag(0), vec | std::ranges::views::take(3), vec | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(0, mppi::Tag(0), vec | std::ranges::views::take(3), vec | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,7,8,9});
        }
    }

    SECTION("Sending an entire non-contiguous stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(list.begin(), list.end(), 0);
            comm.send(1, mppi::Tag(0), list);
        }
        else
        {
            comm.recv(0, mppi::Tag(0), list);
            
            auto iter = list.begin();
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 1);
            REQUIRE(*iter++ == 2);
            REQUIRE(*iter++ == 3);
            REQUIRE(*iter++ == 4);
            REQUIRE(*iter++ == 5);
            REQUIRE(*iter++ == 6);
            REQUIRE(*iter++ == 7);
            REQUIRE(*iter++ == 8);
            REQUIRE(*iter++ == 9);
        }
    }

    SECTION("Sending a fraction of a non-contiguous stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(list.begin(), list.end(), 0);
            comm.send(1, mppi::Tag(0), list | std::ranges::views::take(5));
        }
        else
        {
            comm.recv(0, mppi::Tag(0), list | std::ranges::views::take(5));

            auto iter = list.begin();
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 1);
            REQUIRE(*iter++ == 2);
            REQUIRE(*iter++ == 3);
            REQUIRE(*iter++ == 4);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
        }
    }

    SECTION("Sending multiple fractions of a non-contiguous contiguous stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(list.begin(), list.end(), 0);
            comm.send(1, mppi::Tag(0), list | std::ranges::views::take(3), list | std::ranges::views::drop(7));
        }
        else
        {
            comm.recv(0, mppi::Tag(0), list | std::ranges::views::take(3), list | std::ranges::views::drop(7));

            auto iter = list.begin();
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 1);
            REQUIRE(*iter++ == 2);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 7);
            REQUIRE(*iter++ == 8);
            REQUIRE(*iter++ == 9);
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

        constexpr mppi::DataPattern<TestClass, "_a", "_d"> data_pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            comm.send(1, mppi::Tag(0), data_pattern, tests_vec);
        }
        else
        {
            tests_vec.resize(4);

            comm.recv(0, mppi::Tag(0), data_pattern, tests_vec);

            REQUIRE(tests_vec[0].get_a() == 1);
            REQUIRE(tests_vec[1].get_a() == 2);
            REQUIRE(tests_vec[2].get_a() == 3);
            REQUIRE(tests_vec[3].get_a() == 4);

            REQUIRE(tests_vec[0].get_b() == 0);
            REQUIRE(tests_vec[1].get_b() == 0);
            REQUIRE(tests_vec[2].get_b() == 0);
            REQUIRE(tests_vec[3].get_b() == 0);

            REQUIRE(std::abs(tests_vec[0].get_d()[0] - 4.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[1].get_d()[0] - 5.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[2].get_d()[0] - 6.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[0] - 7.0f) < 0.00001f);
        }
    }

    SECTION("Sending a view of an stl container with a DataPattern")
    {
        class TestClass
        {
        public:
            TestClass() = default;
            TestClass(std::array<double, 2> a, double b, char c, int d)
                : _a(a), _b(b), _c(c), _d(d)
            {}
    
            auto& get_a() { return _a; }
            auto& get_b() { return _b; }
            auto& get_c() { return _c; }
            auto& get_d() { return _d; }
    
        private:
            std::array<double, 2> _a;
            double _b;
            char _c;
            int _d;
        };

        constexpr mppi::DataPattern<TestClass, "_a", "_b", "_d"> data_pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass({4.0, 5.0}, 2.0, 'w', 3));
            tests_vec.emplace_back(TestClass({5.0, 6.0}, 3.0, 'x', 4));
            tests_vec.emplace_back(TestClass({6.0, 7.0}, 4.0, 'y', 5));
            tests_vec.emplace_back(TestClass({7.0, 8.0}, 5.0, 'z', 6));

            comm.send(1, mppi::Tag(0), data_pattern, tests_vec | std::ranges::views::all);
        }
        else
        {
            tests_vec.resize(4);

            comm.recv(0, mppi::Tag(0), data_pattern, tests_vec | std::ranges::views::all);

            REQUIRE(std::abs(tests_vec[0].get_a()[0] - 4.0) < 0.00001);
            REQUIRE(std::abs(tests_vec[1].get_a()[0] - 5.0) < 0.00001);
            REQUIRE(std::abs(tests_vec[2].get_a()[0] - 6.0) < 0.00001);
            REQUIRE(std::abs(tests_vec[3].get_a()[0] - 7.0) < 0.00001);

            REQUIRE(std::abs(tests_vec[0].get_b() - 2.0) < 0.00001);
            REQUIRE(std::abs(tests_vec[1].get_b() - 3.0) < 0.00001);
            REQUIRE(std::abs(tests_vec[2].get_b() - 4.0) < 0.00001);
            REQUIRE(std::abs(tests_vec[3].get_b() - 5.0) < 0.00001);

            REQUIRE(tests_vec[0].get_d() == 3);
            REQUIRE(tests_vec[1].get_d() == 4);
            REQUIRE(tests_vec[2].get_d() == 5);
            REQUIRE(tests_vec[3].get_d() == 6);
        }
    }

    SECTION("Sending an entire stl container with a dynamic DataPattern")
    {
        class TestClass
        {
        public:
            TestClass() = default;
            TestClass(int a, int b, double c, std::vector<float> d)
                : _a(a), _b(b), _c(c), _d(d)
            {}
    
            int& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }
            std::vector<float>& get_d() { return _d; }
    
        private:
            int _a {};
            int _b {};
            double _c {};
            std::vector<float> _d {};
        };


        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f, 10.0f, 11.0f}));

            mppi::DataPattern<TestClass, "_a", "_d"> data_pattern(tests_vec);

            comm.send(1, mppi::Tag(0), data_pattern, tests_vec);
        }
        else
        {
            tests_vec.resize(4);
            tests_vec[0] = TestClass(0, 0, 0.0, {0.0f, 0.0f});
            tests_vec[1] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f});
            tests_vec[2] = TestClass(0, 0, 0.0, {0.0f});
            tests_vec[3] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

            mppi::DataPattern<TestClass, "_a", "_d"> data_pattern(tests_vec);

            comm.recv(0, mppi::Tag(0), data_pattern, tests_vec);

            REQUIRE(tests_vec[0].get_a() == 1);
            REQUIRE(tests_vec[1].get_a() == 2);
            REQUIRE(tests_vec[2].get_a() == 3);
            REQUIRE(tests_vec[3].get_a() == 4);

            REQUIRE(tests_vec[0].get_b() == 0);
            REQUIRE(tests_vec[1].get_b() == 0);
            REQUIRE(tests_vec[2].get_b() == 0);
            REQUIRE(tests_vec[3].get_b() == 0);

            REQUIRE(std::abs(tests_vec[0].get_d()[0] - 4.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[1].get_d()[0] - 5.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[2].get_d()[0] - 6.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[0] - 7.0f) < 0.00001f);

            REQUIRE(std::abs(tests_vec[0].get_d()[1] - 5.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[1].get_d()[1] - 6.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[1] - 8.0f) < 0.00001f);

            REQUIRE(std::abs(tests_vec[1].get_d()[2] - 7.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[2] - 9.0f) < 0.00001f);

            REQUIRE(std::abs(tests_vec[1].get_d()[3] - 8.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[3] - 10.0f) < 0.00001f);

            REQUIRE(std::abs(tests_vec[3].get_d()[4] - 11.0f) < 0.00001f);
        }
    }

    SECTION("Sending an entire stl container with a dynamic, non-consecutive DataPattern")
    {
        class TestClass
        {
        public:
            TestClass() = default;
            TestClass(int a, int b, double c, std::list<float> d)
                : _a(a), _b(b), _c(c), _d(d)
            {}
    
            int& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }
            std::list<float>& get_d() { return _d; }
    
        private:
            int _a {};
            int _b {};
            double _c {};
            std::list<float> _d {};
        };


        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f, 10.0f, 11.0f}));

            mppi::DataPattern<TestClass, "_a", "_d"> data_pattern(tests_vec);

            comm.send(1, mppi::Tag(0), data_pattern, tests_vec);
        }
        else
        {
            tests_vec.resize(4);
            tests_vec[0] = TestClass(0, 0, 0.0, {0.0f, 0.0f});
            tests_vec[1] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f});
            tests_vec[2] = TestClass(0, 0, 0.0, {0.0f});
            tests_vec[3] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

            mppi::DataPattern<TestClass, "_a", "_d"> data_pattern(tests_vec);

            comm.recv(0, mppi::Tag(0), data_pattern, tests_vec);

            REQUIRE(tests_vec[0].get_a() == 1);
            REQUIRE(tests_vec[1].get_a() == 2);
            REQUIRE(tests_vec[2].get_a() == 3);
            REQUIRE(tests_vec[3].get_a() == 4);

            REQUIRE(tests_vec[0].get_b() == 0);
            REQUIRE(tests_vec[1].get_b() == 0);
            REQUIRE(tests_vec[2].get_b() == 0);
            REQUIRE(tests_vec[3].get_b() == 0);

            auto iter0 = tests_vec[0].get_d().begin();
            auto iter1 = tests_vec[1].get_d().begin();
            auto iter2 = tests_vec[2].get_d().begin();
            auto iter3 = tests_vec[3].get_d().begin();

            REQUIRE(std::abs(*iter0++ - 4.0f) < 0.00001f);
            REQUIRE(std::abs(*iter1++ - 5.0f) < 0.00001f);
            REQUIRE(std::abs(*iter2++ - 6.0f) < 0.00001f);
            REQUIRE(std::abs(*iter3++ - 7.0f) < 0.00001f);

            REQUIRE(std::abs(*iter0++ - 5.0f) < 0.00001f);
            REQUIRE(std::abs(*iter1++ - 6.0f) < 0.00001f);
            REQUIRE(std::abs(*iter3++ - 8.0f) < 0.00001f);

            REQUIRE(std::abs(*iter1++ - 7.0f) < 0.00001f);
            REQUIRE(std::abs(*iter3++ - 9.0f) < 0.00001f);

            REQUIRE(std::abs(*iter1++ - 8.0f) < 0.00001f);
            REQUIRE(std::abs(*iter3++ - 10.0f) < 0.00001f);

            REQUIRE(std::abs(*iter3++ - 11.0f) < 0.00001f);
        }
    }

    SECTION("ISending an entire stl container with a DataPattern")
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

        constexpr mppi::DataPattern<TestClass, "_a", "_d"> data_pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            auto request = comm.isend(1, mppi::Tag(0), data_pattern, tests_vec | std::ranges::views::all);
            comm.wait(request);
        }
        else
        {
            tests_vec.resize(4);

            auto request = comm.irecv(0, mppi::Tag(0), data_pattern, tests_vec | std::ranges::views::all);
            comm.wait(request);

            REQUIRE(tests_vec[0].get_a() == 1);
            REQUIRE(tests_vec[1].get_a() == 2);
            REQUIRE(tests_vec[2].get_a() == 3);
            REQUIRE(tests_vec[3].get_a() == 4);

            REQUIRE(tests_vec[0].get_b() == 0);
            REQUIRE(tests_vec[1].get_b() == 0);
            REQUIRE(tests_vec[2].get_b() == 0);
            REQUIRE(tests_vec[3].get_b() == 0);

            REQUIRE(std::abs(tests_vec[0].get_d()[0] - 4.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[1].get_d()[0] - 5.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[2].get_d()[0] - 6.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[0] - 7.0f) < 0.00001f);
        }
    }
}
