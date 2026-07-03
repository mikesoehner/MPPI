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

        TestClass testpattern;
        mppi::Pattern pattern(&testpattern, testpattern.get_a(), testpattern.get_d());

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

        TestClass testpattern;
        mppi::Pattern pattern(&testpattern, testpattern.get_a(), testpattern.get_d());

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

        TestClass testpattern;
        mppi::Pattern pattern(&testpattern, testpattern.get_a(), testpattern.get_d());
        
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

        TestClass testpattern;
        mppi::Pattern pattern(&testpattern, testpattern.get_a(), testpattern.get_b(), testpattern.get_d());

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

        TestClass test_class;
        mppi::Pattern pattern(&test_class, test_class.get_a(), test_class.get_d());

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

        TestTestClass test_class;
        mppi::Pattern pattern0(&test_class, test_class.get_a(), test_class.get_b());
        mppi::Pattern pattern1(&test_class, test_class.get_c(), test_class.get_d());

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

    SECTION("ISending an entire stl container with a new Pattern")
    {
        constexpr mppi::Pattern<int, 
                    mppi::Sizes<4, 4>, 
                    mppi::Subsizes<2, 3>, 
                    mppi::Starts<2, 1>> pattern;

        std::vector<std::array<int, 16>> tests_vec(1);

        if (comm.get_rank() == 0)
        {
            tests_vec[0][0] = 0;
            tests_vec[0][1] = 1;
            tests_vec[0][2] = 2;
            tests_vec[0][3] = 3;
            tests_vec[0][4] = 4;
            tests_vec[0][5] = 5;
            tests_vec[0][6] = 6;
            tests_vec[0][7] = 7;
            tests_vec[0][8] = 8;
            tests_vec[0][9] = 9;
            tests_vec[0][10] = 10;
            tests_vec[0][11] = 11;
            tests_vec[0][12] = 12;
            tests_vec[0][13] = 13;
            tests_vec[0][14] = 14;
            tests_vec[0][15] = 15;

            auto request = comm.isend(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
            request.wait();
        }
        else
        {
            auto request = comm.irecv(mppi::Source(0), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
            request.wait();

            REQUIRE(tests_vec[0][0] == 0);
            REQUIRE(tests_vec[0][1] == 0);
            REQUIRE(tests_vec[0][2] == 0);
            REQUIRE(tests_vec[0][3] == 0);
            REQUIRE(tests_vec[0][4] == 0);
            REQUIRE(tests_vec[0][5] == 0);
            REQUIRE(tests_vec[0][6] == 0);
            REQUIRE(tests_vec[0][7] == 0);
            REQUIRE(tests_vec[0][8] == 0);
            REQUIRE(tests_vec[0][9] == 9);
            REQUIRE(tests_vec[0][10] == 10);
            REQUIRE(tests_vec[0][11] == 11);
            REQUIRE(tests_vec[0][12] == 0);
            REQUIRE(tests_vec[0][13] == 13);
            REQUIRE(tests_vec[0][14] == 14);
            REQUIRE(tests_vec[0][15] == 15);
        }
    }

    SECTION("Advanced ISending an entire stl container with a new Pattern")
    {
        class Dummy
        {
        public:
            std::array<int, 16> _x;
        };

        constexpr mppi::Pattern<int,
                    mppi::Sizes<4, 4, 4, 4>,
                    mppi::Subsizes<2, 1, 3, 2>,
                    mppi::Starts<0, 2, 1, 0>> pattern;


        if (comm.get_rank() == 0)
        {
            std::vector<std::array<std::array<std::array<std::array<int, 4>, 4>, 4>, 4>> tests_vec(1);
            /* Fill with recognizable values */
            for (int t = 0; t < 4; t++)
            for (int z = 0; z < 4; z++)
            for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
            {    
                tests_vec[0][t][z][y][x] =
                    1000*t + 100*z + 10*y + x;
                // std::cerr << "Rank0: " << tests_vec[0][t][z][y][x] << std::endl;
            }

            auto request = comm.isend(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
            request.wait();
        }
        else
        {
            std::array<int, 12> tests_vec;
            auto request = comm.irecv(mppi::Source(0), mppi::Tag(0), tests_vec | std::ranges::views::all);
            request.wait();

            REQUIRE(tests_vec[0] == 210);
            REQUIRE(tests_vec[1] == 211);
            REQUIRE(tests_vec[2] == 220);
            REQUIRE(tests_vec[3] == 221);
            REQUIRE(tests_vec[4] == 230);
            REQUIRE(tests_vec[5] == 231);
            REQUIRE(tests_vec[6] == 1210);
            REQUIRE(tests_vec[7] == 1211);
            REQUIRE(tests_vec[8] == 1220);
            REQUIRE(tests_vec[9] == 1221);
            REQUIRE(tests_vec[10] == 1230);
            REQUIRE(tests_vec[11] == 1231);
        }
    }

    SECTION("Advanced ISending an entire stl container with a new Pattern using Fortran order")
    {
        constexpr mppi::Pattern<int, 
                    mppi::Sizes<4, 4, 4, 4>,
                    mppi::Subsizes<2, 1, 3, 2>,
                    mppi::Starts<0, 2, 1, 0>> pattern;


        if (comm.get_rank() == 0)
        {
            std::vector<std::array<std::array<std::array<std::array<int, 4>, 4>, 4>, 4>> tests_vec(1);
            /* Fill with recognizable values */
            for (int t = 0; t < 4; t++)
            for (int z = 0; z < 4; z++)
            for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
            {    
                tests_vec[0][t][z][y][x] =
                    1000*t + 100*z + 10*y + x;
                // std::cerr << "Rank0: " << tests_vec[0][t][z][y][x] << std::endl;
            }

            auto request = comm.isend(mppi::Destination(1), mppi::Tag(0), tests_vec | mppi::pattern_view(pattern));
            request.wait();
        }
        else
        {
            std::array<int, 12> tests_vec;
            auto request = comm.irecv(mppi::Source(0), mppi::Tag(0), tests_vec | std::ranges::views::all);
            request.wait();

            REQUIRE(tests_vec[0] == 210);
            REQUIRE(tests_vec[1] == 211);
            REQUIRE(tests_vec[2] == 220);
            REQUIRE(tests_vec[3] == 221);
            REQUIRE(tests_vec[4] == 230);
            REQUIRE(tests_vec[5] == 231);
            REQUIRE(tests_vec[6] == 1210);
            REQUIRE(tests_vec[7] == 1211);
            REQUIRE(tests_vec[8] == 1220);
            REQUIRE(tests_vec[9] == 1221);
            REQUIRE(tests_vec[10] == 1230);
            REQUIRE(tests_vec[11] == 1231);
        }
    }
}