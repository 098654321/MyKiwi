#include "./topdieinst.hh"
#include <hardware/tob/tob.hh>
#include <hardware/bump/bump.hh>
#include <cassert>

namespace kiwi::circuit {

    TopDieInstance::TopDieInstance(std::String name, TopDie* topdie, hardware::TOB* tob) :
        _name{std::move(name)},
        _topdie{topdie},
        _tob{tob},
        _nets{}
    {
    }

    void TopDieInstance::place_to_idle_tob(hardware::TOB* tob) {
        assert(this->_tob.has_value());
        assert(tob != nullptr);
        if (!tob->is_idle()) {
            return;
        }

        auto origin_tob = this->tob();
        origin_tob->remove_placed_instance();
        this->_tob.emplace(tob);
        tob->set_placed_instance(this);
    }
    
    auto TopDieInstance::swap_tob_with(TopDieInstance* other) -> void {
        auto tob1 = this->tob();
        auto tob2 = other->tob();
        
        // 1. 收集所有受影响的 Net (去重)
        std::HashSet<Net*> affected_nets;
        for (auto net : this->_nets) {
            affected_nets.insert(net);
        }
        for (auto net : other->_nets) {
            affected_nets.insert(net);
        }

        // 2. 对每个 Net 执行原子交换
        for (auto net : affected_nets) {
            net->swap_tob_position(tob1, tob2);
        }

        // 3. 交换 Instance 自身的 TOB 指针
        this->_tob = tob2;
        other->_tob = tob1;

        // 4. Update placed instance
        tob1->set_placed_instance(other);
        tob2->set_placed_instance(this);
    }

    auto TopDieInstance::move_to_tob(hardware::TOB* tob) -> void {
        assert(tob != nullptr);

        auto prev_tob = this->_tob;
        auto next_tob = tob;
        this->_tob = next_tob;
        if (prev_tob.has_value() && *prev_tob != next_tob) {
            (*prev_tob)->remove_placed_instance();
            next_tob->set_placed_instance(this);
        }
        for (auto net : this->_nets) {
            assert(prev_tob.has_value());
            net->update_tob_postion(*prev_tob, next_tob);
        }
    }



    auto TopDieInstance::add_net(Net* net) -> void {
        this->_nets.emplace_back(net);
    }

    auto TopDieInstance::replace_net(Net* old_net, Net* new_net) -> void {
        if (old_net == nullptr || new_net == nullptr || old_net == new_net) {
            return;
        }

        for (auto& net : this->_nets) {
            if (net == old_net) {
                net = new_net;
            }
        }
    }
}
