#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "pattern.hpp"
#include "datatype.hpp"
#include "communicator.hpp"


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


TEST_CASE( "Pattern class functionality on GPU", "[GPU-Pattern]" )
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

    SECTION("Basic Test on GPU")
    {
        std::vector<Test, GPUAllocatorBackend<Test>> test(1);

        test[0] = Test(1, 2, 3.0, {4.0f, 5.0f, 6.0f});

        Test dummy;
        mppi::Pattern pattern(&dummy, dummy.get_a(), dummy.get_d());

        struct Destination
        {
            int a {};
            std::array<float, 3> d{};
        };

        std::vector<Destination, GPUAllocatorBackend<Destination>> dest(1);

        auto segment_offsets = pattern.get_segment_offsets();
        auto segment_sizes = pattern.get_segment_sizes();
        std::vector<int, GPUAllocatorBackend<int>> segment_data(segment_offsets.size() + segment_sizes.size());
        auto nb_segments = pattern.calc_nb_segments();

        gpu_kernel::gpu_copy_H2D(segment_data.data()                         , segment_offsets.data(), segment_offsets.size() * sizeof(int));
        gpu_kernel::gpu_copy_H2D(segment_data.data() + segment_offsets.size(), segment_sizes.data(), segment_sizes.size() * sizeof(int));
        
        pattern.pack_GPU(reinterpret_cast<std::byte*>(dest.data()), test, segment_data.data(), segment_data.data() + nb_segments);
        gpu_kernel::gpu_device_synchronize();

        REQUIRE(1 == dest[0].a);
        REQUIRE(std::abs(4.0f - dest[0].d[0]) < 0.0001f);
        REQUIRE(std::abs(5.0f - dest[0].d[1]) < 0.0001f);
        REQUIRE(std::abs(6.0f - dest[0].d[2]) < 0.0001f);
    }

    SECTION("Advanced Test on GPU")
    {
        std::vector<Test, GPUAllocatorBackend<Test>> tests;

        tests.emplace_back(Test(1, 2, 3.0, {4.0f, 5.0f, 6.0f}));
        tests.emplace_back(Test(2, 3, 4.0, {5.0f, 6.0f, 7.0f}));
        tests.emplace_back(Test(3, 4, 5.0, {6.0f, 7.0f, 8.0f}));
        tests.emplace_back(Test(4, 5, 6.0, {7.0f, 8.0f, 9.0f}));

        Test test;
        mppi::Pattern pattern(&test, test.get_a(), test.get_c(), test.get_d());

        mppi::GPUAllocator gpu_allocator;
        std::pmr::monotonic_buffer_resource mem_res {&gpu_allocator};
        bool is_gpu = true;
        auto view = tests | mppi::pattern_view(pattern);
        mppi::Data data(mppi::Send{}, mem_res, is_gpu, view);

        // reset original vector
        tests[0] = Test{};
        tests[1] = Test{};
        tests[2] = Test{};
        tests[3] = Test{};

        view = tests | mppi::pattern_view(pattern);
        data.retrieve_data(view);

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
    
    SECTION("Advanced Test with Subarray Pattern on GPU")
    {
        std::vector<int, GPUAllocatorBackend<int>> tests(16);
        // Fill matrix
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
            {    
                tests[i*4 + j] = i * 10 + j;
                // std::cerr << tests[i*4 + j] << std::endl;
            }

        constexpr mppi::Pattern<int, 
                    mppi::Sizes<4, 4>,
                    mppi::Subsizes<2, 4>,
                    mppi::Starts<2, 0>> pattern;


        mppi::GPUAllocator gpu_allocator;
        std::pmr::monotonic_buffer_resource mem_res {&gpu_allocator};
        bool is_gpu = true;
        auto view = tests | mppi::pattern_view(pattern);
        mppi::Data data(mppi::Send{}, mem_res, is_gpu, view);

        // Reset vector
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                tests[i*4 + j] = 0;

        view = tests | mppi::pattern_view(pattern);
        data.retrieve_data(view);

        // for (int i = 0; i < 4; i++)
        //     for (int j = 0; j < 4; j++)
        //         std::cerr << tests[i*4 + j] << std::endl;

        REQUIRE(tests[8] == 20);
        REQUIRE(tests[9] == 21);
        REQUIRE(tests[10] == 22);
        REQUIRE(tests[11] == 23);

        REQUIRE(tests[12] == 30);
        REQUIRE(tests[13] == 31);
        REQUIRE(tests[14] == 32);
        REQUIRE(tests[15] == 33);
    }
    
    SECTION("Advanced Test with Subarray Pattern on GPU (double)")
    {
        std::vector<double, GPUAllocatorBackend<double>> tests(16);
        // Fill matrix
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
            {    
                tests[i*4 + j] = i * 10 + j;
                // std::cerr << tests[i*4 + j] << std::endl;
            }

        constexpr mppi::Pattern<double, 
                    mppi::Sizes<4, 4>,
                    mppi::Subsizes<2, 4>,
                    mppi::Starts<2, 0>> pattern;


        mppi::GPUAllocator gpu_allocator;
        std::pmr::monotonic_buffer_resource mem_res {&gpu_allocator};
        bool is_gpu = true;
        auto view = tests | mppi::pattern_view(pattern);
        mppi::Data data(mppi::Send{}, mem_res, is_gpu, view);

        // Reset vector
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                tests[i*4 + j] = 0;

        view = tests | mppi::pattern_view(pattern);
        data.retrieve_data(view);

        // for (int i = 0; i < 4; i++)
        //     for (int j = 0; j < 4; j++)
        //         std::cerr << tests[i*4 + j] << std::endl;

        REQUIRE(std::abs(tests[8] - 20) < 0.00001);
        REQUIRE(std::abs(tests[9] - 21) < 0.00001);
        REQUIRE(std::abs(tests[10] - 22) < 0.00001);
        REQUIRE(std::abs(tests[11] - 23) < 0.00001);

        REQUIRE(std::abs(tests[12] - 30) < 0.00001);
        REQUIRE(std::abs(tests[13] - 31) < 0.00001);
        REQUIRE(std::abs(tests[14] - 32) < 0.00001);
        REQUIRE(std::abs(tests[15] - 33) < 0.00001);
    }
    
    SECTION("Advanced Test with Subarray Pattern on GPU (short)")
    {
        std::vector<short int, GPUAllocatorBackend<short int>> tests(16);
        // Fill matrix
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
            {    
                tests[i*4 + j] = i * 10 + j;
                // std::cerr << tests[i*4 + j] << std::endl;
            }

        constexpr mppi::Pattern<short int, 
                    mppi::Sizes<4, 4>,
                    mppi::Subsizes<2, 4>,
                    mppi::Starts<2, 0>> pattern;


        mppi::GPUAllocator gpu_allocator;
        std::pmr::monotonic_buffer_resource mem_res {&gpu_allocator};
        bool is_gpu = true;
        auto view = tests | mppi::pattern_view(pattern);
        mppi::Data data(mppi::Send{}, mem_res, is_gpu, view);

        // Reset vector
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                tests[i*4 + j] = 0;

        view = tests | mppi::pattern_view(pattern);
        data.retrieve_data(view);

        // for (int i = 0; i < 4; i++)
        //     for (int j = 0; j < 4; j++)
        //         std::cerr << tests[i*4 + j] << std::endl;

        REQUIRE(std::abs(tests[8] - 20) < 0.00001);
        REQUIRE(std::abs(tests[9] - 21) < 0.00001);
        REQUIRE(std::abs(tests[10] - 22) < 0.00001);
        REQUIRE(std::abs(tests[11] - 23) < 0.00001);

        REQUIRE(std::abs(tests[12] - 30) < 0.00001);
        REQUIRE(std::abs(tests[13] - 31) < 0.00001);
        REQUIRE(std::abs(tests[14] - 32) < 0.00001);
        REQUIRE(std::abs(tests[15] - 33) < 0.00001);
    }
}