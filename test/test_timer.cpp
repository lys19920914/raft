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
    deadline_timer Timer(io, posix_time::seconds(2));
    Timer.async_wait(bind(handler, asio::placeholders::error, &Timer));
    io_service::work work(io);
    std::thread t([&]() { io.run();});


    deadline_timer btimer(io);
    btimer.expires_from_now(posix_time::seconds(3));
    btimer.wait();
    std::cout<<"finish!"<<std::endl;

    Timer.cancel();
    btimer.expires_from_now(posix_time::seconds(5));
    btimer.wait();
    std::cout<<"wait 5s"<<std::endl;

    Timer.expires_from_now(posix_time::seconds(3));
    Timer.async_wait(bind(handler,asio::placeholders::error, &Timer));

    btimer.expires_from_now(posix_time::seconds(5));
    btimer.wait();
    std::cout << "5s pass " << std::endl;

    work.~work();
    t.join();

    
    return 0;
    

}
