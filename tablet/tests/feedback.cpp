#include "../src/feedback.h"
#include <cassert>
#include <iostream>
int main() {
    QSize canvas(1620,2160);
    for(auto stroke:{QRectF(800,400,40,60),QRectF(0,0,8,12),QRectF(1590,2130,29,29)}) {
        auto box=diaryFeedbackRect(stroke,canvas);
        assert(QRect(QPoint(0,0),canvas).contains(box));assert(box.contains(stroke.toAlignedRect()));
    }
    QRect area(174,0,1353,2021),box=diaryFeedbackRect(QRectF(800,400,40,60),canvas);
    QImage a(area.size(),QImage::Format_RGB32);a.fill(Qt::white);QImage b=a.copy();
    {QPainter p(&a);p.fillRect(box.translated(-area.topLeft()),Qt::black);}
    {QPainter p(&b);p.drawLine(box.topLeft()-area.topLeft(),box.bottomRight()-area.topLeft());}
    diaryMaskFeedback(a,box,area.topLeft());diaryMaskFeedback(b,box,area.topLeft());assert(a==b);
    b.setPixelColor(100,100,Qt::black);assert(a!=b);
    auto untouched=b.copy();diaryMaskFeedback(b,{},area.topLeft());assert(untouched==b);
    std::cout<<"PASS: gesture placement at screen edges, animation frames excluded, outside edits still detected\n";
}
