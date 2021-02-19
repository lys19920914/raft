#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include "unistd.h"
#include <boost/bind.hpp>

using namespace boost;
using namespace boost::asio;

void handler(const boost::system::error_code& err, deadline_timer* pt){
    if (err){
    
	std::cout<<"timer cancel!"<<std::endl;
	std::cout<<std::this_thread::get_id()<<std::endl;



    } else {
	std::cout<<std::this_thread::get_id()<<std::endl;
    }
}

int main(){
    io_service io;	
    io_service::work work(io);

    std::thread t([&](){io.run();});
    t.join();


    
    return 0;
    

}
