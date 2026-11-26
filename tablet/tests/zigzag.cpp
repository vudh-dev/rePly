#include "../src/zigzag.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <cassert>
#include <iostream>
int main(){
 QFile f("tests/zigzag-samples.json");assert(f.open(QIODevice::ReadOnly));
 auto samples=QJsonDocument::fromJson(f.readAll()).array();assert(samples.size()==20);
 int index=0;std::vector<QPolygonF> strokes;
 for(auto value:samples){QPolygonF p;for(auto xy:value.toArray()){auto a=xy.toArray();p<<QPointF(a[0].toDouble(),a[1].toDouble());}
  if(!diaryZigzag(p)){std::cerr<<"Rejected user example "<<index<<"\n";return 1;}++index;
  for(double angle:{0.,.5,1.5,3.})for(double scale:{.5,1.,2.}){
   QPolygonF q;for(auto t:p)q<<QPointF(scale*(t.x()*cos(angle)-t.y()*sin(angle))+300,scale*(t.x()*sin(angle)+t.y()*cos(angle))+300);
   assert(diaryZigzag(q));std::reverse(q.begin(),q.end());assert(diaryZigzag(q));
  }strokes.push_back(p);
 }
 QPolygonF line,circle,v,check;
 for(int i=0;i<40;++i){line<<QPointF(i*2,i);circle<<QPointF(50+40*cos(i*2*3.141592653589793/39),50+40*sin(i*2*3.141592653589793/39));}
 for(int i=0;i<20;++i)v<<QPointF(i*2,i*2);for(int i=0;i<20;++i)v<<QPointF(40+i*2,40-i*2);
 assert(!diaryZigzag(line));assert(!diaryZigzag(circle));assert(!diaryZigzag(v));
 QElapsedTimer timer;timer.start();for(int repeat=0;repeat<100;++repeat)for(auto &p:strokes)assert(diaryZigzag(p));
 std::cout<<"PASS: all 20 real gestures, scale/rotation/reversal variations, line/circle/V rejection; "<<timer.nsecsElapsed()/2000000000.<<" ms per gesture (host)\n";
}
