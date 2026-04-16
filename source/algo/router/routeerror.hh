#pragma once

#include <debug/exception.hh>
#include <exception>

namespace PR_tool::circuit {
    class Net;
}

namespace PR_tool::algo {
        // 1. no available tracks for bump
        // 2. path not found
        // 3. illogical events，such as "end track" not in "end_tracks_set"

    class RouteExpt : public std::exception {
    public:
        RouteExpt(const std::string& msg) noexcept
        : _errmsg(msg)
        {}

        virtual ~RouteExpt() noexcept {}
    
    public:
        virtual const char* what() const noexcept {
            return this->_errmsg.c_str();
        }

    protected:
        std::string _errmsg;      
    };

    class RetryExpt: public RouteExpt {
    public:
        RetryExpt(const std::string& msg) noexcept
        : RouteExpt(msg), _net{nullptr}
        {}
        RetryExpt(const std::string& msg, circuit::Net* net) noexcept
        : RouteExpt(msg), _net{net}
        {}

        ~RetryExpt() noexcept {}

    public:
        auto net() const -> circuit::Net* {return this->_net;}
        auto set_net(circuit::Net* net) -> void {this->_net = net;}
    
    private:
        circuit::Net* _net;
    };

    class FinalError: public RouteExpt {
    public:
        FinalError(const std::string& msg) noexcept
        : RouteExpt(msg)
        {}

        ~FinalError() noexcept {}
    };
}