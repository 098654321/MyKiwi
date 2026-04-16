#pragma once

#include <widget/frame/graphicsview.h>

namespace PR_tool::widget {
   
    class View2DView : public GraphicsView {

    public:
        explicit View2DView(QWidget *parent = nullptr);

        ~View2DView() noexcept;
    };

}