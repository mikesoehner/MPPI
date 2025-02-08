#ifndef MPPI_TAG_H
#define MPPI_TAG_H

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


#endif