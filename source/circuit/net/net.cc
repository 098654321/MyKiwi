#include "./net.hh"


namespace kiwi::circuit {

    auto Net::set_pathpackage(const PathPackage& path_package) -> void {
        if (path_package._regular_path.size() == 0 || path_package._length == 0){
            debug::debug("path package is empty");
        }
        this->_path_package = path_package;
    }

    auto Net::show() const -> void {
        this->_path_package.show();
    }

    auto Net::length() const -> std::usize {
        return this->_path_package._length;
    }

    auto Net::clear_related_nets() -> void {
        this->_related_nets_bump.clear();
        this->_related_nets_track.clear();
    }

}

