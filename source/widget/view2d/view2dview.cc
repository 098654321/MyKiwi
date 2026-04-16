#include "./view2dview.h"

namespace PR_tool::widget {

    View2DView::View2DView(QWidget *parent) :
        GraphicsView{parent}
    {
        this->setDragMode(QGraphicsView::RubberBandDrag);
        this->setInteractive(true);
        this->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }

    View2DView::~View2DView() noexcept {}

}