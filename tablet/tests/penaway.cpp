#include "../src/penaway.h"
#include <cassert>
#include <iostream>
int main() {
    DiaryPenAwayGate gate;
    assert(!gate.ready(10000));
    gate.update(0,true,true);
    gate.update(100,true,false); // Lift between letters, still hovering.
    assert(!gate.ready(3000));
    gate.update(3100,true,true);
    gate.update(3200,true,false);
    gate.update(3300,false,false);
    assert(!gate.ready(3749));assert(gate.ready(3750));
    gate.update(3760,true,false);assert(!gate.ready(5000));
    gate.update(5100,false,false);assert(!gate.ready(5549));assert(gate.ready(5550));
    // Down is independently protective if a proximity report is missing.
    gate.update(5600,false,true);assert(!gate.ready(7000));
    gate.update(7100,false,false);gate.update(7200,false,false);
    assert(gate.ready(7550));
    std::cout<<"PASS: hover pauses never accept; continuous 450ms pen-away; proximity return resets; contact overrides proximity\n";
}
