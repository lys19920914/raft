//
// Created by liangyusen on 2018/8/20.
//
#define BOOST_LOG_DYN_LINK

#include <stdarg.h>
#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#define LOG(...) Logger::GetLogger()->Print(__VA_ARGS__)

enum class LogType {
    trace, debug, info, warning, error, fatal
};

class Logger {
public:
    bool Init(bool file = false, std::string filename = "", LogType severity = LogType::info) {
        if (file && filename == "") {
            std::cerr << " filename is null" << std::endl;
            return false;
        }

        boost::log::add_common_attributes();
        boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
        if (file) {
            boost::log::add_file_log(
                    boost::log::keywords::file_name = filename,
                    boost::log::keywords::format = "[%TimeStamp%][%Severity%]: %Message%",
                    boost::log::keywords::auto_flush = true
            );

        } else {
            boost::log::add_console_log(
                    std::clog,
                    boost::log::keywords::format = "[%TimeStamp%][%Severity%]: %Message%"
            );
        }

        switch (severity) {
            case LogType::trace:
                boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
                break;
            case LogType::debug:
                boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
                break;
            case LogType::info:
                boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
                break;
            case LogType::warning:
                boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::warning);
                break;
            case LogType::error:
                boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::error);
                break;
            case LogType::fatal:
                boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::fatal);
                break;
        }

        return true;
    }

    void Print(LogType logType, std::string logMsg, ...) {
        char buf[1024];
        va_list args;
        va_start(args, logMsg);
        vsprintf(buf, logMsg.c_str(), args);
        va_end(args);
        std::string str(buf);
        switch (logType) {
            case LogType::trace:
                BOOST_LOG_TRIVIAL(trace) << str;
                break;
            case LogType::debug:
                BOOST_LOG_TRIVIAL(debug) << str;
                break;
            case LogType::info:
                BOOST_LOG_TRIVIAL(info) << str;
                break;
            case LogType::warning:
                BOOST_LOG_TRIVIAL(warning) << str;
                break;
            case LogType::error:
                BOOST_LOG_TRIVIAL(error) << str;
                break;
            case LogType::fatal:
                BOOST_LOG_TRIVIAL(fatal) << str;
                break;
        }
    }

    static Logger *GetLogger() {
        if (_instance == nullptr)
            _instance = new Logger();
        return _instance;
    }

private:
    static Logger *_instance;
};

Logger *Logger::_instance = nullptr;

int main() {
    Logger::GetLogger()->Init(true, "node/log", LogType::debug);

    LOG(LogType::trace, "%i, %s", 1, "a");
    Logger::GetLogger()->Print(LogType::debug, "%i, %s", 1, "a");
    Logger::GetLogger()->Print(LogType::info, "%i, %s", 1, "a");
    Logger::GetLogger()->Print(LogType::warning, "%i, %s", 1, "a");
    Logger::GetLogger()->Print(LogType::error, "%i, %s", 1, "a");
    Logger::GetLogger()->Print(LogType::fatal, "%i, %s", 1, "a");
//    logger.Print(LogType::trace, "%i, %s", 1, "a");
//    logger.Print(LogType::debug, "%i, %s", 1, "a");
//    logger.Print(LogType::info, "%i, %s", 1, "a");
//    logger.Print(LogType::warning, "%i, %s", 1, "a");
//    logger.Print(LogType::error, "%i, %s", 1, "a");
//    logger.Print(LogType::fatal, "%i, %s", 1, "a");


    return 0;
}
