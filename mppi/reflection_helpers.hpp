#ifndef MPPI_REFLECTION_HELPERS_HPP
#define MPPI_REFLECTION_HELPERS_HPP

#include <experimental/meta>
#include <utility>
#include <vector>
#include <array>
#include <string_view>
#include <memory>
#include <type_traits>
#include <ranges>
#include <functional>
#include <algorithm>
#include <string>
#include <iterator>

#include "stringliteral.hpp"

namespace mppi 
{
    /* ------------------------------ Function that help with Reflection via expansion ------------------------------ */

    namespace __impl {
        template<auto... vals>
        struct replicator_type {
        template<typename F>
            constexpr void operator>>(F body) const {
            (body.template operator()<vals>(), ...);
            }
        };
    
        template<auto... vals>
        replicator_type<vals...> replicator = {};
    }

    template<typename R>
    consteval auto expand(R range) {
        std::vector<std::meta::info> args;
        for (auto r : range) {
            args.push_back(reflect_value(r));
        }
        return substitute(^^__impl::replicator, args);
    }


    /* ------------------------------ Concept to determine if a type is a container ------------------------------ */

    template<typename T>
    concept is_container = requires(T t)
    {
        typename T::value_type;
        { t.begin() } -> std::same_as<typename T::iterator>;
        { t.end() } -> std::same_as<typename T::iterator>;
        { t.size() } -> std::same_as<typename T::size_type>;
        { t.empty() } -> std::same_as<bool>;
        // { t.begin() } -> std::same_as<typename T::const_iterator>;
        // { t.end() } -> std::same_as<typename T::const_iterator>;
        // { t.cbegin() } -> std::same_as<typename T::const_iterator>;
        // { t.cend() } -> std::same_as<typename T::const_iterator>;
    };


    /* ------------------------------ Reflection helper functions that loops through tree of a type ------------------------------ */

    // Travelling depthwise into tree of class
    // recursively going through bases and members of those bases
    template<typename T, typename Function>
    consteval void loop_bases(Function member_function)
    {
        // bases of T
        [: expand(bases_of(^^T)) :] >> [&]<auto base>
        {
            using Base = typename [:type_of(base):];
            // recursively call bases of base
            loop_bases<Base>(member_function);
        };
        // members of T
        loop_members<T>(member_function);
    }

    template<typename T, typename Function>
    consteval void loop_members(Function member_function)
    {
        size_t index = 0;
        constexpr auto size = nonstatic_data_members_of(^^T).size();
        std::array<bool, size> have_rec_members;
        have_rec_members.fill(false);
        // members of T
        [: expand(nonstatic_data_members_of(^^T)) :] >> [&]<auto member>
        {
            using Member = typename [:type_of(member):];

            if constexpr (is_container<Member>)
            {
                // do something here for containers?
            }
            else if constexpr (std::is_fundamental_v<Member>)
            {
                // do nothing: fundamentals cannot be recursively operated on.
            }
            else if constexpr (std::is_pointer_v<Member>)
            {
                // do nothing: 
            }
            else if constexpr (std::is_trivially_copyable_v<Member>)
            {

            }
            else
            {
                have_rec_members[index] = true;
                // recursively call members of member
                loop_bases<Member>(member_function);
            }
            index++;
        };
        // members of T
        loop_branch_members<T>(member_function, have_rec_members);
    }

    template<typename T, typename Function, typename Arr>
    consteval void loop_branch_members(Function member_function, Arr have_rec_members)
    {
        size_t index = 0;
        [: expand(nonstatic_data_members_of(^^T)) :] >> [&]<auto member>
        {
            if (!have_rec_members[index++])
                member_function(member);
        };
    }

    /* ------------------------------ Reflection helper functions that operate on an entire type ------------------------------ */

    template<typename T>
    consteval auto get_total_nb_members()
    {    
        size_t count = 0;

        auto member_function = [&count](auto){ count++; };

        loop_bases<T>(member_function);
        return count;
    }

    template<size_t Index>
    consteval auto expand_indices()
    {
        return StringLiteral("__");
    }

    template<StringLiteral Identifier>
    consteval auto is_dud()
    {
        return Identifier == StringLiteral("__");
    }

    template<StringLiteral... Identifiers>
    consteval auto are_valid_identifiers()
    {
        if constexpr (sizeof...(Identifiers) > 0)
        {
            return !(is_dud<Identifiers>() && ...);
        }

        return false;
    }

    template<typename OriginalType, size_t Index, typename Layout>
    consteval void get_member_types_impl(Layout& types)
    {    
        size_t index = 0;

        auto member_function = [&index, &types](auto member)
        {
            if (index++ == Index)
                types[Index] = type_of(member);
        };

        loop_bases<OriginalType>(member_function);
    }

    template<typename OriginalType, size_t Index, StringLiteral Identifier, typename Layout>
    consteval void get_identifier_types_impl(Layout& types)
    {
        auto member_function = [&types](auto member)
        {
            if (Identifier == identifier_of(member))
                types[Index] = type_of(member);
        };

        loop_bases<OriginalType>(member_function);
    }

    template <typename OriginalType, typename Layout, StringLiteral... Identifiers, size_t... Indices>
    consteval auto get_types_wrapper(Layout& types, std::index_sequence<Indices...>) 
    {
        if constexpr (are_valid_identifiers<Identifiers...>())
            (get_identifier_types_impl<OriginalType, Indices, Identifiers>(types), ...);
        else
            (get_member_types_impl<OriginalType, Indices>(types), ...);
    }

    template <typename OriginalType, StringLiteral... Identifiers>
    consteval auto get_types()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();

        constexpr auto indices = std::make_index_sequence<N>{};
        std::array<std::meta::info, N> types;

        get_types_wrapper<OriginalType, decltype(types), Identifiers...>(types, indices);

        return types;
    }

    template <typename OriginalType, StringLiteral... Identifiers>
    consteval auto get_sizes()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();
        
        constexpr auto types = get_types<OriginalType, Identifiers...>();
        std::array<size_t, N> sizes;

        for (size_t i = 0; i < N; i++)
            sizes[i] = size_of(types[i]);

        return sizes;
    }

    template <typename OriginalType, StringLiteral... Identifiers>
    consteval auto get_alignments()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();

        constexpr auto types = get_types<OriginalType, Identifiers...>();
        std::array<size_t, N> alignments;

        for (size_t i = 0; i < N; i++)
            alignments[i] = alignment_of(types[i]);

        return alignments;
    }


    template<typename OriginalType, size_t Index, typename Layout>
    consteval void get_member_offsets_impl(Layout& offsets_type)
    {
        size_t index = 0;

        auto member_function = [&index, &offsets_type](auto member)
        {
            if (index++ == Index)
                offsets_type[Index] = offset_of(member).bytes;
        };

        loop_bases<OriginalType>(member_function);
    }

    template<typename OriginalType, size_t Index, StringLiteral Identifier, typename Layout>
    consteval void get_identifier_offsets_impl(Layout& offsets_type)
    {
        auto member_function = [&offsets_type](auto member)
        {
            if (Identifier == identifier_of(member))
                offsets_type[Index] = offset_of(member).bytes;
        };

        loop_bases<OriginalType>(member_function);
    }

    template <typename OriginalType, typename Layout, StringLiteral... Identifiers, size_t... Indices>
    consteval auto get_offsets_wrapper(Layout& offsets_type, std::index_sequence<Indices...>) 
    {    if constexpr (are_valid_identifiers<Identifiers...>())
            (get_identifier_offsets_impl<OriginalType, Indices, Identifiers>(offsets_type), ...);
        else
            (get_member_offsets_impl<OriginalType, Indices>(offsets_type), ...);
    }

    template <typename OriginalType, StringLiteral... Identifiers>
    consteval auto get_offsets() 
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();

        constexpr auto indices = std::make_index_sequence<N>{};
        std::array<size_t, N> offsets_type;

        get_offsets_wrapper<OriginalType, decltype(offsets_type), Identifiers...>(offsets_type, indices);

        return offsets_type;
    }


    template<typename OriginalType, StringLiteral... Identifiers>
    consteval auto calc_packed_size()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();

        auto sizes = get_sizes<OriginalType, Identifiers...>();
        auto alignments = get_alignments<OriginalType, Identifiers...>();

        size_t offset = 0;

        for (size_t i = 0; i < N; i++)
        {
            auto mod = offset % alignments[i];
            auto adjust_for_alignment = mod != 0 ? alignments[i] - mod :  0ul;

            offset += sizes[i] + adjust_for_alignment;
        }

        // we now adjust alignment of pattern by checking with larges alignment in the struct
        auto max_align = std::max_element(alignments.begin(), alignments.end());
        auto mod = offset % *max_align;
        auto adjust_for_alignment = mod != 0 ? *max_align - mod :  0ul;

        return offset + adjust_for_alignment;
    }


    // this function compares, if two identifiers are different
    template<size_t Index0, size_t Index1, StringLiteral Identifier0, StringLiteral Identifier1>
    consteval auto are_different_identifiers()
    {
        if (Index0 == Index1)
            return true;
        else
            return Identifier0 != Identifier1;
    }

    // this function calls the above function for one identifier and all others
    template<size_t Index, StringLiteral Identifier, StringLiteral... Identifiers, size_t... Indices>
    consteval auto are_no_duplicate_impl(std::index_sequence<Indices...>)
    {
        return (are_different_identifiers<Index, Indices, Identifier, Identifiers>() && ...);
    }

    // this function calls all identifiers with all other identifiers
    template<StringLiteral... Identifiers, size_t... Indices>
    consteval auto are_no_duplicates(std::index_sequence<Indices...> indices)
    {
        return (are_no_duplicate_impl<Indices, Identifiers, Identifiers...>(indices) && ...);
    }

    // functions checks if a pattern is trivial, i.e. does contain all data members of a type anyway
    template<typename OriginalType, StringLiteral... Identifiers>
    consteval auto is_trivial_pattern()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();
        constexpr auto indices = std::make_index_sequence<N>{};

        // check if an identifier occurs twice
        constexpr bool no_duplicates = (are_no_duplicates<Identifiers...>(indices));
        // check if there are as many identifiers, as data members (if there are wrong identifiers, compilation will fail earlier)
        constexpr bool same_size = N == get_total_nb_members<OriginalType>();

        // if both are true a pattern of a type contains all data members of that type
        return no_duplicates && same_size;
    }


    template <typename OriginalType, StringLiteral... Identifiers>
    consteval auto calc_offsets_in_buffer()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();

        std::array<size_t, N> offsets_buffer;

        auto sizes = get_sizes<OriginalType, Identifiers...>();
        auto alignments = get_alignments<OriginalType, Identifiers...>();

        for (size_t i = 0; i < N; i++)
        {
            if (i == 0)
            {
                offsets_buffer[0] = 0;
            }
            else
            {
                auto prev_type_size = sizes[i-1];
                auto mod = (offsets_buffer[i-1] + prev_type_size) % alignments[i];
                auto adjust_for_alignment = mod != 0 ? alignments[i] - mod : 0ul;
        
                offsets_buffer[i] = offsets_buffer[i-1] + prev_type_size + adjust_for_alignment;
            }
        }

        return offsets_buffer;
    }

    /* ------------------------------ Reflection helper functions that operate on given identifiers ------------------------------ */


    template <typename OriginalType, StringLiteral Identifier>
    consteval auto get_identifier_type()
    {
        std::meta::info type = ^^int;

        auto member_function = [&type](auto member)
        {
            if (Identifier == identifier_of(member))
                type = type_of(member);
        };

        loop_bases<OriginalType>(member_function);

        return type;
    }

    template <typename OriginalType, size_t Index>
    consteval auto get_index_type()
    {
        std::meta::info type = ^^int;
        size_t index = 0;

        auto member_function = [&type, &index](auto member)
        {
            if (Index == index++)
                type = type_of(member);
        };

        loop_bases<OriginalType>(member_function);

        return type;
    }


    template <typename OriginalType, StringLiteral Identifier>
    consteval auto get_identifier_index()
    {
        size_t running_index = 0;
        size_t index;

        auto member_function = [&index, &running_index](auto member)
        {
            if (Identifier == identifier_of(member))
                index = running_index;
            
            running_index++;
        };

        loop_bases<OriginalType>(member_function);

        return index;
    }


    /* ------------------------------ Concepts for reflection ------------------------------ */

    // checks if underlying value type is trivially copyable. used for container checking
    template<typename T>
    concept is_underlying_trivially_copyable = requires {
        typename T::value_type;
        requires std::is_trivially_copyable_v<typename T::value_type>;
    };


    template<typename OriginalType, size_t Index>
    consteval auto is_without_pointer_using_indices()
    {
        constexpr auto type = get_index_type<OriginalType, Index>();

        using T = typename [:type:];

        return !std::is_pointer_v<T>;
    }

    template<typename OriginalType, StringLiteral Identifier>
    consteval auto is_without_pointer_using_identifiers()
    {
        constexpr auto type = get_identifier_type<OriginalType, Identifier>();

        using T = typename [:type:];

        return !std::is_pointer_v<T>;
    }

    template <typename OriginalType, StringLiteral... Identifiers, size_t... Indices>
    consteval auto is_without_pointer_wrapper(std::index_sequence<Indices...>) 
    {
        if constexpr (are_valid_identifiers<Identifiers...>())
            return (is_without_pointer_using_identifiers<OriginalType, Identifiers>() && ...);
        else
            return (is_without_pointer_using_indices<OriginalType, Indices>() && ...);
    }

    template <typename OriginalType, StringLiteral... Identifiers>
    consteval auto is_without_pointer_func()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();

        constexpr auto indices = std::make_index_sequence<N>{};

        return is_without_pointer_wrapper<OriginalType, Identifiers...>(indices);
    }

    template<typename T, StringLiteral... Identifiers>
    concept is_without_pointers = is_without_pointer_func<T, Identifiers...>() && !std::is_pointer_v<T>;

    // https://www.typeerror.org/docs/cpp/named_req/copyinsertable
    template<typename X, typename T, typename A>
    consteval auto is_copy_insertable_impl()
    {
        // using T = typename X::value_type;
        // using A = typename std::allocator<T>;
        A m;
        T t;
        T* p = &t;
        T v{};
        T old_v = v;
        std::allocator_traits<A>::construct(m, p, v);

        if (std::is_same_v<typename X::allocator_type, typename std::allocator_traits<A>::template rebind_alloc<T>>)
            if (v == *p)
                if (v == old_v)
                    return true;

        return false;
    };

    template<typename X>
    concept is_copy_insertable = requires {
        /*using T = */typename X::value_type;
        /*using A = */typename std::allocator<typename X::value_type>;
        typename X::allocator_type;
        } && is_copy_insertable_impl<X, typename X::value_type, typename std::allocator<typename X::value_type>>();


    // !We currently only support containers with an underlying type that is trivially copyable, hence the last concept!
    template<typename T>
    concept is_allocator_aware_container = std::is_default_constructible_v<T> && is_copy_insertable<T> && std::is_copy_assignable_v<T>;



    template<typename OriginalType, size_t Index>
    consteval auto has_only_trivially_copyable_types_using_indices()
    {
        constexpr auto type = get_index_type<OriginalType, Index>();

        using T = typename [:type:];

        return std::is_trivially_copyable_v<T>;
    }

    template<typename OriginalType, StringLiteral Identifier>
    consteval auto has_only_trivially_copyable_types_using_identifiers()
    {
        constexpr auto type = get_identifier_type<OriginalType, Identifier>();

        using T = typename [:type:];

        return std::is_trivially_copyable_v<T>;
    }

    template <typename OriginalType, StringLiteral... Identifiers, size_t... Indices>
    consteval auto has_only_trivially_copyable_types_wrapper(std::index_sequence<Indices...>) 
    {
        if constexpr (are_valid_identifiers<Identifiers...>())
            return (has_only_trivially_copyable_types_using_identifiers<OriginalType, Identifiers>() && ...);
        else
            return (has_only_trivially_copyable_types_using_indices<OriginalType, Indices>() && ...);
    }

    template <typename OriginalType, StringLiteral... Identifiers>
    consteval auto has_only_trivially_copyable_types_func()
    {
        constexpr auto N = are_valid_identifiers<Identifiers...>() ? sizeof...(Identifiers) : get_total_nb_members<OriginalType>();

        constexpr auto indices = std::make_index_sequence<N>{};

        return has_only_trivially_copyable_types_wrapper<OriginalType, Identifiers...>(indices);
    }

    template<typename T, StringLiteral... Identifiers>
    concept has_only_trivially_copyable_types = has_only_trivially_copyable_types_func<T, Identifiers...>() /*&& std::is_trivially_copyable_v<T>*/;


    /* ------------------------------ Reflection helper functions that operate in runtime ------------------------------ */

    template<typename OriginalType, size_t Index>
    auto calc_type (std::byte* range_start, size_t range_size, size_t range_offset, size_t& offset)
    {
        constexpr auto offset_type = get_offsets<OriginalType>();
        constexpr auto sizes = get_sizes<OriginalType>();
        constexpr auto alignments = get_alignments<OriginalType>();
        constexpr auto types = get_types<OriginalType>();

        using T = typename [:types[Index]:];

        if constexpr (is_allocator_aware_container<T>)
        {
            using Value_Type = typename T::value_type;

            auto mod = offset % alignof(Value_Type);
            auto adjust_for_alignment = mod != 0 ? alignof(Value_Type) - mod :  0ul;

            auto container_ptr = reinterpret_cast<T*>(range_start + range_offset + offset_type[Index]);
            auto size = container_ptr->size();

            offset += size * sizeof(Value_Type) + adjust_for_alignment;
        }
        else
        {
            auto mod = offset % alignments[Index];
            auto adjust_for_alignment = mod != 0 ? alignments[Index] - mod :  0ul;

            offset += sizes[Index] + adjust_for_alignment;
        }
    };


    template <typename OriginalType, size_t... Indices>
    auto calc_types (std::byte* range_start, size_t range_size, size_t range_offset, size_t& offset, std::index_sequence<Indices...>)
    {
        (calc_type<OriginalType, Indices>(range_start, range_size, range_offset, offset), ...);
    };

    template<typename OriginalType, typename... Rs>
    auto calc_packed_size_with_container(Rs&... ranges)
    {
        constexpr auto N = get_total_nb_members<OriginalType>();
        constexpr auto indices = std::make_index_sequence<N>{};

        size_t offset = 0;
        
        auto calc_range = [&](auto& range)
        {
            auto* range_start = reinterpret_cast<std::byte*>(range.data());
            auto range_size = range.size();

            for (size_t i = 0; i < range_size; i++)
            {
                auto range_offset = i * sizeof(OriginalType);

                calc_types<OriginalType>(range_start, range_size, range_offset, offset, indices);
            }
        };

        (calc_range(ranges), ...);

        return offset;
    }


    template<typename OriginalType, StringLiteral Identifier>
    auto calc_type_identifier (std::byte* range_start, size_t range_size, size_t range_offset, size_t& offset)
    {
        constexpr auto Index = get_identifier_index<OriginalType, Identifier>();

        constexpr auto offset_type = get_offsets<OriginalType>();
        constexpr auto sizes = get_sizes<OriginalType>();
        constexpr auto alignments = get_alignments<OriginalType>();
        constexpr auto types = get_types<OriginalType>();

        using T = typename [:types[Index]:];

        if constexpr (is_allocator_aware_container<T>)
        {
            using Value_Type = typename T::value_type;

            auto mod = offset % alignof(Value_Type);
            auto adjust_for_alignment = mod != 0 ? alignof(Value_Type) - mod :  0ul;

            auto container_ptr = reinterpret_cast<T*>(range_start + range_offset + offset_type[Index]);
            auto size = container_ptr->size();

            offset += size * sizeof(Value_Type) + adjust_for_alignment;
        }
        else
        {
            auto mod = offset % alignments[Index];
            auto adjust_for_alignment = mod != 0 ? alignments[Index] - mod :  0ul;

            offset += sizes[Index] + adjust_for_alignment;
        }
    };

    template <typename OriginalType, StringLiteral... Identifiers>
    auto calc_types_identifier (std::byte* range_start, size_t range_size, size_t range_offset, size_t& offset)
    {
        (calc_type_identifier<OriginalType, Identifiers>(range_start, range_size, range_offset, offset), ...);
    };

    template<typename OriginalType, StringLiteral... Identifiers, typename... Rs>
    auto calc_packed_size_with_container_identifier(Rs&... ranges)
    {
        constexpr auto nb_ranges = sizeof...(Rs);
        constexpr auto N = sizeof...(Identifiers);

        size_t offset = 0;
        
        auto calc_range = [&](auto& range)
        {
            auto* range_start = reinterpret_cast<std::byte*>(range.data());
            auto range_size = range.size();

            for (size_t i = 0; i < range_size; i++)
            {
                auto range_offset = i * sizeof(OriginalType);

                calc_types_identifier<OriginalType, Identifiers...>(range_start, range_size, range_offset, offset);
            }
        };

        (calc_range(ranges), ...);

        return offset;
    }


    template<typename OriginalType, StringLiteral... Identifiers, typename... Rs>
    auto calc_packed_size_allocator_aware_container(Rs&... ranges)
    {
        if constexpr (are_valid_identifiers<Identifiers...>())
            return calc_packed_size_with_container_identifier<OriginalType, Identifiers...>(ranges...);
        else
            return calc_packed_size_with_container<OriginalType>(ranges...);
    }
};

#endif