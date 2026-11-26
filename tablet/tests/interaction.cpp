#include "../src/questionmark.h"
#include "../src/letterpaths.h"
#include <cassert>
#include <iostream>
int main() {
    QPolygonF hook;hook<<QPointF(0,25)<<QPointF(15,5)<<QPointF(45,0)<<QPointF(80,10)<<QPointF(100,30)<<QPointF(95,48)<<QPointF(75,62)<<QPointF(48,75)<<QPointF(45,100);
    for(double scale:{.25,.5,1.0,1.5}) {
        QPolygonF h,d;
        for(auto p:hook)h<<p*scale+QPointF(400,500);
        d<<QPointF(445*scale+400*(1-scale),625*scale+500*(1-scale));
        assert(diaryQuestionMark(h,d));
        QPolygonF above;above<<h.first()-QPointF(0,20);assert(!diaryQuestionMark(h,above));
        QPolygonF beside;beside<<h.last()+QPointF(200,30);assert(!diaryQuestionMark(h,beside));
        QPolygonF bigDot;bigDot<<h.last()+QPointF(0,20)<<h.last()+QPointF(50,70);assert(!diaryQuestionMark(h,bigDot));
    }
    QPolygonF dot;dot<<QPointF(45,125);
    QPolygonF straight;for(int i=0;i<10;++i)straight<<QPointF(45,i*10);assert(!diaryQuestionMark(straight,dot));
    QPolygonF reverse;for(int i=hook.size()-1;i>=0;--i)reverse<<hook[i];assert(!diaryQuestionMark(reverse,dot));
    QPolygonF two;two<<QPointF(0,25)<<QPointF(20,0)<<QPointF(80,0)<<QPointF(100,30)<<QPointF(0,100)<<QPointF(100,100);assert(!diaryQuestionMark(two,dot));
    QImage transparent(100,60,QImage::Format_ARGB32);transparent.fill(Qt::transparent);
    assert(letterPaths(transparent).empty());
    for(int x=20;x<80;++x)transparent.setPixelColor(x,30,Qt::black);
    auto paths=letterPaths(transparent);assert(!paths.empty());
    for(auto path:paths)for(auto p:path)assert(p.y()>=29 && p.y()<=31 && p.x()>=19 && p.x()<=81);
    // A line crossing the former 60px grid must finish its left glyph before
    // visiting the next glyph; shifting a page must not change reading order.
    for(int shift:{0,15,40,65}) {
        QImage rows(150,240,QImage::Format_ARGB32);rows.fill(Qt::transparent);
        for(int x:{20,90})for(int y:{45+shift,70+shift})
            for(int dx=0;dx<12;++dx)rows.setPixelColor(x+dx,y,Qt::black);
        for(int dx=0;dx<12;++dx)rows.setPixelColor(10+dx,160+shift,Qt::black);
        rows.setText("reply-lines",QString("%1,%2;%3,%4").arg(35+shift).arg(90+shift).arg(150+shift).arg(185+shift));
        auto ordered=letterPaths(rows);assert(ordered.size()==5);
        assert(ordered[0].boundingRect().left()<40 && ordered[1].boundingRect().left()<40);
        assert(ordered[2].boundingRect().left()>80 && ordered[3].boundingRect().left()>80);
        assert(ordered[4].boundingRect().top()>140+shift);
    }
    std::cout<<"PASS: scaled question-mark gestures, non-question rejection, transparent ink tracing, layout-aware reading order\n";
}
