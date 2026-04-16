#pragma once

#include <widget/frame/graphicsview.h>
#include <QWidget>
#include <QGraphicsView>

namespace PR_tool::hardware {
    class Interposer;
    class TOB;
    class COB;
    class Track;
};

namespace PR_tool::circuit {
    class BaseDie;
    class TopDieInstance;
};

namespace PR_tool::widget {
   
    class LayoutScene;

    class LayoutView : public GraphicsView {
    public:
        explicit LayoutView(QWidget *parent = nullptr);

        ~LayoutView() noexcept;
    };

}