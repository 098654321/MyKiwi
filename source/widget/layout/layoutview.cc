#include "./layoutview.h"

namespace PR_tool::widget {

    LayoutView::LayoutView(
        QWidget *parent
    ) :
        GraphicsView{parent}
    {
        this->setDragMode(QGraphicsView::RubberBandDrag);
        this->setInteractive(true);
        this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }

    LayoutView::~LayoutView() noexcept {}

}