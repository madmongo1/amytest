//
// Created by Richard Hodges on 21/04/2017.
//

#pragma once

#include "config.hpp"
#include <amy.hpp>
#include "notstd.hpp"
#include <boost/format.hpp>

struct db_name
    : std::string
{
    using std::string::string;
};

struct verbatim
    : std::string
{
    using std::string::string;
};

struct sql_escaper
{
    sql_escaper(amy::connector& connector)
        : connector(connector) {}


    template<std::size_t N>
    std::string const& operator ()(const char (& arg)[N])
    {
        auto slen = N - 1;
        output_.resize(slen * 2 + 1 + 2);
        output_[0] = '\'';

        auto length = mysql_real_escape_string_quote(connector.native(),
                                                     &output_[1],
                                                     arg, slen,
                                                     '\'');
        output_[1 + length] = '\'';
        output_.erase(length + 2);

        return output_;
    }

    std::string const& operator ()(std::string const& arg)
    {
        auto slen = arg.length();
        output_.resize(slen * 2 + 1 + 2);
        output_[0] = '\'';

        auto length = mysql_real_escape_string_quote(connector.native(),
                                                     &output_[1],
                                                     arg.data(), slen,
                                                     '\'');
        output_[1 + length] = '\'';
        output_.erase(length + 2);

        return output_;
    }

    std::string const& operator ()(db_name const& arg);

    std::string const& operator ()(verbatim const& arg)
    {
        return arg;
    }

    std::string const& operator ()(int x)
    {
        output_ = std::to_string(x);
        return output_;
    }

    amy::connector& connector;
    std::vector<char> buffer_;
    std::string       output_;
};



template<class...Ts>
auto format_query(amy::connector& connector, std::string const& format, Ts&& ...parts)
{
    auto escaper = sql_escaper(connector);
    auto fmt     = boost::format(format);

    std::string result;
    notstd::for_each(std::forward_as_tuple(std::forward<Ts>(parts)...), [&escaper, &fmt](auto&& part)
    {
        fmt % escaper(std::forward<decltype(part)>(part));
    });
    return fmt;
}

template<class...Ts>
auto build_query(amy::connector& connector, std::string const& format, Ts&& ...parts)
{
    return format_query(connector, format, std::forward<Ts>(parts)...).str();
}
