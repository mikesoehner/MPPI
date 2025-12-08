#include <catch2/catch_test_macros.hpp>
#include <communicator.hpp>
#include <numeric>
#include <iostream>
#include <list>
#include <span>

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
            comm.send(mppi::Destination(1), mppi::Tag(0), a, b, c);
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), a, b, c);

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
            comm.send(mppi::Destination(1), mppi::Tag(0), a, b, c);
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), a, b, c);

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
            comm.send(mppi::Destination(1), mppi::Tag(0), vec);

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), vec);

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
    }

    SECTION("Sending a fraction of an stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(mppi::Destination(1), mppi::Tag(0), vec | std::ranges::views::take(3));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), vec | std::ranges::views::take(3));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,0,0,0});
        }
    }

    SECTION("Sending multiple fractions of an contiguous stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);
            comm.send(mppi::Destination(1), mppi::Tag(0), vec | std::ranges::views::take(3), vec | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), vec | std::ranges::views::take(3), vec | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,7,8,9});
        }
    }

    SECTION("Sending an entire non-contiguous stl container")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(list.begin(), list.end(), 0);
            comm.send(mppi::Destination(1), mppi::Tag(0), list);
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), list);
            
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
            comm.send(mppi::Destination(1), mppi::Tag(0), list | std::ranges::views::take(5));
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), list | std::ranges::views::take(5));

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
            std::iota(vec.begin(), vec.end(), 0);
            std::iota(list.begin(), list.end(), 0);
            comm.send(mppi::Destination(1), mppi::Tag(0), vec | std::ranges::views::take(3), list | std::ranges::views::drop(7));
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), vec | std::ranges::views::take(3), list | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,0,0,0});

            auto iter = list.begin();
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 0);
            REQUIRE(*iter++ == 7);
            REQUIRE(*iter++ == 8);
            REQUIRE(*iter++ == 9);
        }
    }

    SECTION("Sending an entire stl container with a Pattern without holes")
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

        constexpr mppi::Pattern<TestClass, "_a", "_b", "_c", "_d"> pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            tests_vec.resize(4);

            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));

            REQUIRE(tests_vec[0].get_a() == 1);
            REQUIRE(tests_vec[1].get_a() == 2);
            REQUIRE(tests_vec[2].get_a() == 3);
            REQUIRE(tests_vec[3].get_a() == 4);

            REQUIRE(tests_vec[0].get_b() == 2);
            REQUIRE(tests_vec[1].get_b() == 3);
            REQUIRE(tests_vec[2].get_b() == 4);
            REQUIRE(tests_vec[3].get_b() == 5);

            REQUIRE(std::abs(tests_vec[0].get_c() - 3.0f) < 0.00001);
            REQUIRE(std::abs(tests_vec[1].get_c() - 4.0f) < 0.00001);
            REQUIRE(std::abs(tests_vec[2].get_c() - 5.0f) < 0.00001);
            REQUIRE(std::abs(tests_vec[3].get_c() - 6.0f) < 0.00001);

            REQUIRE(std::abs(tests_vec[0].get_d()[0] - 4.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[1].get_d()[0] - 5.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[2].get_d()[0] - 6.0f) < 0.00001f);
            REQUIRE(std::abs(tests_vec[3].get_d()[0] - 7.0f) < 0.00001f);
        }
    }

    SECTION("Sending an entire stl container with a Pattern")
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

        constexpr mppi::Pattern<TestClass, "_a", "_d"> pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            tests_vec.resize(4);

            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));

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

    SECTION("Sending an entire stl span with a Pattern")
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

        constexpr mppi::Pattern<TestClass, "_a", "_d"> pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            auto span = std::span(tests_vec);
            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            tests_vec.resize(4);
            
            auto span = std::span(tests_vec);
            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));

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

    SECTION("Sending an entire stl container with a Pattern using Pattern_View")
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

        constexpr mppi::Pattern<TestClass, "_a", "_d"> pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            tests_vec.resize(4);

            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));

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

    SECTION("Sending a view of an stl container with a Pattern")
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

        constexpr mppi::Pattern<TestClass, "_a", "_b", "_d"> pattern;

        std::vector<TestClass> tests_vec;
        
        if (comm.get_rank() == 0)
        {
            tests_vec.emplace_back(TestClass({4.0, 5.0}, 2.0, 'w', 3));
            tests_vec.emplace_back(TestClass({5.0, 6.0}, 3.0, 'x', 4));
            tests_vec.emplace_back(TestClass({6.0, 7.0}, 4.0, 'y', 5));
            tests_vec.emplace_back(TestClass({7.0, 8.0}, 5.0, 'z', 6));

            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            tests_vec.resize(4);

            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));

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

    SECTION("Sending an entire stl container with a dynamic Pattern")
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

            mppi::Pattern<TestClass, "_a", "_d"> pattern;

            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            tests_vec.resize(4);
            tests_vec[0] = TestClass(0, 0, 0.0, {0.0f, 0.0f});
            tests_vec[1] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f});
            tests_vec[2] = TestClass(0, 0, 0.0, {0.0f});
            tests_vec[3] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

            mppi::Pattern<TestClass, "_a", "_d"> pattern;

            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));

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

    SECTION("Sending an entire stl container with a dynamic, non-consecutive Pattern")
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

            mppi::Pattern<TestClass, "_a", "_d"> pattern;

            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            tests_vec.resize(4);
            tests_vec[0] = TestClass(0, 0, 0.0, {0.0f, 0.0f});
            tests_vec[1] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f});
            tests_vec[2] = TestClass(0, 0, 0.0, {0.0f});
            tests_vec[3] = TestClass(0, 0, 0.0, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

            mppi::Pattern<TestClass, "_a", "_d"> pattern;

            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));

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

    SECTION("ISending an entire stl container with a Pattern")
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

        constexpr mppi::Pattern<TestClass, "_a", "_d"> pattern;

        std::vector<TestClass> tests_vec(100'000);
        
        if (comm.get_rank() == 0)
        {
            tests_vec[0] = (TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            tests_vec[1] = (TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            tests_vec[2] = (TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            tests_vec[3] = (TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            auto request = comm.isend(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
            request.wait();
        }
        else
        {
            // tests_vec.resize(4);

            auto request = comm.irecv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
            request.wait();

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

    SECTION("Sending multiple stl containers with multiple Patterns using Views")
    {
        class TestTestClass
        {
        public:
            TestTestClass() = default;
            TestTestClass(std::array<double, 2> a, double b, char c, int d)
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

        constexpr mppi::Pattern<TestTestClass, "_a", "_b"> pattern0;
        constexpr mppi::Pattern<TestTestClass, "_c", "_d"> pattern1;

        std::vector<TestTestClass> tests_vec0;
        std::vector<TestTestClass> tests_vec1;
        
        if (comm.get_rank() == 0)
        {
            tests_vec0.emplace_back(TestTestClass({4.0, 5.0}, 2.0, 'w', 3));
            tests_vec0.emplace_back(TestTestClass({5.0, 6.0}, 3.0, 'x', 4));
            tests_vec1.emplace_back(TestTestClass({6.0, 7.0}, 4.0, 'y', 5));
            tests_vec1.emplace_back(TestTestClass({7.0, 8.0}, 5.0, 'z', 6));

            comm.send(mppi::Destination(1), mppi::Tag(0), tests_vec0 | mppi::pattern_view(pattern0),
                                                          tests_vec1 | mppi::pattern_view(pattern1));
        }
        else
        {
            tests_vec0.resize(2);
            tests_vec1.resize(2);

            comm.recv(mppi::Source(0), mppi::Tag(0), tests_vec0 | mppi::pattern_view(pattern0),
                                                     tests_vec1 | mppi::pattern_view(pattern1));

            REQUIRE(std::abs(tests_vec0[0].get_a()[0] - 4.0) < 0.00001);
            REQUIRE(std::abs(tests_vec0[1].get_a()[0] - 5.0) < 0.00001);

            REQUIRE(std::abs(tests_vec0[0].get_b() - 2.0) < 0.00001);
            REQUIRE(std::abs(tests_vec0[1].get_b() - 3.0) < 0.00001);

            REQUIRE(tests_vec1[0].get_c() == 'y');
            REQUIRE(tests_vec1[1].get_c() == 'z');

            REQUIRE(tests_vec1[0].get_d() == 5);
            REQUIRE(tests_vec1[1].get_d() == 6);
        }
    }

    // SECTION("Sending multiple stl containers with multiple Patterns using isend and probe")
    // {
    //     class TestTestClass
    //     {
    //     public:
    //         TestTestClass() = default;
    //         TestTestClass(std::array<double, 2> a, double b, char c, int d)
    //             : _a(a), _b(b), _c(c), _d(d)
    //         {}
    
    //         auto& get_a() { return _a; }
    //         auto& get_b() { return _b; }
    //         auto& get_c() { return _c; }
    //         auto& get_d() { return _d; }
    
    //     private:
    //         std::array<double, 2> _a;
    //         double _b;
    //         char _c;
    //         int _d;
    //     };

    //     constexpr mppi::Pattern<TestTestClass, "_a", "_b"> pattern0;
    //     constexpr mppi::Pattern<TestTestClass, "_c", "_d"> pattern1;

    //     std::vector<TestTestClass> tests_vec0;
    //     std::vector<TestTestClass> tests_vec1;
        
    //     if (comm.get_rank() == 0)
    //     {
    //         auto tuple0 = std::tuple{pattern0, tests_vec0 | std::ranges::views::all};
    //         auto tuple1 = std::tuple{pattern1, tests_vec1 | std::ranges::views::all};

    //         tests_vec0.emplace_back(TestTestClass({4.0, 5.0}, 2.0, 'w', 3));
    //         tests_vec0.emplace_back(TestTestClass({5.0, 6.0}, 3.0, 'x', 4));
    //         tests_vec1.emplace_back(TestTestClass({6.0, 7.0}, 4.0, 'y', 5));
    //         tests_vec1.emplace_back(TestTestClass({7.0, 8.0}, 5.0, 'z', 6));

    //         auto request = comm.isend(mppi::Destination(1), mppi::Tag(0), tuple0, tuple1);
    //         request.wait();
    //     }
    //     else
    //     {
    //         auto tuple0 = std::tuple{pattern0, std::move(tests_vec0)};
    //         auto tuple1 = std::tuple{pattern1, std::move(tests_vec1)};

    //         auto flag = false;
    //         while(!flag) 
    //         {
    //             flag = comm.iprobe_recv(mppi::Source(0), mppi::Tag(0), tuple0, tuple1);
    //         }

    //         tests_vec0 = std::move(std::get<1>(tuple0));
    //         tests_vec1 = std::move(std::get<1>(tuple1));

    //         REQUIRE(std::abs(tests_vec0[0].get_a()[0] - 4.0) < 0.00001);
    //         REQUIRE(std::abs(tests_vec0[1].get_a()[0] - 5.0) < 0.00001);

    //         REQUIRE(std::abs(tests_vec0[0].get_b() - 2.0) < 0.00001);
    //         REQUIRE(std::abs(tests_vec0[1].get_b() - 3.0) < 0.00001);

    //         REQUIRE(tests_vec1[0].get_c() == 'y');
    //         REQUIRE(tests_vec1[1].get_c() == 'z');

    //         REQUIRE(tests_vec1[0].get_d() == 5);
    //         REQUIRE(tests_vec1[1].get_d() == 6);
    //     }
    // }
}
