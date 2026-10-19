#include "gpu/hud_layout.hpp"
#include <cassert>
#include <iostream>
using namespace mhp2g::gpu::ui;
HudProbe clock_probe() { return {{8,8,60,56},{0,32,52,80},0x91527a0,256,256,true,false,false}; }
HudProbe opener(float x,float y) { return {{x,y,x+16,y+8},{0,208,16,216},0x91527a0,256,256,true,false,false}; }
int main() {
 for (const auto &[x,y] : {std::pair{328.f,208.f},std::pair{328.f,200.f},std::pair{312.f,208.f},std::pair{312.f,200.f}}) {
  HudLayout h;
  assert(!h.classify(1,1,opener(x,y)));
  assert(h.classify(1,1,clock_probe()).group==HudGroup::TopLeft);
  auto a=h.classify(1,1,opener(x,y));
  assert(a.group==HudGroup::Items && a.x==472 && a.y==260);
  // A known start-menu panel opener and a wide menu fill remain unscaled.
  assert(!h.classify(1,1,{{360,168,376,176},{0,232,16,240},0x91527a0,256,256,true,false,false}));
  assert(!h.classify(1,1,{{232,200,456,208},{0,0,224,8},0x915f820,16,8,true,false,false}));
  assert(!h.classify(2,1,{{400,240,416,256},{0,0,16,16},0x91527a0,256,256,true,false,false}));
 }
 HudLayout held;
 held.classify(1,1,clock_probe()); held.classify(1,1,opener(328,200));
 auto cap=held.classify(1,1,{{456,256,472,264},{32,224,48,232},0x91527a0,256,256,true,false,false});
 assert(cap.group==HudGroup::Items);
 assert(cap.apply_x(472,.5f)==472 && cap.apply_y(260,.5f)==260);
 // A different framebuffer cannot inherit selector recognition.
 assert(!held.classify(1,2,{{456,256,472,264},{32,224,48,232},0x91527a0,256,256,true,false,false}));
 HudLayout stock;
 stock.classify(1,1,clock_probe()); stock.classify(1,1,opener(328,208));
 assert(!stock.classify(1,1,{{312,224,328,232},{0,224,16,232},0x91527a0,256,256,true,false,false}));
 HudLayout h; h.classify(1,1,clock_probe()); h.classify(1,1,opener(312,200));
 assert(h.classify(1,1,{{312.0f,208.0f,328.0f,216.0f},{0.0f,208.0f,16.0f,216.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{328.0f,208.0f,456.0f,216.0f},{0.0f,0.0f,128.0f,8.0f},0x915f820,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{456.0f,208.0f,472.0f,216.0f},{32.0f,208.0f,48.0f,216.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{312.0f,216.0f,328.0f,224.0f},{0.0f,0.0f,16.0f,8.0f},0x915ffa0,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{456.0f,216.0f,472.0f,224.0f},{0.0f,0.0f,16.0f,8.0f},0x91600a0,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{328.0f,216.0f,456.0f,224.0f},{0.0f,0.0f,128.0f,8.0f},0x9160020,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{312.0f,224.0f,328.0f,232.0f},{0.0f,224.0f,16.0f,232.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{328.0f,224.0f,456.0f,232.0f},{0.0f,0.0f,128.0f,8.0f},0x9160820,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{456.0f,224.0f,472.0f,232.0f},{32.0f,224.0f,48.0f,232.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{320.0f,240.0f,330.0f,260.0f},{192.0f,52.0f,202.0f,72.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{330.0f,240.0f,454.0f,260.0f},{202.0f,52.0f,206.0f,72.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{454.0f,240.0f,464.0f,260.0f},{206.0f,52.0f,216.0f,72.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{372.0f,200.0f,412.0f,240.0f},{152.0f,32.0f,192.0f,72.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{402.0f,224.0f,426.0f,240.0f},{224.0f,96.0f,248.0f,112.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{358.0f,224.0f,382.0f,240.0f},{224.0f,96.0f,248.0f,112.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{363.0f,224.0f,379.0f,240.0f},{48.0f,0.0f,64.0f,16.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{320.0f,212.0f,340.0f,228.0f},{64.0f,0.0f,84.0f,16.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{312.0f,200.0f,328.0f,208.0f},{0.0f,208.0f,16.0f,216.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{328.0f,200.0f,456.0f,208.0f},{0.0f,0.0f,128.0f,8.0f},0x915f820,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{456.0f,200.0f,472.0f,208.0f},{32.0f,208.0f,48.0f,216.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{312.0f,208.0f,328.0f,256.0f},{0.0f,0.0f,16.0f,48.0f},0x915ffa0,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{456.0f,208.0f,472.0f,256.0f},{0.0f,0.0f,16.0f,48.0f},0x91600a0,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{328.0f,208.0f,456.0f,256.0f},{0.0f,0.0f,128.0f,48.0f},0x9160020,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{312.0f,256.0f,328.0f,264.0f},{0.0f,224.0f,16.0f,232.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{328.0f,256.0f,456.0f,264.0f},{0.0f,0.0f,128.0f,8.0f},0x9160820,16,8,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{456.0f,256.0f,472.0f,264.0f},{32.0f,224.0f,48.0f,232.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{324.0f,200.0f,356.0f,232.0f},{220.0f,224.0f,252.0f,256.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{428.0f,200.0f,460.0f,232.0f},{220.0f,224.0f,252.0f,256.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{348.0f,200.0f,380.0f,232.0f},{220.0f,224.0f,252.0f,256.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{404.0f,200.0f,436.0f,232.0f},{220.0f,224.0f,252.0f,256.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{312.0f,224.0f,328.0f,240.0f},{48.0f,0.0f,64.0f,16.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{456.0f,224.0f,472.0f,240.0f},{0.0f,0.0f,16.0f,16.0f},0x91527a0,256,256,true,false,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{322.0f,209.0f,329.0f,223.0f},{160.0f,22.0f,180.0f,44.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{329.0f,209.0f,336.0f,223.0f},{120.0f,0.0f,140.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{336.0f,209.0f,343.0f,223.0f},{200.0f,22.0f,220.0f,44.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{350.0f,209.0f,357.0f,223.0f},{60.0f,0.0f,80.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{357.0f,209.0f,364.0f,223.0f},{20.0f,0.0f,40.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{364.0f,209.0f,371.0f,223.0f},{40.0f,0.0f,60.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{371.0f,209.0f,378.0f,223.0f},{60.0f,0.0f,80.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{378.0f,209.0f,385.0f,223.0f},{80.0f,0.0f,100.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{392.0f,209.0f,399.0f,223.0f},{60.0f,0.0f,80.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{399.0f,209.0f,406.0f,223.0f},{80.0f,22.0f,100.0f,44.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{406.0f,209.0f,413.0f,223.0f},{200.0f,0.0f,220.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{413.0f,209.0f,420.0f,223.0f},{200.0f,0.0f,220.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{420.0f,209.0f,427.0f,223.0f},{40.0f,0.0f,60.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{427.0f,209.0f,434.0f,223.0f},{120.0f,0.0f,140.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{434.0f,209.0f,441.0f,223.0f},{40.0f,22.0f,60.0f,44.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{315.0f,225.0f,322.0f,239.0f},{20.0f,22.0f,40.0f,44.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{322.0f,225.0f,329.0f,239.0f},{100.0f,22.0f,120.0f,44.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{318.0f,200.0f,325.0f,214.0f},{60.0f,88.0f,80.0f,110.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{325.0f,200.0f,332.0f,214.0f},{40.0f,0.0f,60.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{332.0f,200.0f,339.0f,214.0f},{180.0f,22.0f,200.0f,44.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{346.0f,200.0f,360.0f,214.0f},{160.0f,110.0f,180.0f,132.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{360.0f,200.0f,367.0f,214.0f},{80.0f,132.0f,100.0f,154.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{367.0f,200.0f,374.0f,214.0f},{20.0f,44.0f,40.0f,66.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{374.0f,200.0f,381.0f,214.0f},{180.0f,0.0f,200.0f,22.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 assert(h.classify(1,1,{{381.0f,200.0f,388.0f,214.0f},{100.0f,132.0f,120.0f,154.0f},0x40d4000,256,256,true,true,false}).group==HudGroup::Items);
 std::cout << "HUD item layout tests passed; 57 captured quest item signatures replayed\n";
}
