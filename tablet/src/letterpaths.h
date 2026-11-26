#pragma once
#include <QImage>
#include <QPainter>
#include <QPolygonF>
#include <QLineF>
#include <QSet>
#include <vector>
#include <algorithm>
#include <cmath>

// Thin the handwriting glyphs to centre lines, then follow their connected
// strokes. This avoids both scanline filling and double-outlined letters.
inline std::vector<QPolygonF> letterPaths(const QImage &source) {
    // Flatten transparency onto white: transparent pixels often contain black
    // RGB values and must never become a full-page pen stroke.
    QImage image(source.size(),QImage::Format_RGB32);image.fill(Qt::white);
    { QPainter painter(&image);painter.drawImage(0,0,source); }
    const int w=image.width()+2,h=image.height()+2;
    std::vector<unsigned char> ink(w*h,0);
    for(int y=0;y<image.height();++y)
        for(int x=0;x<image.width();++x)
            ink[(y+1)*w+x+1]=qGray(image.pixel(x,y))<170;
    const int offsets[]={-w,-w+1,1,w+1,w,w-1,-1,-w-1};
    bool changed=true;
    while(changed) {
        changed=false;
        for(int pass=0;pass<2;++pass) {
            std::vector<int> remove;
            for(int y=1;y<h-1;++y) for(int x=1;x<w-1;++x) {
                int i=y*w+x;if(!ink[i])continue;
                int n[8],count=0,transitions=0;
                for(int k=0;k<8;++k){n[k]=ink[i+offsets[k]];count+=n[k];}
                if(count<2 || count>6)continue;
                for(int k=0;k<8;++k)if(!n[k] && n[(k+1)%8])++transitions;
                if(transitions!=1)continue;
                bool blocked=pass==0 ? (n[0]*n[2]*n[4] || n[2]*n[4]*n[6])
                                     : (n[0]*n[2]*n[6] || n[0]*n[4]*n[6]);
                if(!blocked)remove.push_back(i);
            }
            for(int i:remove)ink[i]=0;
            changed=changed || !remove.empty();
        }
    }
    auto neighbors=[&](int i) {
        std::vector<int> result;
        for(int k=0;k<8;++k) {
            int j=i+offsets[k];if(j<0 || j>=w*h || !ink[j])continue;
            // Avoid a redundant diagonal across an already connected corner.
            if((k%2) && (ink[i+offsets[(k+7)%8]] || ink[i+offsets[(k+1)%8]]))continue;
            result.push_back(j);
        }
        return result;
    };
    auto edge=[](int a,int b)->quint64 {return (quint64(std::min(a,b))<<32)|quint32(std::max(a,b));};
    auto point=[&](int i){return QPointF(i%w-1,i/w-1);};
    QSet<quint64> visited;
    std::vector<QPolygonF> paths;
    auto trace=[&](int start,int next) {
        QPolygonF path;path<<point(start);
        int previous=start,current=next;
        while(true) {
            visited.insert(edge(previous,current));path<<point(current);
            auto ns=neighbors(current);
            if(ns.size()!=2)break;
            int candidate=ns[0]==previous ? ns[1]:ns[0];
            if(visited.contains(edge(current,candidate)))break;
            previous=current;current=candidate;
        }
        paths.push_back(path);
    };
    // Endpoints and junctions first, then closed loops (o, a, e, etc.).
    for(int i=w+1;i<w*(h-1)-1;++i)if(ink[i]) {
        auto ns=neighbors(i);
        if(ns.empty()){QPolygonF dot;dot<<point(i)<<point(i)+QPointF(0.4,0);paths.push_back(dot);}
        if(ns.size()!=2)for(int j:ns)if(!visited.contains(edge(i,j)))trace(i,j);
    }
    for(int i=w+1;i<w*(h-1)-1;++i)if(ink[i])
        for(int j:neighbors(i))if(!visited.contains(edge(i,j)))trace(i,j);
    // Remove one-pixel staircase noise without moving open stroke endpoints.
    for(auto &path:paths)if(path.size()>3) {
        QPolygonF original=path;
        bool closed=original.first()==original.last();
        int count=original.size()-(closed?1:0);
        for(int i=closed?0:1;i<(closed?count:count-1);++i)
            path[i]=(original[(i+count-1)%count]+original[i]*2+original[(i+1)%count])/4;
        if(closed)path.last()=path.first();
    }
    // Prefer the renderer's actual line boxes. A fixed y/60 grid can split
    // one glyph between buckets and draw every letter's top before its bottom.
    std::vector<QPair<double,double>> lines;
    for(const auto &entry:source.text("reply-lines").split(';',Qt::SkipEmptyParts)) {
        const auto pair=entry.split(',');if(pair.size()!=2)continue;
        bool a=false,b=false;double top=pair[0].toDouble(&a),bottom=pair[1].toDouble(&b);
        if(a && b && std::isfinite(top) && std::isfinite(bottom) && bottom>top)lines.push_back({top,bottom});
    }
    // Unannotated images use ink rows separated by real whitespace, never a
    // page-origin-dependent grid. Join small gaps for dots and detached accents.
    if(lines.empty()) {
        int top=-1,last=-1;
        for(int y=0;y<image.height();++y) {
            bool occupied=false;
            for(int x=0;x<image.width() && !occupied;++x)occupied=qGray(image.pixel(x,y))<170;
            if(!occupied)continue;
            if(top<0)top=y;
            else if(y-last>9){lines.push_back({double(top),double(last+1)});top=y;}
            last=y;
        }
        if(top>=0)lines.push_back({double(top),double(last+1)});
    }
    std::sort(lines.begin(),lines.end());
    std::vector<QPair<double,double>> merged;
    for(auto line:lines) {
        if(!merged.empty() && line.first<merged.back().second)merged.back().second=std::max(merged.back().second,line.second);
        else merged.push_back(line);
    }
    auto lineFor=[&](const QRectF &box) {
        int best=0;double distance=1e100,y=box.center().y();
        for(int i=0;i<int(merged.size());++i) {
            double d=std::max({merged[i].first-y,0.0,y-merged[i].second});
            if(d<distance){distance=d;best=i;}
        }
        return best;
    };
    std::stable_sort(paths.begin(),paths.end(),[&](const QPolygonF &a,const QPolygonF &b){
        QRectF x=a.boundingRect(),y=b.boundingRect();
        int lineA=lineFor(x),lineB=lineFor(y);
        return lineA==lineB ? x.left()<y.left() : lineA<lineB;
    });
    return paths;
}
