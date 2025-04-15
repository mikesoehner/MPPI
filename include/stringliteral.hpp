#ifndef MPPI_STRINGLITERAL_HPP
#define MPPI_STRINGLITERAL_HPP

#include <algorithm>
#include <string_view>
#include <string>
#include <array>

template<size_t N>
struct StringLiteral {
    consteval StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value.data());
    }

    consteval StringLiteral(std::string str) {
        // std::copy_n(str, N, value);
        std::copy(str.begin(), str.end(), value.begin());
        // std::copy_n(str.data(), N, value.data());
    }

    consteval StringLiteral(std::array<char, N> arr) {
        // std::copy_n(arr.begin(), N, value.begin());
        value = arr;
    }

    consteval bool operator==(const StringLiteral<N> str) const {
        return std::equal(str.value.begin(), str.value.end(), value.begin());
    }

    template<std::size_t N2>
    consteval bool operator==(const StringLiteral<N2> s) const {
        return false;
    }

    consteval bool operator==(std::string_view const str_view) const {
        return std::string(value.data()) == str_view;
    }
    
    std::array<char, N> value;
};

template<std::size_t s1>
consteval auto operator==(StringLiteral<s1> fs, std::string_view const str_view) {
    return std::string(fs.value.data()) == str_view;
}

template<std::size_t s2>
consteval auto operator==(std::string_view const str_view, StringLiteral<s2> fs) {
    return str_view == std::string(fs.value.data());
}


#endif