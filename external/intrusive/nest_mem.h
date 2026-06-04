
#pragma once

#include <concepts>

namespace intrusive
{

    template <typename Outer, typename T, T Outer::*member>
    struct member_pointer
    {
        using memb_type = T;
        using outer = Outer;

        static T* resolve(Outer* obj)
        {
            return &(obj->*member);
        }
    };

    namespace memb_chain_impl
    {

        template <typename Last>
        auto* resolve_memb_chain(typename Last::outer* obj)
        {
            return Last::resolve(obj);
        }

        template <typename First, typename Next, typename... Args>
        auto* resolve_memb_chain(typename First::outer* obj)
        {
            auto* v = First::resolve(obj);
            return resolve_memb_chain<Next, Args...>(v);
        }

    }

    template <typename First, typename... Args>
    struct member_chain
    {
        static auto resolve(typename First::outer* obj)
        {
            return memb_chain_impl::resolve_memb_chain<First, Args...>(obj);
        }
    };

    template <typename Res, typename T, typename membchain>
    concept member_chain_resolves = requires(T* outer)
    {
        { membchain::resolve(outer) } -> std::same_as<Res>;
    };

}
