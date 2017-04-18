//
// Created by Richard Hodges on 18/04/2017.
//

#include "config.hpp"

#include <amy.hpp>
#include <iostream>

using namespace amytest;

int main()
{
    asio::io_service ios;
    amy::connector connector(ios);

    std::cout << "hello\n";
}