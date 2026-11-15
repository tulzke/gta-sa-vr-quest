#define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include "../vendor/cgltf/cgltf.h"
#undef CGLTF_IMPLEMENTATION
#include "WatchModel.cpp"
#include "WatchLayout.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main(){
    using namespace savr;
    std::ifstream file(WATCH_FILE,std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    watchmodel::Model model;
    CHECK(model.Load(bytes.data(),bytes.size()));
    CHECK(model.meshes.size()==9&&model.materials.size()==4&&model.images.size()==2);
    std::size_t triangles=0,displays=0;
    for(const auto& mesh:model.meshes){triangles+=mesh.indices.size()/3;displays+=mesh.display;}
    CHECK(triangles==2562&&displays==1);
    CHECK(std::abs(model.centre[0]-.0043426f)<.00001f);
    CHECK(std::abs(model.centre[2]-.0343966f)<.00001f);
    for(std::size_t length:{std::size_t(0),std::size_t(12),std::size_t(100),bytes.size()-4})
        CHECK(!model.Load(bytes.data(),length)&&model.meshes.empty());
    for(float maximum:{100.f,150.f,200.f}) {
        CHECK(watchhud::Fraction(maximum,maximum)==1.f);
        CHECK(watchhud::Fraction(maximum*.5f,maximum)==.5f);
        CHECK(watchhud::Fraction(0,maximum)==0.f);
        CHECK(watchhud::Fraction(maximum*2,maximum)==1.f);
    }
    CHECK(watchhud::Fraction(10,0)<0&&watchhud::Fraction(-1,100)<0);
    CHECK(watchhud::Fraction(std::numeric_limits<float>::quiet_NaN(),100)<0);
    for(int degrees=0;degrees<360;++degrees){
        const float angle=degrees*.01745329252f;
        float x=10000*std::sin(angle),y=10000*std::cos(angle);
        watchhud::Pin(x,y);
        // All corners of an icon's enclosing circle must stay in view.
        const float margin=24.f*1.414214f;
        CHECK(std::hypot(x,y)+margin<=watchhud::kMapRadius*watchhud::kMapPixels+.001f);
        CHECK(y-margin>=watchhud::kMapTop*watchhud::kMapPixels-.001f);
        CHECK(y+margin<=watchhud::kMapBottom*watchhud::kMapPixels+.001f);
    }
    float x=5,y=-10;watchhud::Pin(x,y);CHECK(x==5&&y==-10);
    CHECK(watchhud::LocalOpacity(0,0)==1);
    CHECK(watchhud::LocalOpacity(0,10000)==0&&watchhud::LocalOpacity(10000,0)==0);
    std::puts("PASS watch GLB, hand-relative fit, current-max vitals, clipped-edge mission markers");
}
