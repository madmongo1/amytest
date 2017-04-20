//
// Created by Richard Hodges on 20/04/2017.
//

#pragma once
#include <cstdlib>

struct base64
{
int
needed_encoded_length(int length_of_data) const;

int
needed_decoded_length(int length_of_encoded_data) const;

    int
    encode(const void *src, size_t src_len, char *dst) const;

    int
    decode(const char *src_base, size_t len,
           void *dst, const char **end_ptr) const;

};