#ifndef MPPI_TAG_HPP
#define MPPI_TAG_HPP

namespace mppi
{
    class Tag
    {
    public:
        explicit Tag(int tag)
            : _tag(tag)
        {}
    
        void set(int tag) { _tag = tag; }
        auto get() const { return _tag; }
    
    private:
        int _tag{};
    };
};


#endif