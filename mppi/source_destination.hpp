#ifndef MPPI_Source_Destination_HPP
#define MPPI_Source_Destination_HPP

namespace mppi
{
    class Source
    {
    public:
        explicit Source(int source)
            : _source(source)
        {}
    
        void set(int source) { _source = source; }
        auto get() const { return _source; }
    
    private:
        int _source{};
    };

    class Destination
    {
    public:
        explicit Destination(int destination)
            : _destination(destination)
        {}
    
        void set(int destination) { _destination = destination; }
        auto get() const { return _destination; }
    
    private:
        int _destination{};
    };
};


#endif