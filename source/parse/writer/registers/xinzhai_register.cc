#include "xinzhai_register.hh"
#include <format>


namespace PR_tool::parse
{
    XinzhaiRegister::~XinzhaiRegister() noexcept {
        this->_pinterposer = nullptr;
    }

    auto XinzhaiRegister::fetch_controlbits(RegisterValue& rv) -> void
    {
        fetch_padctrl_right(rv.xinzhai.padctrl_right);
        fetch_padctrl_left(rv.xinzhai.padctrl_left);
        fetch_padctrl_up(rv.xinzhai.padctrl_up);
        fetch_padctrl_down(rv.xinzhai.padctrl_down);

        fetch_SiPpadctrl_right(rv.xinzhai.SiPpadctrl_right);
        fetch_SiPpadctrl_left(rv.xinzhai.SiPpadctrl_left);
        fetch_SiPpadctrl_up(rv.xinzhai.SiPpadctrl_up);
        fetch_SiPpadctrl_down(rv.xinzhai.SiPpadctrl_down);
    }

    auto XinzhaiRegister::fetch_padctrl_right(std::Bits<128>& bits) -> void {
        this->fetch_padctrl_template(bits, 2, 11, hardware::COBDirection::Up);
    }

    auto XinzhaiRegister::fetch_padctrl_left(std::Bits<128>& bits) -> void {
        this->fetch_padctrl_template(bits, 8, 2, hardware::COBDirection::Right);
    }

    auto XinzhaiRegister::fetch_padctrl_up(std::Bits<128>& bits) -> void {
        this->fetch_padctrl_template(bits, 0, 1, hardware::COBDirection::Left);
    }

    auto XinzhaiRegister::fetch_padctrl_down(std::Bits<128>& bits) -> void {
        this->fetch_padctrl_template(bits, 8, 10, hardware::COBDirection::Right);
    }

    auto XinzhaiRegister::fetch_SiPpadctrl_right(std::Bits<128>& bits) -> void {
        this->fetch_SiPpadctrl_template(bits, 0, 8, hardware::COBDirection::Left);
    }

    auto XinzhaiRegister::fetch_SiPpadctrl_left(std::Bits<128>& bits) -> void {
        this->fetch_SiPpadctrl_template(bits, 4, 0, hardware::COBDirection::Down);
    }

    auto XinzhaiRegister::fetch_SiPpadctrl_up(std::Bits<128>& bits) -> void {
        this->fetch_SiPpadctrl_template(bits, 0, 5, hardware::COBDirection::Left);
        auto pcob = _pinterposer->get_cob(0, 4);
        if (!pcob.has_value())
        {
            throw std::logic_error("cob at (0, 4) does not exist");
        }
        auto cob = pcob.value();

        bits[0] = cob->get_sel_resgiter_value(hardware::COBDirection::Left, 0) == hardware::COBSignalDirection::TrackToCOB ? 1 : 0;
        bits[8] = cob->get_sel_resgiter_value(hardware::COBDirection::Left, 8) == hardware::COBSignalDirection::TrackToCOB ? 1 : 0;
        bits[16] = cob->get_sel_resgiter_value(hardware::COBDirection::Left, 16) == hardware::COBSignalDirection::TrackToCOB ? 1 : 0;
        bits[24] = cob->get_sel_resgiter_value(hardware::COBDirection::Left, 24) == hardware::COBSignalDirection::TrackToCOB ? 1 : 0;
    }

    auto XinzhaiRegister::fetch_SiPpadctrl_down(std::Bits<128>& bits) -> void {
        this->fetch_SiPpadctrl_template(bits, 8, 6, hardware::COBDirection::Right);
    }

    auto XinzhaiRegister::fetch_padctrl_template(std::Bits<128>& bits, std::i64 row, std::i64 col, hardware::COBDirection dir) -> void
    {
        auto pcob = _pinterposer->get_cob(row, col);
        if (!pcob.has_value())
        {
            throw std::logic_error(std::format("cob at ({}, {}) does not exist", row, col));
        }
        auto cob = pcob.value();

        // group 1
        for (std::size_t index = 127; index >= 96; --index) {
            bits[index] = 1;
        }

        // group 2
        for (std::size_t index = 95; index >= 64; --index) {
            bits[index] = 0;
        }

        // group 3
        for (std::size_t index = 63; index >= 48; --index) {
            std::size_t port_index = this->_cob_pad_ext_port_index[15 - (index - 48)];
            auto value = cob->get_sel_resgiter_value(dir, port_index);
            if (value == hardware::COBSignalDirection::TrackToCOB)
            {
                bits[index] = 1;
            }
            else
            {
                bits[index] = 0;
            }
        }

        // group 4
        for (std::size_t index = 47; index >= 32; --index) {
            bits[index] = 0;
        }

        // group 5
        for (int index = 31; index >= 0; --index) {
            bits[index] = 0;
        }
    }

    auto XinzhaiRegister::fetch_SiPpadctrl_template(std::Bits<128>& bits, std::i64 row, std::i64 col, hardware::COBDirection dir) -> void
    {
        auto pcob = _pinterposer->get_cob(row, col);
        if (!pcob.has_value())
        {
            throw std::logic_error(std::format("cob at ({}, {}) does not exist", row, col));
        }
        auto cob = pcob.value();

        // group 1
        for (std::size_t index = 127; index >= 96; --index) {
            bits[index] = 1;
        }

        // group 2
        for (std::size_t index = 95; index >= 64; --index) {
            bits[index] = 0;
        }

        // group 3
        for (std::size_t index = 63; index >= 48; --index) {
            std::size_t port_index = this->_cob_Sip_ext_port_index[15 - (index - 48)];
            auto value = cob->get_sel_resgiter_value(dir, port_index);
            if (value == hardware::COBSignalDirection::TrackToCOB)
            {
                bits[index] = 1;
            }
            else
            {
                bits[index] = 0;
            }
        }

        // group 4
        for (std::size_t index = 47; index >= 32; --index) {
            bits[index] = 0;
        }

        // group 5
        for (int index = 31; index >= 0; --index) {
            bits[index] = 0;
        }
    }
    
}

