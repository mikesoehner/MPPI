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
}
