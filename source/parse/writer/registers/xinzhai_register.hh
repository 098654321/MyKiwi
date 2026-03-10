#pragma once


#include "baseregister.hh"


namespace kiwi::hardware
{
    class Interposer;
}


namespace kiwi::parse {

    class XinzhaiRegister : public BaseRegister
    {
    public:
        XinzhaiRegister(hardware::Interposer* pinterpsoer):
            _pinterposer{pinterpsoer}, 
            _cob_pad_ext_port_index{71, 9, 78, 18, 85, 0, 92, 27, 45, 99, 54, 106, 36, 113, 63, 120},
            _cob_Sip_ext_port_index{0, 9, 18, 27, 36, 45, 54, 63, 71, 78, 85, 92, 99, 106, 113, 120}
        {}

        ~XinzhaiRegister() noexcept;

    public:
        auto fetch_controlbits(RegisterValue& rv) -> void override;

    private:
        auto fetch_padctrl_right(std::Bits<128>&) -> void;
        auto fetch_padctrl_left(std::Bits<128>&) -> void;
        auto fetch_padctrl_up(std::Bits<128>&) -> void;
        auto fetch_padctrl_down(std::Bits<128>&) -> void;
        auto fetch_SiPpadctrl_right(std::Bits<128>&) -> void;
        auto fetch_SiPpadctrl_left(std::Bits<128>&) -> void;
        auto fetch_SiPpadctrl_up(std::Bits<128>&) -> void;
        auto fetch_SiPpadctrl_down(std::Bits<128>&) -> void;

        auto fetch_padctrl_template(std::Bits<128>&, std::i64, std::i64, hardware::COBDirection) -> void;
        auto fetch_SiPpadctrl_template(std::Bits<128>&, std::i64, std::i64, hardware::COBDirection) -> void;

    private:
        hardware::Interposer* _pinterposer;
        std::size_t _cob_pad_ext_port_index[16];
        std::size_t _cob_Sip_ext_port_index[16];
    };

}

