// Synthetic tests only; optional argv[1] checks a player's own KTX2 file.
#include "texture_pack_ktx.hpp"
#include "texture_pack_quick.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#if defined(MHP2G_HAS_KTX2) && MHP2G_HAS_KTX2
#include <ktx.h>
#endif

using namespace mhp2g::gpu;
namespace {
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
constexpr std::array<std::uint8_t, 12> magic{0xAB,0x4B,0x54,0x58,0x20,0x32,0x30,0xBB,0x0D,0x0A,0x1A,0x0A};

#if defined(MHP2G_HAS_KTX2) && MHP2G_HAS_KTX2
void put(std::vector<std::uint8_t> &b, std::size_t at, std::uint64_t value, unsigned count = 4) {
    for (unsigned i = 0; i < count; ++i) b.at(at + i) = static_cast<std::uint8_t>(value >> (i * 8u));
}
std::vector<std::uint8_t> synthetic_rgba(bool flip_y = false, bool premultiplied = false) {
    // Public libktx API supplies a standard RGBA8 DFD; serialize only our
    // own 2x2 pixels. No KTX writer library or third-party artwork is needed.
    ktxTextureCreateInfo info{};
    info.vkFormat=37;info.baseWidth=2;info.baseHeight=2;info.baseDepth=1;
    info.numDimensions=2;info.numLevels=1;info.numLayers=1;info.numFaces=1;
    ktxTexture2 *texture=nullptr;
    require(ktxTexture2_Create(&info,KTX_TEXTURE_CREATE_NO_STORAGE,&texture)==KTX_SUCCESS,"create synthetic DFD");
    const auto length=texture->pDfd[0];
    std::vector<std::uint8_t> dfd(reinterpret_cast<std::uint8_t*>(texture->pDfd),
                                reinterpret_cast<std::uint8_t*>(texture->pDfd)+length);
    ktxTexture_Destroy(ktxTexture(texture));
    if (premultiplied) dfd.at(15)|=1u;
    std::vector<std::uint8_t> kv;
    if (flip_y) {
        const char value[]="KTXorientation\0ru";
        kv.resize((4u+sizeof(value)+3u)&~3u);
        put(kv,0,sizeof(value));std::memcpy(kv.data()+4,value,sizeof(value));
    }
    const std::size_t kv_at=104u+dfd.size();
    const std::size_t data_at=(kv_at+kv.size()+7u)&~std::size_t(7u);
    std::vector<std::uint8_t> b(data_at+16u);
    std::copy(magic.begin(),magic.end(),b.begin());
    put(b,12,37);put(b,16,1);put(b,20,2);put(b,24,2);put(b,36,1);put(b,40,1);
    put(b,48,104);put(b,52,dfd.size());
    if(!kv.empty()){put(b,56,kv_at);put(b,60,kv.size());std::copy(kv.begin(),kv.end(),b.begin()+kv_at);}
    put(b,80,data_at,8);put(b,88,16,8);put(b,96,16,8);
    std::copy(dfd.begin(),dfd.end(),b.begin()+104);
    const std::array<std::uint8_t,16> pixels{64,32,16,128, 0,255,0,255, 0,0,255,255, 0,0,0,0};
    std::copy(pixels.begin(),pixels.end(),b.begin()+data_at);
    return b;
}
#endif
}

int main(int argc,char **argv) {
    try {
        std::array<std::uint8_t,256> bytes{};
        // Fixed vectors independently checked against the format reference.
        require(texture_pack_quick(bytes.data(),0)==0x9535599Cu,"quick empty aligned");
        require(texture_pack_quick(bytes.data(),64)==0x2689EAF0u,"quick zero block");
        for(unsigned i=0;i<bytes.size();++i)bytes[i]=static_cast<std::uint8_t>(i);
        require(texture_pack_quick(bytes.data(),64)==0xDB8BE980u,"quick ramp64");
        require(texture_pack_quick(bytes.data(),128)==0xD70047E4u,"quick ramp128");
        require(texture_pack_quick(bytes.data(),32)==0u,"quick short fallback");
        require(texture_pack_quick(bytes.data(),31)==texture_pack_quick(bytes.data(),24),"quick ignored tail");
        std::array<std::uint8_t,257> shifted{};
        std::copy(bytes.begin(),bytes.end(),shifted.begin()+1);
        require(texture_pack_quick(shifted.data()+1,64,0)==texture_pack_quick(bytes.data(),64,0),
                "quick uses guest, not host alignment");
        KtxPackImage image;std::string error;
        require(!decode_ktx2_image({},image,error),"reject empty image");
        require(is_ktx2_image(magic),"KTX2 magic");
#if defined(MHP2G_HAS_KTX2) && MHP2G_HAS_KTX2
        const auto decode = [&](const std::vector<std::uint8_t> &encoded) {
            const bool ok = decode_ktx2_image(encoded, image, error);
            require(ok, error.c_str());
        };
        auto valid=synthetic_rgba();
        decode(valid);
        require(image.width==2&&image.height==2&&image.pixels.size()==16,"RGBA dimensions");
        require(image.pixels[0]==64&&image.pixels[3]==128&&image.pixels[15]==0,"RGBA and alpha preserved");
        auto flipped=synthetic_rgba(true);
        decode(flipped);
        require(image.pixels[2]==255&&image.pixels[3]==255,"orientation converted to top-left");
        auto premultiplied=synthetic_rgba(false,true);
        decode(premultiplied);
        require(image.pixels[0]==128&&image.pixels[1]==64&&image.pixels[2]==32,"straight alpha conversion");
        auto truncated=valid;truncated.resize(100);
        require(!decode_ktx2_image(truncated,image,error)&&image.pixels.empty(),"reject truncated file");
        auto oversized=valid;put(oversized,20,16384);
        require(!decode_ktx2_image(oversized,image,error),"reject oversized image");
        auto array=valid;put(array,32,2);
        require(!decode_ktx2_image(array,image,error),"reject arrays");
        if(argc>1){
            std::ifstream in(argv[1],std::ios::binary);
            require(bool(in),"cannot open sample KTX2");
            const std::vector<std::uint8_t> sample((std::istreambuf_iterator<char>(in)),{});
            decode(sample);
            std::cout<<"sample: "<<image.width<<"x"<<image.height<<", "<<image.pixels.size()<<" RGBA bytes\n";
        }
#else
        (void)argc;(void)argv;
        require(!decode_ktx2_image(magic,image,error)&&error.find("disabled")!=std::string::npos,"disabled decoder message");
#endif
        std::cout<<"texture pack codec tests passed\n";
        return 0;
    } catch(const std::exception &e) {
        std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;
    }
}
