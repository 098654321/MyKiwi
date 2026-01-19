#include "./connection.hh"

namespace kiwi::circuit {

    Connection::Connection(int mode, int sync, Pin input, Pin output) :
        _mode{mode},
        _sync{sync},
        _input{input},
        _output{output}
    {
    }

    

}