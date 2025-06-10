#include <catch2/catch_test_macros.hpp>
#include "pattern.hpp"
#include "datatype.hpp"

TEST_CASE( "Reflection functionality", "[Reflection]" )
{
    SECTION("Test with non-standard-layout type (v-tables)")
    {
        class X
        {
        public:
            X() = default;
            X(char a, int b, double c)
                : _a(a), _b(b), _c(c)
            {}

            char& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }

            virtual ~X() = default;
        private:
            char _a;
            int _b;
            double _c;
        };

        std::array<X, 3> xs;
        xs[0] = X('1', 2, 3.1);
        xs[1] = X('2', 3, 4.2);
        xs[2] = X('3', 4, 5.6);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, xs);

        // reset original vector
        xs[0] = X{};
        xs[1] = X{};
        xs[2] = X{};

        data.retrieve_data(xs);


        REQUIRE(xs[0].get_a() == '1');
        REQUIRE(xs[1].get_a() == '2');
        REQUIRE(xs[2].get_a() == '3');

        REQUIRE(xs[0].get_b() == 2);
        REQUIRE(xs[1].get_b() == 3);
        REQUIRE(xs[2].get_b() == 4);

        REQUIRE(std::abs(xs[0].get_c() - 3.1) < 0.00001);
        REQUIRE(std::abs(xs[1].get_c() - 4.2) < 0.00001);
        REQUIRE(std::abs(xs[2].get_c() - 5.6) < 0.00001);
    }

    SECTION("Test with non-standard-layout type (v-tables) in non-related class")
    {
        class X
        {
        public:
            X() = default;
            X(char a, int b)
                : _a(a), _b(b)
            {}

            char& get_a() { return _a; }
            int& get_b() { return _b; }

            virtual ~X() = default;
        private:
            char _a;
            int _b;
        };

        class Y
        {
        public:
            Y() = default;
            Y(char a, int b, double c)
                : _x(a, b), _c(c)
            {}

            auto& get_x() { return _x; }
            auto& get_c() { return _c; }
        private:
            X _x;
            double _c;
        };

        std::array<Y, 3> ys;
        ys[0] = Y('1', 2, 3.1);
        ys[1] = Y('2', 3, 4.2);
        ys[2] = Y('3', 4, 5.6);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, ys);

        // reset original vector
        ys[0] = Y{};
        ys[1] = Y{};
        ys[2] = Y{};

        data.retrieve_data(ys);

        REQUIRE(ys[0].get_x().get_a() == '1');
        REQUIRE(ys[1].get_x().get_a() == '2');
        REQUIRE(ys[2].get_x().get_a() == '3');

        REQUIRE(ys[0].get_x().get_b() == 2);
        REQUIRE(ys[1].get_x().get_b() == 3);
        REQUIRE(ys[2].get_x().get_b() == 4);

        REQUIRE(std::abs(ys[0].get_c() - 3.1) < 0.00001);
        REQUIRE(std::abs(ys[1].get_c() - 4.2) < 0.00001);
        REQUIRE(std::abs(ys[2].get_c() - 5.6) < 0.00001);
    }

    SECTION("Test with non-standard-layout type (derived)")
    {
        class X
        {
        public:
            X() = default;
            X(char a, int b, double c)
                : _a(a), _b(b), _c(c)
            {}

            char& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }
        protected:
            char _a {};
            int _b {};
            double _c {};
        };

        class Y : public X
        {
        public:
            Y() = default;
            Y(char a, int b, double c, float d)
                : X(a, b, c), _d(d)
            {}

            float& get_d() { return _d; }
        private:
            float _d {};
        };

        std::array<Y, 3> ys;
        ys[0] = Y('1', 2, 3.1, 6.7f);
        ys[1] = Y('2', 3, 4.2, 7.4f);
        ys[2] = Y('3', 4, 5.6, 8.2f);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, ys);

        // create array into which packed data can be copied
        std::array<Y, 3> copy_ys;

        data.retrieve_data(copy_ys);

        REQUIRE(copy_ys[0].get_a() == '1');
        REQUIRE(copy_ys[1].get_a() == '2');
        REQUIRE(copy_ys[2].get_a() == '3');

        REQUIRE(copy_ys[0].get_b() == 2);
        REQUIRE(copy_ys[1].get_b() == 3);
        REQUIRE(copy_ys[2].get_b() == 4);

        REQUIRE(std::abs(copy_ys[0].get_c() - 3.1) < 0.00001);
        REQUIRE(std::abs(copy_ys[1].get_c() - 4.2) < 0.00001);
        REQUIRE(std::abs(copy_ys[2].get_c() - 5.6) < 0.00001);

        REQUIRE(std::abs(copy_ys[0].get_d() - 6.7f) < 0.00001f);
        REQUIRE(std::abs(copy_ys[1].get_d() - 7.4f) < 0.00001f);
        REQUIRE(std::abs(copy_ys[2].get_d() - 8.2f) < 0.00001f);
    }

    SECTION("Test with non-standard-layout type (multi-level derived)")
    {
        class X
        {
        public:
            X() = default;
            X(char a, int b, double c)
                : _a(a), _b(b), _c(c)
            {}

            char& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }
        protected:
            char _a {};
            int _b {};
            double _c {};
        };

        class Y : public X
        {
        public:
            Y() = default;
            Y(char a, int b, double c, float d)
                : X(a, b, c), _d(d)
            {}

            float& get_d() { return _d; }
        private:
            float _d {};
        };

        class Z : public Y
        {
        public:
            Z() = default;
            Z(char a, int b, double c, float d, short e)
                : Y(a, b, c, d), _e(e)
            {}

            short& get_e() { return _e; }
        private:
            short _e {};
        };

        std::array<Z, 1> zs;
        zs[0] = Z('1', 2, 3.1, 6.7f, 8);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, zs);

        std::array<Z, 1> copy_zs;

        data.retrieve_data(copy_zs);

        REQUIRE(copy_zs[0].get_a() == '1');
        REQUIRE(copy_zs[0].get_b() == 2);
        REQUIRE(std::abs(copy_zs[0].get_c() - 3.1) < 0.00001);
        REQUIRE(std::abs(copy_zs[0].get_d() - 6.7f) < 0.00001f);
        REQUIRE(copy_zs[0].get_e() == 8);
    }

    SECTION("Test with standard-layout type")
    {
        class X
        {
        public:
            X() = default;
            X(char a, int b, double c)
                : _a(a), _b(b), _c(c)
            {}

            char& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }

        private:
            char _a;
            int _b;
            double _c;
        };

        std::array<X, 3> xs;
        xs[0] = X('1', 2, 3.1);
        xs[1] = X('2', 3, 4.2);
        xs[2] = X('3', 4, 5.6);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, xs);

        std::array<X, 3> xs_copy;

        data.retrieve_data(xs_copy);

        REQUIRE(xs_copy[0].get_a() == '1');
        REQUIRE(xs_copy[1].get_a() == '2');
        REQUIRE(xs_copy[2].get_a() == '3');

        REQUIRE(xs_copy[0].get_b() == 2);
        REQUIRE(xs_copy[1].get_b() == 3);
        REQUIRE(xs_copy[2].get_b() == 4);

        REQUIRE(std::abs(xs_copy[0].get_c() - 3.1) < 0.00001);
        REQUIRE(std::abs(xs_copy[1].get_c() - 4.2) < 0.00001);
        REQUIRE(std::abs(xs_copy[2].get_c() - 5.6) < 0.00001);
    }

    SECTION("Test with standard-layout type, multiple ranges")
    {
        class X
        {
        public:
            X() = default;
            X(char a, int b, double c)
                : _a(a), _b(b), _c(c)
            {}

            char& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }

        private:
            char _a;
            int _b;
            double _c;
        };

        std::array<X, 3> xs0;
        xs0[0] = X('1', 2, 3.1);
        xs0[1] = X('2', 3, 4.2);
        xs0[2] = X('3', 4, 5.6);

        std::array<X, 3> xs1;
        xs1[0] = X('1', 2, 3.1);
        xs1[1] = X('2', 3, 4.2);
        xs1[2] = X('3', 4, 5.6);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, xs0, xs1);

        std::array<X, 3> xs0_copy;
        std::array<X, 3> xs1_copy;

        data.retrieve_data(xs0_copy, xs1_copy);

        REQUIRE(xs0_copy[0].get_a() == '1');
        REQUIRE(xs0_copy[1].get_a() == '2');
        REQUIRE(xs0_copy[2].get_a() == '3');

        REQUIRE(xs0_copy[0].get_b() == 2);
        REQUIRE(xs0_copy[1].get_b() == 3);
        REQUIRE(xs0_copy[2].get_b() == 4);

        REQUIRE(std::abs(xs0_copy[0].get_c() - 3.1) < 0.00001);
        REQUIRE(std::abs(xs0_copy[1].get_c() - 4.2) < 0.00001);
        REQUIRE(std::abs(xs0_copy[2].get_c() - 5.6) < 0.00001);

        REQUIRE(xs1_copy[0].get_a() == '1');
        REQUIRE(xs1_copy[1].get_a() == '2');
        REQUIRE(xs1_copy[2].get_a() == '3');

        REQUIRE(xs1_copy[0].get_b() == 2);
        REQUIRE(xs1_copy[1].get_b() == 3);
        REQUIRE(xs1_copy[2].get_b() == 4);

        REQUIRE(std::abs(xs1_copy[0].get_c() - 3.1) < 0.00001);
        REQUIRE(std::abs(xs1_copy[1].get_c() - 4.2) < 0.00001);
        REQUIRE(std::abs(xs1_copy[2].get_c() - 5.6) < 0.00001);
    }

    SECTION("ReflConcepts")
    {
        class X
        {
        public:
            X() = default;
            X(char a, int b, double c)
                : _a(a), _b(b), _c(c)
            {}

            char& get_a() { return _a; }
            int& get_b() { return _b; }
            double& get_c() { return _c; }

        private:
            char _a;
            int _b;
            double _c;
            std::array<int,3> _d;
        };

        class Y
        {
        public:
            // virtual ~Y() = default;
            char _a;
            int _b;
            double _c;
        };

        // static_assert(has_only_trivially_copyable_types<X, "_a", "_b", "_c", "_d">);
        // static_assert(is_underlying_trivially_copyable<std::list<Y>>);
    }

    SECTION("Test with dynamic allocator container in class")
    {
        class X
        {
        public:
            X() = default;
            X(char a, std::vector<int> d)
                : _a(a), _d(d)
            {}

            auto& get_a() { return _a; }
            auto& get_d() { return _d; }

        private:
            char _a;
            std::vector<int> _d;
        };

        std::array<X, 3> xs;
        xs[0] = X('a', {3, 1, 5});
        xs[1] = X('b', {4, 2, 6});
        xs[2] = X('c', {5, 6, 7});

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, xs);

        std::array<X, 3> xs_copy;
        xs_copy[0] = X(' ', {0, 0, 0});
        xs_copy[1] = X(' ', {0, 0, 0});
        xs_copy[2] = X(' ', {0, 0, 0});

        data.retrieve_data(xs_copy);

        REQUIRE(xs_copy[0].get_a() == 'a');
        REQUIRE(xs_copy[1].get_a() == 'b');
        REQUIRE(xs_copy[2].get_a() == 'c');

        REQUIRE(xs_copy[0].get_d()[0] == 3);
        REQUIRE(xs_copy[1].get_d()[0] == 4);
        REQUIRE(xs_copy[2].get_d()[0] == 5);

        REQUIRE(xs_copy[0].get_d()[1] == 1);
        REQUIRE(xs_copy[1].get_d()[1] == 2);
        REQUIRE(xs_copy[2].get_d()[1] == 6);

        REQUIRE(xs_copy[0].get_d()[2] == 5);
        REQUIRE(xs_copy[1].get_d()[2] == 6);
        REQUIRE(xs_copy[2].get_d()[2] == 7);
    }

    SECTION("Test with differently-sized dynamic allocator container in class")
    {
        class X
        {
        public:
            X() = default;
            X(char a, std::vector<int> d)
                : _a(a), _d(d)
            {}

            auto& get_a() { return _a; }
            auto& get_d() { return _d; }

        private:
            char _a;
            std::vector<int> _d;
        };

        std::array<X, 3> xs;
        xs[0] = X('a', {3, 1});
        xs[1] = X('b', {4, 2, 6, 12});
        xs[2] = X('c', {5, 6, 7});

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, xs);

        std::array<X, 3> xs_copy;
        xs_copy[0] = X(' ', {0, 0});
        xs_copy[1] = X(' ', {0, 0, 0, 0});
        xs_copy[2] = X(' ', {0, 0, 0});

        data.retrieve_data(xs_copy);

        REQUIRE(xs_copy[0].get_a() == 'a');
        REQUIRE(xs_copy[1].get_a() == 'b');
        REQUIRE(xs_copy[2].get_a() == 'c');

        REQUIRE(xs_copy[0].get_d()[0] == 3);
        REQUIRE(xs_copy[1].get_d()[0] == 4);
        REQUIRE(xs_copy[2].get_d()[0] == 5);

        REQUIRE(xs_copy[0].get_d()[1] == 1);
        REQUIRE(xs_copy[1].get_d()[1] == 2);
        REQUIRE(xs_copy[2].get_d()[1] == 6);

        REQUIRE(xs_copy[1].get_d()[2] == 6);
        REQUIRE(xs_copy[2].get_d()[2] == 7);

        REQUIRE(xs_copy[1].get_d()[3] == 12);
    }

    SECTION("Test with differently-sized dynamic allocator non-consecutive container in class")
    {
        class X
        {
        public:
            X() = default;
            X(char a, std::list<int> d)
                : _a(a), _d(d)
            {}

            auto& get_a() { return _a; }
            auto& get_d() { return _d; }

        private:
            char _a;
            std::list<int> _d;
        };

        std::array<X, 3> xs;
        xs[0] = X('a', {3, 1});
        xs[1] = X('b', {4, 2, 6, 12});
        xs[2] = X('c', {5, 6, 7});

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, xs);

        std::array<X, 3> xs_copy;
        xs_copy[0] = X(' ', {0, 0});
        xs_copy[1] = X(' ', {0, 0, 0, 0});
        xs_copy[2] = X(' ', {0, 0, 0});

        data.retrieve_data(xs_copy);

        REQUIRE(xs_copy[0].get_a() == 'a');
        REQUIRE(xs_copy[1].get_a() == 'b');
        REQUIRE(xs_copy[2].get_a() == 'c');

        auto iter0 = xs_copy[0].get_d().begin();
        auto iter1 = xs_copy[1].get_d().begin();
        auto iter2 = xs_copy[2].get_d().begin();

        REQUIRE(*iter0++ == 3);
        REQUIRE(*iter1++ == 4);
        REQUIRE(*iter2++ == 5);

        REQUIRE(*iter0++ == 1);
        REQUIRE(*iter1++ == 2);
        REQUIRE(*iter2++ == 6);

        REQUIRE(*iter1++ == 6);
        REQUIRE(*iter2++ == 7);

        REQUIRE(*iter1++ == 12);
    }

    SECTION("Test with dynamic allocator container in base class")
    {
        class X
        {
        public:
            X() = default;
            X(char a, std::vector<int> b)
                : _a(a), _b(b)
            {}

            auto& get_a() { return _a; }
            auto& get_b() { return _b; }

        protected:
            char _a;
            std::vector<int> _b;
        };

        class Y : public X
        {
        public:
            Y() = default;
            Y(char a, std::vector<int> b, double c)
                : X(a, b), _c(c)
            {}

            auto& get_c() { return _c; }

        private:
            double _c;
        };

        std::array<Y, 3> ys;
        ys[0] = Y('a', {3, 1, 5}, 0.7);
        ys[1] = Y('b', {4, 2, 6}, 8.9);
        ys[2] = Y('c', {5, 6, 7}, 12.2);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, ys);

        std::array<Y, 3> ys_copy;
        ys_copy[0] = Y(' ', {0, 0, 0}, 0.0);
        ys_copy[1] = Y(' ', {0, 0, 0}, 0.0);
        ys_copy[2] = Y(' ', {0, 0, 0}, 0.0);

        data.retrieve_data(ys_copy);

        REQUIRE(ys_copy[0].get_a() == 'a');
        REQUIRE(ys_copy[1].get_a() == 'b');
        REQUIRE(ys_copy[2].get_a() == 'c');

        REQUIRE(ys_copy[0].get_b()[0] == 3);
        REQUIRE(ys_copy[1].get_b()[0] == 4);
        REQUIRE(ys_copy[2].get_b()[0] == 5);

        REQUIRE(ys_copy[0].get_b()[1] == 1);
        REQUIRE(ys_copy[1].get_b()[1] == 2);
        REQUIRE(ys_copy[2].get_b()[1] == 6);

        REQUIRE(ys_copy[0].get_b()[2] == 5);
        REQUIRE(ys_copy[1].get_b()[2] == 6);
        REQUIRE(ys_copy[2].get_b()[2] == 7);

        REQUIRE(std::abs(ys_copy[0].get_c() - 0.7) < 0.00001);
        REQUIRE(std::abs(ys_copy[1].get_c() - 8.9) < 0.00001);
        REQUIRE(std::abs(ys_copy[2].get_c() - 12.2) < 0.00001);
    }

    SECTION("Test with dynamic allocator container in not-related class")
    {
        class X
        {
        public:
            X() = default;
            X(double a, std::vector<char> b)
                : _a(a), _b(b)
            {}

            auto& get_a() { return _a; }
            auto& get_b() { return _b; }

        protected:
            double _a;
            std::vector<char> _b;
        };

        class Y
        {
        public:
            Y() = default;
            Y(double a, std::vector<char> b, short c)
                : _x(a, b), _c(c)
            {}

            auto& get_x() { return _x; }
            auto& get_c() { return _c; }

        private:
            X _x;
            short _c;
        };

        std::array<Y, 3> ys;
        ys[0] = Y(1.2, {'3', '1'}, 42);
        ys[1] = Y(2.4, {'4', '2', '6', '9'}, 99);
        ys[2] = Y(3.6, {'5', '6', '7'}, 123);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, ys);

        std::array<Y, 3> ys_copy;
        ys_copy[0] = Y(0.0, {' ', ' '}, 0);
        ys_copy[1] = Y(0.0, {' ', ' ', ' ', ' '}, 0);
        ys_copy[2] = Y(0.0, {' ', ' ', ' '}, 0);

        data.retrieve_data(ys_copy);

        REQUIRE(std::abs(ys_copy[0].get_x().get_a() - 1.2) < 0.00001);
        REQUIRE(std::abs(ys_copy[1].get_x().get_a() - 2.4) < 0.00001);
        REQUIRE(std::abs(ys_copy[2].get_x().get_a() - 3.6) < 0.00001);

        REQUIRE(ys_copy[0].get_x().get_b()[0] == '3');
        REQUIRE(ys_copy[1].get_x().get_b()[0] == '4');
        REQUIRE(ys_copy[2].get_x().get_b()[0] == '5');

        REQUIRE(ys_copy[0].get_x().get_b()[1] == '1');
        REQUIRE(ys_copy[1].get_x().get_b()[1] == '2');
        REQUIRE(ys_copy[2].get_x().get_b()[1] == '6');

        REQUIRE(ys_copy[1].get_x().get_b()[2] == '6');
        REQUIRE(ys_copy[2].get_x().get_b()[2] == '7');

        REQUIRE(ys_copy[1].get_x().get_b()[3] == '9');

        REQUIRE(ys_copy[0].get_c() == 42);
        REQUIRE(ys_copy[1].get_c() == 99);
        REQUIRE(ys_copy[2].get_c() == 123);
    }

    SECTION("Test with dynamic allocator container in base of not-related class")
    {
        class X
        {
        public:
            X() = default;
            X( std::vector<int> a)
                : _a(a)
            {}

            auto& get_a() { return _a; }

        protected:
            std::vector<int> _a;
        };

        class Y : public X
        {
        public:
            Y() = default;
            Y(std::vector<int> a, float b)
                : X(a), _b(b)
            {}

            auto& get_b() { return _b; }

        private:
            float _b;
        };

        class Z
        {
        public:
            Z() = default;
            Z(std::vector<int> a, float b, short c)
                : _y(a, b), _c(c)
            {}

            auto& get_y() { return _y; }
            auto& get_c() { return _c; }

        private:
            Y _y;
            short _c;
        };

        std::array<Z, 3> zs;
        zs[0] = Z({1, 2, 3}, 3.1f, 42);
        zs[1] = Z({4}, 4.26f, 99);
        zs[2] = Z({30, 40, 50, 60, 70}, 5.6f, 123);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, zs);

        std::array<Z, 3> zs_copy;
        zs_copy[0] = Z({0, 0, 0}, 0.0f, 0);
        zs_copy[1] = Z({0}, 0.0f, 0);
        zs_copy[2] = Z({0, 0, 0, 0, 0}, 0.0f, 0);

        data.retrieve_data(zs_copy);

        REQUIRE(zs_copy[0].get_y().get_a()[0] == 1);
        REQUIRE(zs_copy[1].get_y().get_a()[0] == 4);
        REQUIRE(zs_copy[2].get_y().get_a()[0] == 30);

        REQUIRE(zs_copy[0].get_y().get_a()[1] == 2);
        REQUIRE(zs_copy[2].get_y().get_a()[1] == 40);

        REQUIRE(zs_copy[0].get_y().get_a()[2] == 3);
        REQUIRE(zs_copy[2].get_y().get_a()[2] == 50);

        REQUIRE(zs_copy[2].get_y().get_a()[3] == 60);
        REQUIRE(zs_copy[2].get_y().get_a()[4] == 70);

        REQUIRE(std::abs(zs_copy[0].get_y().get_b() - 3.1f) < 0.00001f);
        REQUIRE(std::abs(zs_copy[1].get_y().get_b() - 4.26f) < 0.00001f);
        REQUIRE(std::abs(zs_copy[2].get_y().get_b() - 5.6f) < 0.00001f);

        REQUIRE(zs_copy[0].get_c() == 42);
        REQUIRE(zs_copy[1].get_c() == 99);
        REQUIRE(zs_copy[2].get_c() == 123);
    }

    SECTION("Test with complicated standard-layout type")
    {
        class A
        {
        public:
            A() = default;
            A(char a0, double a1)
                : _a0(a0), _a1(a1)
            {}

            auto& get_a0() { return _a0; }
            auto& get_a1() { return _a1; }
        protected:
            char _a0;
            double _a1;
        };

        class X
        {
        public:
            X() = default;
            X(char x0, double x1)
                : _x(x0, x1)
            {}

            auto& get_x() { return _x; }
        protected:
            A _x;
        };

        class Y : public X
        {
        public:
            Y() = default;
            Y(char x0, double x1)
                : X(x0, x1)
            {}
        };

        class Z
        {
        public:
            Z() = default;
            Z(int z)
                : _z(z)
            {}
            auto& get_z() { return _z; }
        protected:
            int _z;
        };

        class C
        {
        public:
            C() = default;
            C(short c)
                : _c(c)
            {}
            auto& get_c() { return _c; }
        protected:
            short _c;
        };

        class B : public C
        {
        public:
            B() = default;
            B(short c, size_t b)
                : C(c), _b(b)
            {}

            auto& get_b() { return _b; } 
        private:
            size_t _b;
        };

        class Origin : public Z, public Y
        {
        public:
            Origin() = default;
            Origin(short b0, size_t b1, int z, char y0, double y1)
                : Z(z), Y(y0, y1), _b(b0, b1)
            {}

            auto& get_b() { return _b; }
        private:
            B _b;
        };

        std::array<Origin, 1> xs;
        xs[0] = Origin(0, 1, 2, '3', 4.0);

        std::pmr::monotonic_buffer_resource mem_res {};
        mppi::Data data(mppi::Send{}, mem_res, xs);

        std::array<Origin, 1> xs_copy;

        data.retrieve_data(xs_copy);

        REQUIRE(xs_copy[0].get_b().get_b() == 1);
        REQUIRE(xs_copy[0].get_b().get_c() == 0);

        REQUIRE(xs_copy[0].get_z() == 2);

        REQUIRE(xs_copy[0].get_x().get_a0() == '3');
        REQUIRE(std::abs(xs_copy[0].get_x().get_a1() - 4.0) < 0.00001);
    }

    SECTION("Test with differently-sized dynamic allocator container in class, addressed with Identifiers")
    {
        class X
        {
        public:
            X() = default;
            X(char a, std::vector<int> d)
                : _a(a), _d(d)
            {}

            auto& get_a() { return _a; }
            auto& get_d() { return _d; }

        private:
            char _a;
            std::vector<int> _d;
        };

        std::array<X, 3> xs;
        xs[0] = X('a', {3, 1});
        xs[1] = X('b', {4, 2, 6, 12});
        xs[2] = X('c', {5, 6, 7});

        std::pmr::monotonic_buffer_resource mem_res {};

        mppi::Pattern<X, "_a", "_d"> pattern(xs);

        mppi::Data data(mppi::Send{}, mem_res, pattern, xs);

        std::array<X, 3> xs_copy;
        xs_copy[0] = X(' ', {0, 0});
        xs_copy[1] = X(' ', {0, 0, 0, 0});
        xs_copy[2] = X(' ', {0, 0, 0});

        data.retrieve_data(pattern, xs_copy);

        REQUIRE(xs_copy[0].get_a() == 'a');
        REQUIRE(xs_copy[1].get_a() == 'b');
        REQUIRE(xs_copy[2].get_a() == 'c');

        REQUIRE(xs_copy[0].get_d()[0] == 3);
        REQUIRE(xs_copy[1].get_d()[0] == 4);
        REQUIRE(xs_copy[2].get_d()[0] == 5);

        REQUIRE(xs_copy[0].get_d()[1] == 1);
        REQUIRE(xs_copy[1].get_d()[1] == 2);
        REQUIRE(xs_copy[2].get_d()[1] == 6);

        REQUIRE(xs_copy[1].get_d()[2] == 6);
        REQUIRE(xs_copy[2].get_d()[2] == 7);

        REQUIRE(xs_copy[1].get_d()[3] == 12);
    }
}