#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QPainter>
#include <QDebug>

namespace PR_tool::hardware {
    class Bump;
}

namespace PR_tool::widget::view2d {

    class BumpItem : public QGraphicsEllipseItem {
    public:
        static constexpr qreal RADIUS = 5.;
        static constexpr qreal DIAMETER = 2. * RADIUS;
        static const    QColor COLOR;
        static const    QColor HOVERED_COLOR;

    public:
        BumpItem(hardware::Bump* bump, QPointF position, QGraphicsItem *parent = nullptr);

    protected:
        void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

    private:
        hardware::Bump* _bump;
    };


}