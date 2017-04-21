//
// Created by Richard Hodges on 18/04/2017.
//

#include "config.hpp"

#include <amy.hpp>
#include <mysql/mysql.h>

#include <iostream>
#include <iomanip>
#include <tuple>
#include <utility>
#include <boost/format.hpp>
#include <random>
#include <google/protobuf/message.h>
#include <google/protobuf/util/type_resolver_util.h>
#include <google/protobuf/util/json_util.h>
#include <sodium/crypto_generichash.h>

#include "proto/test.pb.h"
#include "proto/proto_storage.pb.h"

#include "base64.hpp"

#include "sql_escaper.hpp"
#include "table_lookup.hpp"
#include "query_builder.hpp"

using namespace amytest;

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
                                 amy::client_multi_statements | amy::client_multi_results | amy::client_ssl,
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
            connector_.autocommit(false);
            std::random_device              rnd;
            std::default_random_engine      eng(rnd());
            std::uniform_int_distribution<> dist(1, 99);
            int                             age = dist(eng);

            auto query = build_query(connector_,
                                     R"__(
START TRANSACTION ;
insert into people (`name`, `age`) values (%1%, %2%);
select count(*) from people where `name` = %1%;
select * from people where age > %3%;
COMMIT;
select * from people)__",
                                     "richard",
                                     age, age / 2);
            std::cout << "query: " << query << std::endl;
            connector_.async_query(query,
                                   [this](auto&& ...args)
                                   {
                                       this->handle_query(std::forward<decltype(args)>(args)...);
                                   });
        }
    }

    void handle_query(boost::system::error_code const& ec)
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
            std::cout << __func__ << " : " << connector_.error_message(ec) << std::endl;
        }
        else {
            std::cout << "result set: " << rs.affected_rows() << " affected rows:\n";
            for (auto&& r : rs) {
                const char *sep = "";
                for (auto&& f : r) {
                    std::cout << sep << f.as<std::string>();
                    sep = ", ";
                }
                std::cout << std::endl;
            }
        }
        if (connector_.has_more_results()) {
            handle_query(boost::system::error_code());
        }

    }

    endpoint_type  endpoint{address::from_string("127.0.0.1"), 3306};
    amy::auth_info auth_info{"test-user", "test-password"};
    amy::connector connector_;
};

struct query_doer
{
    query_doer(amy::connector& con)
        : con(con)
    {
        con.query("SELECT DATABASE()");
        auto rs = con.store_result();
        if (rs.affected_rows() != 1) { throw std::runtime_error("not 1 row for select database()"); }
        schema = rs[0][0].as<std::string>();
    }

    template<class...Ts>
    auto operator ()(const std::string& sql, Ts&& ...ts) const
    {
        auto query = build_query(con, sql, std::forward<Ts>(ts)...);
        std::cout << "executing:\n" << query << std::endl;
        con.query(query);
        return con.store_result();
    }

    bool add_missing_column(std::string const& table_name, std::string const& column_name,
                            std::string const& column_def) const
    {
        auto rs = self()(R"__(SELECT COUNT(*)
FROM `information_schema`.`COLUMNS`
WHERE
    `TABLE_SCHEMA` = %1%
AND `TABLE_NAME` = %2%
AND `COLUMN_NAME` = %3%)__", schema, table_name, column_name);
        if (rs.at(0).at(0).as<int>() == 0) {
            self()(R"__(ALTER TABLE %1% ADD %2% %3%)__",
                   db_name(table_name), db_name(column_name), verbatim(column_def));
            return true;
        }
        else {
            return false;
        }
    }

    std::string enquote(const std::string& str)
    {
        return escaper(str);
    }

    const query_doer& self() const { return *this; }

    query_doer& self() { return *this; }

    amy::connector& con;
    sql_escaper escaper { con };
    std::string schema;
    table_lookup tbl_lookup { con };
};

std::string deduce_string_storage(std::int64_t max_length)
{
    if (max_length == 0)
    {
        return "LONGTEXT";
    }
    else if (max_length < 256)
    {
        return "VARCHAR(" + std::to_string(max_length) + ")";
    }
    else if (max_length < 65536) {
        return "TEXT";
    }
    else {
        return "LONGTEXT";
    }
}

struct member_history
{
    using Descriptor = google::protobuf::Descriptor;
    using FieldDescriptor = google::protobuf::FieldDescriptor;

    member_history(Descriptor const* descriptor)
            : base(descriptor)
    {}

    template<class Iter>
    member_history(Descriptor const* descriptor, Iter first, Iter last)
            : base(descriptor)
    , fields { first, last }
    {}

    member_history& operator+=(FieldDescriptor const* field) {
        fields.push_back(field);
        return *this;
    }

    std::string name() const {
        std::string myResult = base->full_name();
        for (auto field : fields)
        {
            myResult += ":" + std::to_string(field->number());
        }
        return myResult;
    }

    bool has_parent() const {
        return not fields.empty();
    };

    member_history parent() const {
        auto first = fields.begin();
        auto last = fields.end();
        if (last != first) --last;
        return member_history(base, first, last);
    }

    google::protobuf::Descriptor const* base;
    std::vector<::google::protobuf::FieldDescriptor const*> fields;
};

member_history operator + (member_history l, member_history::FieldDescriptor const* r) {
    return l += r;
}

void build_repeated_scheme(query_doer& con,
                           member_history history)
{
    auto this_table_name = history.name();
}



void create_message_table(query_doer& con, member_history const& history)
{
    query_builder builder(con.escaper);
    builder.add_component("CREATE TABLE IF NOT EXISTS %s (\n", db_name(con.tbl_lookup.lookup(history.name())));
    builder.add_component(" __id__ INT NOT NULL AUTO_INCREMENT PRIMARY KEY\n");
    if (history.has_parent())
    {
        builder.add_component(",__parent__ INT NOT NULL\n");
        builder.add_component(", CONSTRAINT FOREIGN KEY (__parent__)"
        " REFERENCES %s (__id__)"
        " ON DELETE CASCADE"
        " ON UPDATE CASCADE", db_name(con.tbl_lookup.lookup(history.parent().name())));
    }
    builder.add_component(")");
    std::cout << "formatting:\n" << builder.format_str << std::endl;
    auto query = builder().str();
    std::cout << "executing:\n" << query << std::endl;
    execute(con.con, query);
}

void build_scheme(query_doer& con, member_history const& child)
{
    create_message_table(con, child);


}

void build_scheme(query_doer& con, google::protobuf::Descriptor const *descriptor)
{
    using namespace ::google::protobuf;

    auto table_hash_name = con.tbl_lookup.lookup(descriptor->full_name());

    auto history = member_history(descriptor);

    create_message_table(con, history);

    auto                   nfields = descriptor->field_count();

    for (decltype(nfields) ifield  = 0; ifield < nfields; ++ifield) {
        auto field = descriptor->field(ifield);
        std::cout << field->full_name();
        std::cout << " type: " << field->type() << " - " << field->type_name();
        std::cout << std::endl;
        if (field->is_repeated()) {
            build_repeated_scheme(con, history + field);
        }
        else {
            switch (field->type()) {
                case FieldDescriptor::TYPE_STRING: {
                    auto maxLength = field->options().GetExtension(limits::maxLength);
                    auto storage_def = deduce_string_storage(maxLength);
                    if (auto oneof = field->containing_oneof()) {
                        storage_def += " NULL";
                    }
                    else {
                        storage_def += " NOT NULL DEFAULT " + con.enquote(field->default_value_string());
                    }

                    con.add_missing_column(table_hash_name, std::to_string(field->number()), storage_def);
                }
                    break;

                case FieldDescriptor::TYPE_INT32: {
                    con.add_missing_column(table_hash_name, std::to_string(field->number()), "INT(9) NULL");
                }
                    break;

                case FieldDescriptor::TYPE_MESSAGE:
                    build_scheme(con, history + field);
                    break;
                default:
                    std::cout << "ignored\n";
            }
        }
    }

}

void build_scheme(amy::connector& con, const google::protobuf::Descriptor *descriptor)
{
    query_doer helper(con);
    build_scheme(helper, descriptor);
}

void make_blob_store(amy::connector& connection)
{
    query_doer helper(connection);
    execute(connection, R"__(
CREATE TABLE IF NOT EXISTS `tbl_message_store` (
  `unique_id` int(11) NOT NULL AUTO_INCREMENT,
  `message_type` varchar(255) NOT NULL,
  `binary_data` longblob,
  `json_data` longtext,
  PRIMARY KEY (`unique_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
)__");
}

std::string to_base64(std::string in)
{
    auto        b    = base64();
    int         len  = b.needed_encoded_length(in.size());
    std::string result(len, ' ');
    auto        len2 = b.encode(in.c_str(), in.size(), &result[0]);
    result.erase(std::strlen(result.c_str()));
    return result;
}

std::string to_json(google::protobuf::Message const& message)
{
    using namespace google::protobuf;

    auto result = std::string();
    auto status = util::MessageToJsonString(message, &result);
    if (not status.ok()) {
        throw std::runtime_error("failed to convert to json: " + status.ToString());
    }
    return result;

}

int write_message(amy::connector& conn, ::google::protobuf::Message const& message, bool as_json = false)
{
    auto query = std::string();
    if (as_json) {
        query = build_query(conn, "INSERT INTO tbl_message_store (message_type, json_data) VALUES(%1%, %2%)",
                            message.GetDescriptor()->full_name(),
                            to_json(message));

    }
    else {
        query = build_query(conn,
                            "INSERT INTO tbl_message_store (message_type, binary_data) VALUES(%1%, FROM_BASE64(%2%))",
                            message.GetDescriptor()->full_name(),
                            to_base64(message.SerializeAsString()));
    }
    std::cout << "executing: " << query << std::endl;
    auto affected = execute(conn, query);
    if (not(affected == 1)) {
        throw std::runtime_error("failed to insert");
    }

    execute(conn, "SELECT LAST_INSERT_ID()");
    auto id = conn.store_result()[0][0].as<int>();
    return id;
}

void read_message(amy::connector& conn, ::google::protobuf::Message& message, int id)
{
    auto query = build_query(conn,
                             "SELECT"
                                 " message_type, binary_data, json_data"
                                 " FROM tbl_message_store"
                                 " WHERE unique_id = %1%", id);
    std::cout << "executing: " << query << std::endl;
    execute(conn, query);
    auto rs = conn.store_result();
    auto&& row = rs.at(0);
    auto message_type = row.at(0).as<std::string>();
    if (message_type != message.GetDescriptor()->full_name())
        throw std::runtime_error("message type mismatch: " + message_type);
    if (not row.at(1).is_null()) {
        auto blobdata = row.at(1).as<std::string>();
        message.ParseFromString(blobdata);
    }
    else if (not row.at(2).is_null()) {
        auto json = row.at(2).as<std::string>();
        ::google::protobuf::util::JsonStringToMessage(json, &message);
    }
    else {
        throw std::runtime_error("invalid record");
    }
}

int main()
{
    auto addr      = tcp_endpoint(ip_address::from_string("127.0.0.1"), 3306);
    auto auth_info = amy::auth_info{"test-user", "test-password"};

    asio::io_service ios;
    amy::connector   connection(ios);
    connection.connect(addr, auth_info, "test", amy::client_multi_statements | amy::client_multi_results);

    try {
        auto lookup = table_lookup(connection);
        lookup.init();

        make_blob_store(connection);
        auto do_it = [&](auto use_json)
        {
            test::BigMessage source;
            source.mutable_y()->mutable_a()->assign("value for a");
            source.mutable_y()->add_c("foo");

            for (int i  = 0; i < 10; ++i)
            {
                source.mutable_y()->add_c("bar " + std::to_string(i));
            }
            auto             id = write_message(connection, source, use_json);
            test::BigMessage dest;
            read_message(connection, dest, id);
            std::cout << source.ShortDebugString() << std::endl;
            std::cout << dest.ShortDebugString() << std::endl;

            std::cout << std::boolalpha << "same? " << (source.ShortDebugString() == dest.ShortDebugString())
                      << std::endl;
        };
        do_it(true);
        do_it(false);

        build_scheme(connection, test::BigMessage::descriptor());
    }
    catch (AMY_SYSTEM_NS::system_error const& se) {
        auto&& category = se.code().category();
        if (category == amy::error::get_client_category()) {
            std::cerr << connection.error_message(se.code());
        }
        else {
            std::cerr << se.code().message() << std::endl;
        }
    }


    perform_test tester{ios};
    tester.start();
    ios.run();


}