//
// Created by Richard Hodges on 21/04/2017.
//

#pragma once

#include "proto/proto_storage.pb.h"

void hash(std::vector<std::uint8_t>& target,
          const std::uint8_t* first,
          const std::uint8_t* last,
          proto::storage::HashAlgorithm const& algorithm);

template<class Iter>
void hash(std::vector<std::uint8_t>& target, Iter first, Iter last, proto::storage::HashAlgorithm const& algorithm)
{
    hash(target,
         reinterpret_cast<const std::uint8_t*>(std::addressof(*first)),
         reinterpret_cast<const std::uint8_t*>(std::addressof(*last)),
    algorithm);
}