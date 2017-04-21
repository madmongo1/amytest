//
// Created by Richard Hodges on 21/04/2017.
//

#pragma once

#include "config.hpp"
#include <amy.hpp>
#include <boost/format.hpp>
#include "notstd.hpp"
#include "sql_escaper.hpp"

struct query_builder {

    query_builder(sql_escaper& escaper)
            : escaper(escaper)
    {}

    template<class...Args>
    void add_component(std::string const& fmt, Args&&...args)
    {
        format_str += fmt;
        notstd::for_each(std::forward_as_tuple(std::forward<Args>(args)...),
        [&](auto&& arg) {
            using arg_type = decltype(arg);
            arg_functions.emplace_back([this, arg = std::forward<arg_type>(arg)](boost::format& formatter) {
                formatter % this->escaper(arg);
            });
        });
    }

    boost::format operator()() const {
        boost::format formatter(format_str);
        for (auto&& func : arg_functions) {
            func(formatter);
        }
        return formatter;
    }

    std::string format_str;
    std::vector<std::function<void(boost::format&)>> arg_functions;
    sql_escaper& escaper;
};



