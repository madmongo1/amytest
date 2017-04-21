//
// Created by Richard Hodges on 21/04/2017.
//

#include "sql_escaper.hpp"


std::string const& sql_escaper::operator ()(db_name const& arg)
{
    auto slen = arg.length();
    output_.resize(slen * 2 + 1 + 2);
    output_[0] = '`';

    auto length = mysql_real_escape_string_quote(connector.native(),
                                                 &output_[1],
                                                 arg.data(), slen,
                                                 '`');
    output_[1 + length] = '`';
    output_.erase(length + 2);

    return output_;
}
