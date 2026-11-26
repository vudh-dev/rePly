#include "../src/inkpagination.h"
#include <QLineF>
#include <cassert>
#include <iostream>
int main(){
 QImage image(200,3000,QImage::Format_RGB32);
 image.setText("reply-lines","1150,1220;1190,1250;2320,2410");
 auto cuts=diaryInkCuts(image);assert(cuts[1]==1147);assert(cuts.back()==3000);
 for(size_t i=1;i<cuts.size();++i)assert(cuts[i]>cuts[i-1]&&cuts[i]-cuts[i-1]<=1200);
 QPolygonF vertical;vertical<<QPointF(10,0)<<QPointF(10,2999);
 QPolygonF horizontal;horizontal<<QPointF(0,1200)<<QPointF(100,1200);
 double length=0;int horizontals=0;
 for(auto span:std::vector<QPair<double,double>>{{0,1200},{1200,2400},{2400,3000}}){
  auto pieces=diaryInkBand({vertical,horizontal},span.first,span.second);
  for(auto p:pieces)for(int i=1;i<p.size();++i){assert(p[i].y()>=span.first&&p[i].y()<=span.second);length+=QLineF(p[i-1],p[i]).length();if(p[i].y()==p[i-1].y())++horizontals;}
 }
 assert(std::abs(length-3099)<0.001);assert(horizontals==1);
 QPolygonF reverse;reverse<<QPointF(10,2999)<<QPointF(10,0);auto clipped=diaryInkBand({reverse},1200,2400);assert(clipped.size()==1&&clipped[0].first().y()==2400&&clipped[0].last().y()==1200);
 std::cout<<"Ink pagination tests passed\n";
}
