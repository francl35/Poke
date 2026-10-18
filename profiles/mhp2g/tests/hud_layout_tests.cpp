#include "gpu/hud_layout.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mhp2g::gpu::ui;
namespace {
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
HudProbe sheet(HudRect screen, HudRect uv) { return {screen, uv, 0x091527A0u, 256, 256, true, false, false}; }
HudProbe clock_draw() { return sheet({8,8,60,56}, {0,32,52,80}); }
bool near(float a, float b) { return std::abs(a-b)<0.001f; }
}
int main() {
    try {
        HudLayout layout;
        const unsigned target=0x04000000u;
        // Texture reuse on title/menu screens must not establish a quest HUD.
        require(!layout.classify(1,target,sheet({132,10,156,34},{96,160,120,184})),"menu icon stays original");
        require(!layout.classify(1,target,sheet({328,208,344,216},{0,208,16,216})),"item-like panel without quest clock stays original");
        auto top=layout.classify(2,target,clock_draw());
        require(top.group==HudGroup::TopLeft,"clock establishes quest group");
        auto cap=layout.classify(2,target,sheet({320,9,344,21},{52,32,76,44}));
        require(cap.group==HudGroup::TopLeft,"long health cap stays with left group");
        HudProbe fill{{56,17,320,19},{0,0,0,0},0,0,0,false,false,false};
        require(layout.classify(2,target,fill).group==HudGroup::TopLeft,"untextured fill follows its frame");
        HudProbe glyph{{26,58,34,72},{0,0,20,22},0x040D4000u,256,256,true,true,false};
        require(layout.classify(2,target,glyph).group==HudGroup::TopLeft,"queued name glyph follows its plate");
        require(near(top.apply_x(8,0.5f),8)&&near(top.apply_y(8,1.0f/3.0f),8),"top-left margins stay fixed");
        require(near(top.apply_x(344,0.5f),176),"long bar halves around left anchor");
        HudProbe map{{316,32,468,184},{0,0,150,150},0x0419C200u,256,256,true,false,true};
        auto ma=layout.classify(2,target,map);
        require(ma.group==HudGroup::Map&&near(ma.apply_x(468,0.5f),468)&&near(ma.apply_y(32,0.5f),32),"expanded map margins stay fixed");
        auto marker=layout.classify(2,target,sheet({433,152,445,164},{48,244,60,256}));
        require(marker.group==HudGroup::Map&&marker.x==ma.x&&marker.y==ma.y,"map marker shares map anchor");
        layout.classify(3,target,clock_draw());
        map.screen={430,32,468,70};
        ma=layout.classify(3,target,map);
        require(ma.group==HudGroup::Map&&ma.x==468&&ma.y==32,"collapsed map uses its own bounds");
        auto item=layout.classify(3,target,sheet({328,208,344,216},{0,208,16,216}));
        require(item.group==HudGroup::Items,"item panel establishes item group");
        auto button=layout.classify(3,target,sheet({336,212,356,228},{64,0,84,16}));
        require(button.group==HudGroup::Items,"item buttons share item anchor");
        require(near(item.apply_x(472,1.0f/3.0f),472)&&near(item.apply_y(260,0.5f),260),"item margins stay fixed");
        // Start menu frame from the captured trace: HUD remains at the top,
        // while right-hand menu panes are deliberately not resized.
        layout.classify(4,target,clock_draw());
        require(!layout.classify(4,target,sheet({344,40,360,48},{0,232,16,240})),"start menu corner stays original");
        require(!layout.classify(4,target,sheet({456,208,472,256},{0,0,16,48})),"menu lower edge is not an item panel");
        require(!layout.classify(4,target,sheet({216,200,232,208},{0,208,16,216})),"menu panel does not arm item scaling");
        require(!layout.classify(4,target,sheet({160,120,176,128},{0,208,16,216})),"centered dialog stays original");
        auto invalid=clock_draw();invalid.screen.left=std::numeric_limits<float>::quiet_NaN();
        require(!layout.classify(5,target,invalid),"invalid geometry is ignored");
        require(!layout.classify(5,target,fill),"frame state resets; no stale quest detection");
        layout.classify(6,target,clock_draw());
        require(!layout.classify(6,target+1,fill),"target state resets");
        // Applying either transform order must agree under Fill aspect fit.
        const float f=0.5f,fit=0.75f,p=320,a=8;
        const float lhs=240+(a+(p-a)*f-240)*fit;
        const float fitted_a=240+(a-240)*fit;
        const float rhs=fitted_a+(240+(p-240)*fit-fitted_a)*f;
        require(near(lhs,rhs),"HUD and aspect-fit composition agree");
        require(near(top.apply_x(344,1),344),"original factor is identity");
        layout.classify(40,target,clock_draw());
        require(!layout.classify(40,target,sheet({232,40,248,48},{0,208,16,216})),
            "equipment panel top-left corner is not HUD");
        require(!layout.classify(40,target,sheet({272,40,288,48},{32,208,48,216})),
            "equipment panel top-right corner is not HUD");
        require(!layout.classify(40,target,sheet({24,40,40,48},{0,232,16,240})),
            "alternate menu corner is not HUD");
        require(layout.classify(40,target,sheet({60,32,104,40},{108,44,152,52})).group==HudGroup::TopLeft,
            "sharpness still belongs to HUD");
        require(layout.classify(40,target,sheet({8,56,24,72},{120,16,136,32})).group==HudGroup::TopLeft,
            "name-plate corner still belongs to HUD");
        std::cout<<"HUD layout tests passed\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
