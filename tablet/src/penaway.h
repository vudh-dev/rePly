#pragma once
#include <QtGlobal>
// Require continuous absence from the digitizer's hover range, not merely
// pen-up between letters. Any return of the marker restarts the quiet period.
class DiaryPenAwayGate {
    qint64 awaySince=-1;
public:
    static constexpr qint64 delayMs=450;
    void update(qint64 now,bool near,bool down) {
        if(near || down)awaySince=-1;
        else if(awaySince<0)awaySince=now;
    }
    bool ready(qint64 now) const {return awaySince>=0 && now-awaySince>=delayMs;}
};
