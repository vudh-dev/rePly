#pragma once
#include <QImage>
#include <QLineF>
#include <QPolygonF>
#include <vector>
#include <algorithm>
#include <cmath>

inline std::vector<double> diaryInkCuts(const QImage &image,double height=1200) {
    std::vector<QPair<double,double>> lines;
    for(auto entry:image.text("reply-lines").split(';',Qt::SkipEmptyParts)) {
        auto pair=entry.split(',');if(pair.size()!=2)continue;
        bool a,b;double top=pair[0].toDouble(&a),bottom=pair[1].toDouble(&b);
        if(a&&b&&std::isfinite(top)&&std::isfinite(bottom)&&bottom>top)lines.push_back({top-3,bottom+3});
    }
    std::vector<double> cuts{0};
    while(cuts.back()+height<image.height()) {
        double start=cuts.back(),cut=start+height;
        // Resolve overlapping line boxes from different table columns.
        bool moved=true;while(moved){moved=false;for(auto line:lines)if(line.first<cut&&line.second>cut&&line.first>start+1){cut=line.first;moved=true;}}
        if(cut<=start+1)cut=start+height;
        cuts.push_back(cut);
    }
    cuts.push_back(image.height());return cuts;
}

// Clip already-traced pen paths, so table rules crossing a section boundary
// continue at exactly the same page coordinate after scrolling.
inline std::vector<QPolygonF> diaryInkBand(const std::vector<QPolygonF> &paths,double top,double bottom) {
    std::vector<QPolygonF> out;
    for(const auto &path:paths) {
        QPolygonF part;
        auto flush=[&]{if(!part.empty()){out.push_back(part);part.clear();}};
        for(int i=1;i<path.size();++i) {
            QPointF a=path[i-1],b=path[i];double dy=b.y()-a.y(),lo=0,hi=1;
            if(std::abs(dy)<1e-9){if(a.y()<top||a.y()>=bottom){flush();continue;}}
            else {double t=(top-a.y())/dy,u=(bottom-a.y())/dy;if(t>u)std::swap(t,u);lo=std::max(lo,t);hi=std::min(hi,u);if(lo>=hi){flush();continue;}}
            QPointF first=a+(b-a)*lo,last=a+(b-a)*hi;
            if(!part.empty()&&QLineF(part.last(),first).length()>0.001)flush();
            if(part.empty())part<<first;
            part<<last;
            if(hi<1)flush();
        }
        flush();
    }return out;
}
