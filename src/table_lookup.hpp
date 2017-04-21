//
// Created by Richard Hodges on 21/04/2017.
//

#pragma once

#include "config.hpp"
#include <amy.hpp>
#include <unordered_map>
#include <string>

struct table_lookup
{
    table_lookup(amy::connector& conn) : connection_(conn) {}

    void init();

    std::string lookup(std::string const& real_name);

    struct cache
    {
        auto lookup(amy::connector& conn, std::string const& real_name) -> std::string;

        void update(std::string const& real_name, std::string const& hashed_name);

        std::unordered_map<std::string, std::string> real_to_hash_;
        std::unordered_map<std::string, std::string> hash_to_real_;
        std::mutex mutex_;
    };

    static cache& get_static_cache() {
        static cache cache_ {};
        return cache_;
    }

    amy::connector& connection_;
    std::unordered_map<std::string, std::string> my_real_to_hash_;
    std::unordered_map<std::string, std::string> my_hash_to_real_;
};



