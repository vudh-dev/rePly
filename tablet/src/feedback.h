#pragma once
#include <QRectF>
#include <QSize>
#include <QImage>
#include <QPainter>
inline QRect diaryFeedbackRect(const QRectF &stroke,const QSize &canvas) {
    if(stroke.isEmpty() || !canvas.isValid())return {};
    QRectF minimum(stroke.center()-QPointF(36,36),QSizeF(72,72));
    return stroke.united(minimum).adjusted(-8,-8,8,8).toAlignedRect().intersected(QRect(QPoint(0,0),canvas));
}
inline void diaryMaskFeedback(QImage &image,const QRect &screenRect,const QPoint &cropOrigin) {
    if(screenRect.isEmpty())return;
    QPainter painter(&image);painter.fillRect(screenRect.translated(-cropOrigin).adjusted(-2,-2,2,2),Qt::white);
}
