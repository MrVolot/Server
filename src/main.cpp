#include <boost/asio.hpp>
#include <iostream>
#include <boost/optional/optional.hpp>
#include <boost/utility/typed_in_place_factory.hpp>
#include "server.h"

int main()
{
    try {
        boost::asio::io_service io_service;
        Server server{ io_service };
        boost::optional<boost::asio::io_service::work> work;
        work.emplace(io_service);
        io_service.run();
    }
    catch (std::exception& ex) {
        std::cout << ex.what();
    }
}