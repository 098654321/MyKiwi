#pragma once 

#include <std/utility.hh>
#include <std/integer.hh>
#include <assert.h>

namespace kiwi::hardware {

    class TOBMuxConnector;

    enum class TOBMuxRegState {
        Given_out,          // given out(temperarily stored in PathPackage) but not connected
        Stay_inside         // neither given out nor connected
    };

    class TOBMuxRegister {
    public:
        TOBMuxRegister(std::usize index) :
            _index{index}, _state{std::Tuple<TOBMuxRegState, std::Option<std::usize>>{TOBMuxRegState::Stay_inside, std::nullopt}} {}

        TOBMuxRegister():
            _index{std::nullopt}, _state{std::Tuple<TOBMuxRegState, std::Option<std::usize>>{TOBMuxRegState::Stay_inside, std::nullopt}} {} 

        auto get() const -> std::Option<std::usize> {
            return this->_index;
        }

        auto is_given_out() const -> bool {
            auto& [reg_state, reg_output] = this->_state;
            return reg_state == TOBMuxRegState::Given_out;
        }

        auto given_out_index() const -> std::Option<std::usize> {
            auto& [reg_state, reg_output] = this->_state;
            return reg_output;
        }

        auto give_out(std::usize output_index) -> void {
            auto& [reg_state, reg_output] = this->_state;
            reg_state = TOBMuxRegState::Given_out;
            reg_output = output_index;
        }

        auto stay_inside() -> void {
            auto& [reg_state, reg_output] = this->_state;
            reg_state = TOBMuxRegState::Stay_inside;
            reg_output.reset();
        }

        // Warning!!! All 'TOBMuxRegister' in one TOBMux must `None` or have different value. 
        // Becarefull when you call `set` directly 
        auto set(std::usize index) -> void {
            auto& [reg_state, reg_output] = this->_state;
            if (reg_state == TOBMuxRegState::Given_out) {
                assert(reg_output.has_value() && reg_output.value() == index);
            }

            this->_index.emplace(index);
            reg_state = TOBMuxRegState::Given_out;
            reg_output.emplace(index);
        }

        auto reset() -> void {
            this->_index.reset();

            auto& [reg_state, reg_output] = this->_state;
            reg_state = TOBMuxRegState::Stay_inside;
            reg_output.reset();
        }

    private:
        std::Option<std::usize> _index; // target mux index
        std::Tuple<TOBMuxRegState, std::Option<std::usize>> _state;
    };

    //////////////////////////////////////////////////

    enum class TOBBumpDirection {
        DisConnected,
        BumpToTOB,
        TOBToBump,
    };

    class TOBBumpDirRegister {
    public:
        TOBBumpDirRegister(TOBBumpDirection direction) :
            _direction{direction} {}

        TOBBumpDirRegister() :
            TOBBumpDirRegister{TOBBumpDirection::DisConnected} {}

        auto get() const -> TOBBumpDirection { 
            return this->_direction; 
        }

        auto set(TOBBumpDirection direction) -> void {
            this->_direction = direction;
        } 

        auto reset() -> void {
            this->set(TOBBumpDirection::DisConnected);
        }

    private:
        TOBBumpDirection _direction;
    };

    //////////////////////////////////////////////////

    enum class TOBTrackDirection {
        DisConnected,
        TrackToTOB,
        TOBToTrack,
    }; 

    class TOBTrackDirRegister {
    public:
        TOBTrackDirRegister(TOBTrackDirection direction) :
            _direction{direction} {}

        TOBTrackDirRegister() :
            TOBTrackDirRegister{TOBTrackDirection::DisConnected} {}

        auto get() const -> TOBTrackDirection { 
            return this->_direction; 
        }

        auto set(TOBTrackDirection direction) -> void {
            this->_direction = direction;
        } 

        auto reset() -> void {
            this->set(TOBTrackDirection::DisConnected);
        }

    private:
        TOBTrackDirection _direction;
    };

}