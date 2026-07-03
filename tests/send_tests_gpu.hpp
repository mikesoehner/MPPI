#include <catch2/catch_test_macros.hpp>
#include <communicator.hpp>
#include <numeric>
#include <iostream>
#include <list>
#include <span>


template<typename T>
class GPUAllocatorBackend
{
public:
    typedef T value_type;

    GPUAllocatorBackend() = default;

    template <class U>
    constexpr GPUAllocatorBackend(const GPUAllocatorBackend<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();

        T* ptr;
        
        gpu_kernel::gpu_malloc_managed(reinterpret_cast<void**>(&ptr), n * sizeof(T));

        return ptr;
    }

    void deallocate(T* ptr, std::size_t n) noexcept {
        gpu_kernel::gpu_free(ptr);
    }
};

template<class T, class U>
inline bool operator==(const GPUAllocatorBackend <T>&, const GPUAllocatorBackend <U>&) { return true; }
 
template<class T, class U>
inline bool operator!=(const GPUAllocatorBackend <T>&, const GPUAllocatorBackend <U>&) { return false; }


TEST_CASE( "Send and Recv functionality on GPU", "[send_recv_gpu]" )
{
    // setup to be communicated vector
    std::vector<int, GPUAllocatorBackend<int>> gpu_vec(10);
    std::vector<int> vec(10, 0);

    mppi::Communicator comm;

    SECTION("Sending an entire stl container on GPU")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);

            gpu_kernel::gpu_copy_H2D(gpu_vec.data(), vec.data(), vec.size()*sizeof(int));

            comm.send(mppi::Destination(1), mppi::Tag(0), gpu_vec);

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            // for (auto i : vec)
            //     std::cerr << i << std::endl;

            comm.recv(mppi::Source(0), mppi::Tag(0), gpu_vec);

            gpu_kernel::gpu_copy_D2H(vec.data(), gpu_vec.data(), vec.size()*sizeof(int));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});

            // for (auto i : vec)
            //     std::cerr << i << std::endl;
        }
    }

    SECTION("Sending multiple fractions of an contiguous stl container on GPU")
    {
        if (comm.get_rank() == 0)
        {
            std::iota(vec.begin(), vec.end(), 0);

            gpu_kernel::gpu_copy_H2D(gpu_vec.data(), vec.data(), vec.size()*sizeof(int));

            comm.send(mppi::Destination(1), mppi::Tag(0), gpu_vec | std::ranges::views::take(3), gpu_vec | std::ranges::views::drop(7));

            REQUIRE(vec == std::vector<int> {0,1,2,3,4,5,6,7,8,9});
        }
        else
        {
            comm.recv(mppi::Source(0), mppi::Tag(0), gpu_vec | std::ranges::views::take(3), gpu_vec | std::ranges::views::drop(7));

            gpu_kernel::gpu_copy_D2H(vec.data(), gpu_vec.data(), vec.size()*sizeof(int));

            REQUIRE(vec == std::vector<int> {0,1,2,0,0,0,0,7,8,9});
        }
    }

    SECTION("Sending an entire stl container with a Pattern on GPU")
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

        std::vector<TestClass, GPUAllocatorBackend<TestClass>> gpu_tests_vec;

        if (comm.get_rank() == 0)
        {
            gpu_tests_vec.emplace_back(TestClass(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
            gpu_tests_vec.emplace_back(TestClass(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
            gpu_tests_vec.emplace_back(TestClass(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
            gpu_tests_vec.emplace_back(TestClass(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

            // for (auto& i : gpu_tests_vec)
            // {
            //     std::cerr << i.get_a() << " " << i.get_b() << " " << i.get_c() << " " 
            //             << i.get_d()[0] << " " << i.get_d()[1] << " " << i.get_d()[2] << std::endl;
            // }
            // gpu_kernel::gpu_copy_H2D(gpu_tests_vec.data(), tests_vec.data(), tests_vec.size()*sizeof(TestClass));

            comm.send(mppi::Destination(1), mppi::Tag(0), gpu_tests_vec | mppi::pattern_view(pattern));
        }
        else
        {
            gpu_tests_vec.resize(4);

            comm.recv(mppi::Source(0), mppi::Tag(0), gpu_tests_vec | mppi::pattern_view(pattern));

            // gpu_kernel::gpu_copy_D2H(tests_vec.data(), gpu_tests_vec.data(), gpu_tests_vec.size()*sizeof(TestClass));

            // for (auto& i : gpu_tests_vec)
            // {
            //     std::cerr << i.get_a() << " " << i.get_b() << " " << i.get_c() << " " 
            //             << i.get_d()[0] << " " << i.get_d()[1] << " " << i.get_d()[2] << std::endl;
            // }

            REQUIRE(gpu_tests_vec[0].get_a() == 1);
            REQUIRE(gpu_tests_vec[1].get_a() == 2);
            REQUIRE(gpu_tests_vec[2].get_a() == 3);
            REQUIRE(gpu_tests_vec[3].get_a() == 4);

            REQUIRE(gpu_tests_vec[0].get_b() == 0);
            REQUIRE(gpu_tests_vec[1].get_b() == 0);
            REQUIRE(gpu_tests_vec[2].get_b() == 0);
            REQUIRE(gpu_tests_vec[3].get_b() == 0);

            REQUIRE(std::abs(gpu_tests_vec[0].get_d()[0] - 4.0f) < 0.00001f);
            REQUIRE(std::abs(gpu_tests_vec[1].get_d()[0] - 5.0f) < 0.00001f);
            REQUIRE(std::abs(gpu_tests_vec[2].get_d()[0] - 6.0f) < 0.00001f);
            REQUIRE(std::abs(gpu_tests_vec[3].get_d()[0] - 7.0f) < 0.00001f);
        }
    }

    SECTION("ISending an entire stl container with a new Pattern on GPU")
    {
        constexpr mppi::Pattern<int, 
                    mppi::Sizes<4, 4>, 
                    mppi::Subsizes<2, 3>, 
                    mppi::Starts<2, 1>> pattern;

        std::vector<int, GPUAllocatorBackend<int>> gpu_tests_vec(16);

        if (comm.get_rank() == 0)
        {
            gpu_tests_vec[0] = 0;
            gpu_tests_vec[1] = 1;
            gpu_tests_vec[2] = 2;
            gpu_tests_vec[3] = 3;
            gpu_tests_vec[4] = 4;
            gpu_tests_vec[5] = 5;
            gpu_tests_vec[6] = 6;
            gpu_tests_vec[7] = 7;
            gpu_tests_vec[8] = 8;
            gpu_tests_vec[9] = 9;
            gpu_tests_vec[10] = 10;
            gpu_tests_vec[11] = 11;
            gpu_tests_vec[12] = 12;
            gpu_tests_vec[13] = 13;
            gpu_tests_vec[14] = 14;
            gpu_tests_vec[15] = 15;

            auto request = comm.isend(mppi::Destination(1), mppi::Tag(0), gpu_tests_vec | mppi::pattern_view(pattern));
            request.wait();
        }
        else
        {
            auto request = comm.irecv(mppi::Source(0), mppi::Tag(0), gpu_tests_vec | mppi::pattern_view(pattern));
            request.wait();

            REQUIRE(gpu_tests_vec[0] == 0);
            REQUIRE(gpu_tests_vec[1] == 0);
            REQUIRE(gpu_tests_vec[2] == 0);
            REQUIRE(gpu_tests_vec[3] == 0);
            REQUIRE(gpu_tests_vec[4] == 0);
            REQUIRE(gpu_tests_vec[5] == 0);
            REQUIRE(gpu_tests_vec[6] == 0);
            REQUIRE(gpu_tests_vec[7] == 0);
            REQUIRE(gpu_tests_vec[8] == 0);
            REQUIRE(gpu_tests_vec[9] == 9);
            REQUIRE(gpu_tests_vec[10] == 10);
            REQUIRE(gpu_tests_vec[11] == 11);
            REQUIRE(gpu_tests_vec[12] == 0);
            REQUIRE(gpu_tests_vec[13] == 13);
            REQUIRE(gpu_tests_vec[14] == 14);
            REQUIRE(gpu_tests_vec[15] == 15);
        }
    }
}
