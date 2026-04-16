#pragma once

#include <QWidget>

class QLabel;
class QSpinBox;

namespace PR_tool::widget {
    class ColorPickerButton;
    class SchematicView;
}

namespace PR_tool::widget::schematic {

    class ViewInfoWidget : public QWidget {
    public:
        ViewInfoWidget(SchematicView* view, QWidget* parent = nullptr);

    private:
        SchematicView* _view {nullptr};
    };

}