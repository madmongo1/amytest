//
// Created by Richard Hodges on 21/04/2017.
//

#include "table_lookup.hpp"
#include "sql_escaper.hpp"
#include "hasher.hpp"
#include "google/protobuf/util/json_util.h"

namespace {

    auto default_hash_algoritm() -> proto::storage::HashAlgorithm const & {
        static proto::storage::HashAlgorithm store;
        static proto::storage::HashAlgorithm const &ref = [&](auto &store) -> decltype(auto) {
            store.mutable_cryptogenerichash()->set_hashlength(22);
            return store;
        }(store);
        return ref;
    }

    std::string to_json(proto::storage::HashAlgorithm const &algo) {
        std::string result;
        ::google::protobuf::util::MessageToJsonString(algo, &result);
        return result;
    }

    const

    std::string hex_encode(std::uint8_t const *first, std::uint8_t const *const last) {
        static const char myDigits[] = "0123456789ABCDEF";
        std::string myResult;
        auto dist = std::distance(first, last);
        myResult.reserve(dist * 2);
        while (first != last) {
            auto byte = *first++;
            myResult.push_back(myDigits[byte >> 4]);
            myResult.push_back(myDigits[byte & 0xf]);
        }
        return myResult;
    }

    template<class Iter>
    std::string hex_encode(Iter first, Iter last) {
        return hex_encode(reinterpret_cast<std::uint8_t const *>(std::addressof(*first)),
                          reinterpret_cast<std::uint8_t const *>(std::addressof(*last)));
    }
}

void table_lookup::init() {
    static const char query[] = ""
            "CREATE TABLE IF NOT EXISTS tbl_table_name"
            "("
            "   real_name VARCHAR(1024) NOT NULL PRIMARY KEY,"
            "   hash_name VARCHAR(64) NOT NULL,"
            "   hash_algorithm VARCHAR(512) NOT NULL,"
            "   UNIQUE INDEX (hash_name, hash_algorithm)"
            ")";
    execute(connection_, query);
}

std::string table_lookup::lookup(std::string const &real_name) {
    auto ifind = my_real_to_hash_.find(real_name);
    if (ifind != my_real_to_hash_.end())
        return ifind->second;

    auto &our_cache = get_static_cache();
    auto hash_name = our_cache.lookup(connection_, real_name);
    my_real_to_hash_[real_name] = hash_name;
    my_hash_to_real_[hash_name] = real_name;
    return hash_name;
}

auto table_lookup::cache::lookup(amy::connector &conn,
                                 std::string const &real_name) -> std::string {
    auto lock = std::unique_lock<std::mutex>(mutex_);
    auto ifind = real_to_hash_.find(real_name);
    if (ifind != real_to_hash_.end())
        return ifind->second;
    conn.query(build_query(conn, "select hash_name from tbl_table_name where real_name=%1%;", real_name));
    auto rs = conn.store_result();
    if (rs.size() == 0) {
        std::vector<std::uint8_t> hash_bytes;
        static auto &&algorithm = default_hash_algoritm();
        static const auto json = to_json(algorithm);
        hash(hash_bytes, std::begin(real_name), std::end(real_name), default_hash_algoritm());
        auto hash_name = hex_encode(std::begin(hash_bytes), std::end(hash_bytes));
        update(real_name, hash_name);
        execute(conn, build_query(conn,
                                  "insert into tbl_table_name (real_name, hash_name, hash_algorithm)"
                                          " values (%1%, %2%, %3%)",
                                  real_name, hash_name, json));
        return hash_name;

    } else {
        auto hash_name = rs.at(0).at(0).as<std::string>();
        update(real_name, hash_name);
        return hash_name;
    }
}

void table_lookup::cache::update(std::string const &real_name, std::string const &hashed_name) {
    real_to_hash_[real_name] = db_name(hashed_name);
    hash_to_real_[hashed_name] = real_name;
}

