#include "./net.hh"


namespace kiwi::circuit {

    auto Net::set_pathpackage(const PathPackage& path_package) -> void {
        if (path_package._regular_path.size() == 0 || path_package._length == 0){
            debug::info("path package is empty");
        }
        // this->_history_path_package.emplace(HistoryPathPackage(this->_path_package));
        this->_path_package = path_package;
    }

    auto Net::set_history_pathpackage() -> void {
        this->_history_path_package.emplace(HistoryPathPackage(this->_path_package));
        debug::info_fmt(
            "set history path package with length {} from current path package with length {}", 
            this->_history_path_package.value()._length, this->_path_package._length
        );
    }

    auto Net::show_path() const -> void {
        this->_path_package.show();
    }

    auto Net::length() const -> std::usize {
        return this->_path_package._length;
    }

    auto Net::clear_related_nets() -> void {
        this->_related_nets_bump.clear();
        this->_related_nets_track.clear();
    }

    auto Net::clear_path() -> void {
        this->_path_package.clear_all();
        this->_history_path_package.reset();
        this->_related_nets_bump.clear();
        this->_related_nets_track.clear();
    }

    auto Net::clear_current_package() -> void {
        this->_path_package.clear_all();
    }

    auto Net::move_history_to_current(hardware::Interposer* interposer) -> void {
        if (this->_history_path_package.has_value()) {
            debug::info_fmt("net {} move history path package to current path package with length {}", this->name(), this->_history_path_package.value()._length);
            this->_path_package = circuit::PathPackage(this->_history_path_package.value(), interposer);
            this->_path_package.occupy_all();
        }
        else {
            debug::info_fmt("net {} has empty history path package", this->name());
        }
    }

}

