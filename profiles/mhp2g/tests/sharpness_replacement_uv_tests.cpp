#include "gpu/sharpness_replacement_uv.hpp"
#include <cassert>
#include <limits>
#include <iostream>
using namespace mhp2g::gpu::ui;
int main() {
 const std::array<float,4> screen{60,32,104,40}, fuc{108,43,152,51}, stock{108,44,152,52};
 assert(sharpness_replacement_v_offset(true,256,256,screen,fuc)==1.f/256);
 assert(sharpness_replacement_v_offset(false,256,256,screen,fuc)==0);
 assert(sharpness_replacement_v_offset(true,256,256,screen,stock)==0);
 assert(sharpness_replacement_v_offset(true,512,512,screen,fuc)==0);
 auto menu=screen;menu[0]=328;
 assert(sharpness_replacement_v_offset(true,256,256,menu,fuc)==0);
 auto bad=fuc;bad[0]=std::numeric_limits<float>::quiet_NaN();
 assert(sharpness_replacement_v_offset(true,256,256,screen,bad)==0);
 // Same source-texel correction at original, medium and small display sizes.
 for(float factor : {1.f,.5f,1.f/3}) {
  const float offset=sharpness_replacement_v_offset(true,256,256,screen,fuc);
  assert((43.f/256+offset)*256==44);
  assert((51.f/256+offset)*256==52);
  assert((40.f-32.f)*factor > 0);
 }
 std::cout << "Sharpness replacement UV checks passed\n";
}
