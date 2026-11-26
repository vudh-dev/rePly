#pragma once
#include <QPolygonF>
#include <QLineF>
#include <vector>
#include <cmath>
#include <algorithm>

// Recognize the conventional hook followed by a separate dot, using pen
// geometry only. No screenshot, notebook history or network call is needed.
inline std::vector<QPointF> diaryResample(const QPolygonF &stroke,int count=32) {
    std::vector<QPointF> out;
    if(stroke.size()<2)return out;
    std::vector<double> lengths{0};
    for(int i=1;i<stroke.size();++i)lengths.push_back(lengths.back()+QLineF(stroke[i-1],stroke[i]).length());
    if(lengths.back()<1)return out;
    size_t segment=1;
    for(int i=0;i<count;++i) {
        double distance=lengths.back()*i/(count-1);
        while(segment+1<lengths.size() && lengths[segment]<distance)++segment;
        double span=lengths[segment]-lengths[segment-1];
        double t=span>0?(distance-lengths[segment-1])/span:0;
        out.push_back(stroke[int(segment-1)]*(1-t)+stroke[int(segment)]*t);
    }
    return out;
}
inline bool diaryQuestionMark(const QPolygonF &hook,const QPolygonF &dot) {
    if(hook.size()<5 || dot.isEmpty())return false;
    QRectF box=hook.boundingRect(),db=dot.boundingRect();
    double h=box.height(),w=box.width();
    if(h<18 || h>200 || w<h*.25 || w>h*1.5)return false;
    if(db.width()>std::max(10.0,h*.30) || db.height()>std::max(10.0,h*.30))return false;
    QPointF d=db.center(),end=hook.last();
    if(d.y()<box.bottom()+h*.03 || d.y()>box.bottom()+h*.70 || std::abs(d.x()-end.x())>w*.6+4)return false;
    // A question mark starts near the upper left, curves across the top and
    // comes back toward the centre before ending below the arc.
    QPointF start=hook.first();
    if(start.y()>box.top()+h*.55 || end.y()<box.top()+h*.78 || end.x()>box.left()+w*.85)return false;
    QPolygonF normalized;
    for(auto p:hook)normalized<<QPointF(100*(p.x()-box.left())/w,100*(p.y()-box.top())/h);
    auto sample=diaryResample(normalized);
    QPolygonF shape;shape<<QPointF(0,25)<<QPointF(15,5)<<QPointF(45,0)<<QPointF(80,10)<<QPointF(100,30)<<QPointF(95,48)<<QPointF(75,62)<<QPointF(48,75)<<QPointF(45,100);
    auto reference=diaryResample(shape);
    if(sample.empty())return false;
    double error=0;
    for(size_t i=0;i<sample.size();++i)error+=QLineF(sample[i],reference[i]).length()/100;
    return error/sample.size()<.24;
}
