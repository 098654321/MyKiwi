#pragma once

#include <QThread>
#include <debug/debug.hh>
#include <algo/route_data.hh>
#include <algo/router/single_mode/route_nets.hh>
#include <algo/netbuilder/netbuilder.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <algo/router/common/allocate/hopcroft_karp.hh>

namespace kiwi::widget {

    class PRThread : public QThread {
        Q_OBJECT

    public:
        PRThread(hardware::Interposer* i, circuit::BaseDie* b) : 
            QThread{},
            _interposer{i},
            _basedie{b} 
        {
        }

    protected:
        void run() override {
            debug::info_fmt("Begin to execute P&R");
            algo::build_nets(this->_basedie, this->_interposer);
            auto data = algo::route_nets(this->_interposer, this->_basedie, algo::MazeRouteStrategy{}, algo::HK{}, 0, false, false);
            debug::info_fmt("P&R finished with total path length '{}'", data._total_length); 
            emit this->prFinished();
        }

    signals:
        void prFinished();

    protected:
        hardware::Interposer* _interposer;
        circuit::BaseDie* _basedie;
    };

}