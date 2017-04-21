//
// Created by Richard Hodges on 21/04/2017.
//


#include "hasher.hpp"
#include <sodium/crypto_generichash.h>
#include <stdexcept>

void hash(std::vector<std::uint8_t>& target,
          const std::uint8_t* first,
          const std::uint8_t* last,
          proto::storage::HashAlgorithm::CryptoGenericHash const& algorithm)
{
    auto hashLength = algorithm.hashlength();
    target.resize(hashLength);
    if (algorithm.key().empty())
    {
        crypto_generichash(target.data(), hashLength,
                           first, std::distance(first, last),
                           nullptr, 0);
    }
    else
    {
        crypto_generichash(target.data(), hashLength,
                           first, std::distance(first, last),
                           reinterpret_cast<const std::uint8_t*>(algorithm.key().data()), algorithm.key().length());
    }
}

void hash(std::vector<std::uint8_t>& target,
          const std::uint8_t* first,
          const std::uint8_t* last,
          proto::storage::HashAlgorithm const& algorithm)
{
    switch(algorithm.whichAlgorithm_case())
    {
        case proto::storage::HashAlgorithm::WhichAlgorithmCase ::kCryptoGenericHash:
            hash(target, first, last, algorithm.cryptogenerichash());
            break;

        case proto::storage::HashAlgorithm::WhichAlgorithmCase ::WHICHALGORITHM_NOT_SET:
            throw std::runtime_error("no algorithm case");
            break;
    }
}
