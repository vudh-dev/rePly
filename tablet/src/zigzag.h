#pragma once
#include <QPolygonF>
#include <QLineF>
#include <algorithm>
#include <cmath>
#include <limits>

// Fit three segments to a single pen stroke. Rotation, scale and drawing
// direction are unrestricted; only two alternating substantial turns matter.
inline bool diaryZigzag(const QPolygonF &stroke) {
    if(stroke.size()<6 || stroke.size()>5000)return false;
    auto box=stroke.boundingRect();double scale=std::hypot(box.width(),box.height());
    if(box.width()<8 || box.height()<8 || scale<22 || scale>550)return false;
    double length=0;for(int i=1;i<stroke.size();++i)length+=QLineF(stroke[i-1],stroke[i]).length();
    if(length<1.15*scale || length>4.8*scale || QLineF(stroke.first(),stroke.last()).length()<.2*scale)return false;
    QPolygonF p;p<<stroke.first();int segment=1;double before=0;
    for(int k=1;k<31;++k) {
        double target=length*k/31.;
        while(segment<stroke.size()-1 && before+QLineF(stroke[segment-1],stroke[segment]).length()<target) {
            before+=QLineF(stroke[segment-1],stroke[segment]).length();++segment;
        }
        double d=QLineF(stroke[segment-1],stroke[segment]).length();
        p<<stroke[segment-1]+(stroke[segment]-stroke[segment-1])*(d>0?(target-before)/d:0);
    }
    p<<stroke.last();
    auto dot=[](QPointF a,QPointF b){return a.x()*b.x()+a.y()*b.y();};
    auto cross=[](QPointF a,QPointF b){return a.x()*b.y()-a.y()*b.x();};
    double error[32][32]{};
    for(int a=0;a<31;++a)for(int b=a+1;b<32;++b) {
        auto v=p[b]-p[a];double vv=dot(v,v);
        for(int k=a+1;k<b;++k) {
            double t=vv>0?std::clamp(dot(p[k]-p[a],v)/vv,0.,1.):0;
            auto delta=p[k]-(p[a]+t*v);error[a][b]+=dot(delta,delta);
        }
    }
    for(int i=4;i<=23;++i)for(int j=i+4;j<=27;++j) {
        auto a=p[i]-p[0],b=p[j]-p[i],c=p[31]-p[j];
        double la=std::sqrt(dot(a,a)),lb=std::sqrt(dot(b,b)),lc=std::sqrt(dot(c,c));
        if(std::min({la,lb,lc})<.17*scale)continue;
        double ab=dot(a,b)/(la*lb),bc=dot(b,c)/(lb*lc);
        if(ab>.65 || bc>.65 || ab<-.995 || bc<-.995 || dot(a,c)/(la*lc)<.05)continue;
        if(cross(a,b)*cross(b,c)>=0)continue;
        if((error[0][i]+error[i][j]+error[j][31])/(32*scale*scale)<.006)return true;
    }
    return false;
}
