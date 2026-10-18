#include "camera/fov.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <cstdlib>
using namespace mhp2g::camera::fov;
void require(bool ok) { if (!ok) { std::cerr << "FOV check failed\n"; std::exit(1); } }
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
Matrix perspective(float degrees) {
    Matrix p{}; p[5]=1/std::tan(degrees*kDegreesToRadians/2); p[0]=p[5]/(480.f/272.f);
    p[10]=-1.02f; p[11]=-1; p[14]=-2.02f; return p;
}
Matrix multiply(const Matrix&a,const Matrix&b) {
    Matrix r{}; for(int c=0;c<4;++c) for(int row=0;row<4;++row)
        for(int k=0;k<4;++k) r[c*4+row]+=a[k*4+row]*b[c*4+k];
    return r;
}
int main() {
    Cache cache; auto p=perspective(50); auto a=analyze(p,10);
    require(a.valid && near(a.native_degrees,50) && near(a.adjusted_degrees,60) && a.scale<1);
    require(analyze(p,-10).scale>1);
    Matrix world{1,2,3,0, 4,5,6,0, 7,8,9,0, 11,12,13,1};
    p[8]=.12f; p[9]=-.17f;
    auto native=multiply(p,world); auto result=cache.apply(native,p,10);
    auto edited=p; auto adjustment=analyze(p,10); edited[0]*=adjustment.scale; edited[5]*=adjustment.scale;
    auto expected=multiply(edited,world);
    for(int i=0;i<16;++i) require(near(result[i],expected[i]));
    for(int c=0;c<4;++c) { require(result[c*4+2]==native[c*4+2]); require(result[c*4+3]==native[c*4+3]); }
    require(cache.apply(native,p,10)==result);
    require(cache.apply(native,p,-10)!=result);
    auto original=cache.apply(native,p,0); require(std::memcmp(original.data(),native.data(),sizeof(native))==0);
    auto invalid=p; invalid[15]=1; require(cache.apply(native,invalid,10)==native);
    invalid=p; invalid[11]=0; require(cache.apply(native,invalid,10)==native);
    invalid=p; invalid[5]=0; require(cache.apply(native,invalid,10)==native);
    invalid=p; invalid[4]=.1f; require(cache.apply(native,invalid,10)==native);
    invalid=p; invalid[0]=std::numeric_limits<float>::quiet_NaN(); require(cache.apply(native,invalid,10)==native);
    original=cache.apply(invalid,invalid,0); require(std::memcmp(original.data(),invalid.data(),sizeof(invalid))==0);
    require(cache.apply(native,p,7)==native);
    require(near(analyze(perspective(10),-15).adjusted_degrees,5));
    require(near(analyze(perspective(115),30).adjusted_degrees,120));
    auto other=perspective(70); require(cache.apply(other,other,10)!=cache.apply(p,p,10));
    std::int32_t value=99;
    for(auto text : {"", "7", "10x", "999999999999999", "-20"}) { require(!parse_offset(text,value)); require(value==99); }
    require(parse_offset("-15",value)&&value==-15); require(parse_offset("30",value)&&value==30); require(parse_offset("0",value)&&value==0);
    std::cout << "FOV tests passed\n";
}
