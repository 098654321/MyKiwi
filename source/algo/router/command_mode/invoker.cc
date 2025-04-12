#include "./invoker.hh"
#include <format>


namespace kiwi::algo {

auto Invoker::invoke(
    hardware::Interposer* interposer, RouteEngine& engine
) -> void {
    while (!this->_commands.empty()) {
        this->_current_command = this->_commands.front();
        this->_commands.pop_front();    // _current_command has to be a box, not a pure_pointer
                                        // when box is poped, the command object is destroyed if no other box points to it
        
        auto command = this->_current_command.get();
        if (command == nullptr) {
            auto msg = std::format("Command {} is null", command->to_string());
            throw FinalError(msg);
        }
        command->execute(interposer, engine);
    }
}

auto Invoker::set_route_commands(bool incremental, bool path_exists) -> void {
    if (incremental) {
        this->_commands.emplace_back(this->create_command(CommandType::Sort));
        this->_commands.emplace_back(this->create_command(CommandType::Resources));
        //TODO: route
        this->_commands.emplace_back(this->create_command(CommandType::Connect));
    }
    else {
        this->_commands.emplace_back(this->create_command(CommandType::Sort));
        this->_commands.emplace_back(this->create_command(CommandType::Resources));
        // this->_commands.emplace_back(this->create_command(CommandType::Allocate));
        this->_commands.emplace_back(this->create_command(CommandType::Route));
        this->_commands.emplace_back(this->create_command(CommandType::Connect));
    }
}

auto Invoker::call_remediation(Command* c) -> bool {
    auto& remediation = c->remediation();
    if (remediation.empty()) {
        return false;
    }
    else{
        for (auto iter = remediation.rbegin(); iter != remediation.rend(); ++iter) {
            this->_commands.emplace_front(*iter);
        }
        return true;
    }
}

auto Invoker::check_command() -> bool {
    return this->_commands.empty();
}

auto Invoker::create_command(CommandType type) -> std::Rc<Command>  {
    switch (type) {
        case CommandType::Clear:
            return std::make_shared<Clear>();
        case CommandType::Connect:
            return std::make_shared<Connect>();
        case CommandType::Reroute:
            return std::make_shared<Reroute>();
        case CommandType::Resources:
            return std::make_shared<Resources>();
        case CommandType::Route:
            return std::make_shared<Route>();
        case CommandType::Incre_route:
            return std::make_shared<Incre_route>();
        case CommandType::Sort:
            return std::make_shared<Sort>();
        case CommandType::Allocate:
            return std::make_shared<Allocate>();
        default:
            throw FinalError("Invalid command type");
    }
}

}


