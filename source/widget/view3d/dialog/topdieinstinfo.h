#pragma once

#include <QDialog>

class QComboBox;
class QSpinBox;
class QTreeView;
class QStandardItemModel;
class QLabel;

namespace PR_tool::circuit {
    class TopDieInstance;
}

namespace PR_tool::widget {

    class TopDieInsDialog : public QDialog {
    public:
        TopDieInsDialog(circuit::TopDieInstance* topdieinst);
        ~TopDieInsDialog();
        
    private:
        auto updateTreeView(int bumpIndex) -> void;

    private:
        circuit::TopDieInstance* _topdieinst;
    };

}