//
// Created by Richard Hodges on 18/04/2017.
//

#include "config.hpp"

#include <amy.hpp>
#include <iostream>
#include <tuple>
#include <utility>
#include <boost/format.hpp>

using namespace amytest;

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
        detail::for_each(std::forward<Tuple>(tuple), std::forward<F>(f), std::make_index_sequence<tuple_size>());
    };
}

struct sql_escaper
{
    sql_escaper(amy::connector& connector)
        : connector(connector) {}

    std::string const& operator ()(std::string arg)
    {
        auto slen = arg.length();
        output_.resize(slen * 2 + 1 + 2);
        output_[0] = '\'';

        auto length = mysql_real_escape_string_quote(connector.native(), &output_[1], arg.data(), slen, '\'');
        output_[1 + length] = '\'';
        output_.erase(length + 2);

        return output_;
    }

    amy::connector& connector;
    std::vector<char> buffer_;
    std::string       output_;
};


template<class...Ts>
auto build_query(amy::connector& connector, std::string const& format, Ts&& ...parts)
{
    auto escaper = sql_escaper(connector);
    auto fmt     = boost::format(format);

    std::string result;
    notstd::for_each(std::forward_as_tuple(std::forward<Ts>(parts)...), [&escaper, &fmt](auto&& part)
    {
        fmt % escaper(std::forward<decltype(part)>(part));
    });
    return fmt.str();
}

struct perform_test
{
    using address = asio::ip::address_v4;
    using endpoint_type = asio::ip::tcp::endpoint;

    perform_test(asio::io_service& owner)
        : connector_(owner)
    {
    }

    void start()
    {
        connector_.async_connect(endpoint,
                                 auth_info,
                                 "test",
                                 amy::client_multi_statements | amy::client_multi_results,
                                 [this](auto&& ...args)
                                 {
                                     this->handle_connect(std::forward<decltype(args)>(args)...);
                                 });
    }

private:
    void handle_connect(boost::system::error_code const& error)
    {
        if (error) {
            std::cout << __func__ << " : " << connector_.error_message(error) << std::endl;
        }
        else {
            auto query = build_query(connector_,
                                     "insert into people (`name`) values (%1%)"
                                         " ; "
                                         "select count(*) from people where `name` = %1%"
                                         " ; "
                                         "select * from people",
                                     "richard");
            std::cout << "query: " << query << std::endl;
            connector_.async_query(query,
                                   [this](auto&& ...args)
                                   {
                                       this->handle_insert(std::forward<decltype(args)>(args)...);
                                   });
        }
    }

    void handle_insert(boost::system::error_code const& ec)
    {
        if (ec) {
            std::cout << __func__ << " : " << ec.message() << std::endl;
        }
        else {
            std::cout << "affected rows: " << connector_.affected_rows() << std::endl;
            connector_.async_store_result([this](auto&& ...args)
                                          {
                                              this->handle_store_result(std::forward<decltype(args)>(args)...);
                                          });
        }

    }

    void handle_store_result(boost::system::error_code const& ec,
                             amy::result_set rs)
    {
        if (ec) {
            std::cout << __func__ << " : " << ec.message() << std::endl;
        }
        else {
            std::cout << "result set: " << rs.affected_rows() << " affected rows:\n";
            for (auto&& r : rs) {
                const char *sep = "";
                for (auto  && f : r) {
                    std::cout << sep << f.as<std::string>();
                    sep = ", ";
                }
                std::cout << std::endl;
            }
            if (connector_.has_more_results()) {
                handle_insert(boost::system::error_code());
            }
        }

    }

    endpoint_type  endpoint{address::from_string("127.0.0.1"), 3306};
    amy::auth_info auth_info{"test-user", "test-password"};
    amy::connector connector_;
};

int main()
{
    asio::io_service ios;

    perform_test tester{ios};
    tester.start();
    ios.run();


}