#pragma once

namespace notstd {

    namespace detail {
        template<class Tuple, class F, std::size_t...Is>
        void for_each(Tuple&& tuple, F&& f, std::index_sequence<Is...>)
        {
            using expand = int[];
            void(expand{
                0,
                (f(std::get<Is>(std::forward<Tuple>(tuple))), 0)...
            });
        };

    }

    template<class Tuple, class F>
    void for_each(Tuple&& tuple, F&& f)
    {
        using base_tuple = std::decay_t<Tuple>;
        constexpr auto tuple_size = std::tuple_size<base_tuple>::value;
        detail::for_each(std::forward<Tuple>(tuple),
                         std::forward<F>(f),
                         std::make_index_sequence<tuple_size>());
    };
}
