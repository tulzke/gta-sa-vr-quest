// Game-thread PDA overlay. Reads radar data without changing frontend/HUD state.
// Retail ARM64 trace layout: 250 entries, 48 bytes each; see task 33 ABI notes.
void DrawPdaRadarOverlay(void* handle, const GameSymbols::Vec3& player, float heading) {
    struct Rect { float left,bottom,right,top; };
    using Quad = void (*)(void*,float,float,float,float,float,float,float,float,const void*);
    using PoolLookup = void* (*)(int);
    using EntryPosition = void (*)(void*,GameSymbols::Vec3&);
    static auto quad=reinterpret_cast<Quad>(dlsym(handle,"_ZN9CSprite2d4DrawEffffffffRK5CRGBA"));
    static auto* sprites=static_cast<std::uint8_t*>(dlsym(handle,"_ZN6CRadar16RadarBlipSpritesE"));
    static auto* traces=static_cast<std::uint8_t*>(dlsym(handle,"_ZN6CRadar13ms_RadarTraceE"));
    static auto vehicle=reinterpret_cast<PoolLookup>(dlsym(handle,"_ZN6CPools10GetVehicleEi"));
    static auto ped=reinterpret_cast<PoolLookup>(dlsym(handle,"_ZN6CPools6GetPedEi"));
    static auto object=reinterpret_cast<PoolLookup>(dlsym(handle,"_ZN6CPools9GetObjectEi"));
    static auto entryPosition=reinterpret_cast<EntryPosition>(dlsym(handle,"_ZN10CEntryExit33GetPositionRelativeToOutsideWorldER7CVector"));
    using Visible = bool (*)(int,signed char);
    static auto visible=reinterpret_cast<Visible>(dlsym(handle,"_ZN6CRadar15DisplayThisBlipEia"));
    static auto rect=reinterpret_cast<void (*)(const void*,const void*)>(dlsym(handle,"_ZN9CSprite2d8DrawRectERK5CRectRK5CRGBA"));
    using TraceColor = std::uint32_t (*)(std::uint32_t,unsigned char,unsigned char);
    static auto traceColor=reinterpret_cast<TraceColor>(dlsym(handle,"_ZN6CRadar19GetRadarTraceColourEjhh"));
    if (!quad || !sprites || !traces || !rect || !g.CSprite2d_Draw ||
        reinterpret_cast<std::uintptr_t>(traces)!=g.LoadBase+0xa66740u ||
        reinterpret_cast<std::uintptr_t>(quad)!=g.LoadBase+0x5f7ef4u) return;
    constexpr float pixelsPerMetre=400.0f/300.0f;
    const float c=std::cos(heading), s=std::sin(heading);
    const float halfX=hud::PdaPortrait()?188.0f:308.0f;
    const float halfY=hud::PdaPortrait()?308.0f:188.0f;
    auto project=[&](float dx,float dy,float& x,float& y) {
        // Same world-to-source texture mapping as the tiles, then clamp in the
        // FINAL rotated viewport, and transform back into source coordinates.
        const float sx=dx*pixelsPerMetre, sy=-dy*pixelsPerMetre;
        float vx=c*sx-s*sy, vy=s*sx+c*sy;
        const float edge=std::max(1.0f,std::max(std::abs(vx)/halfX,std::abs(vy)/halfY));
        vx/=edge; vy/=edge;
        x=400.0f+c*vx+s*vy; y=400.0f-s*vx+c*vy;
    };
    const unsigned char white[4]={255,255,255,255};
    auto icon=[&](int id,float x,float y,float angle,float radius) {
        if (id<0 || id>=64 || !*reinterpret_cast<void**>(sprites+id*8)) return;
        const float ca=std::cos(angle), sa=std::sin(angle);
        auto corner=[&](float a,float b) { return std::pair<float,float>{x+ca*a+sa*b,y-sa*a+ca*b}; };
        const auto tl=corner(-radius,-radius), tr=corner(radius,-radius),
                   bl=corner(-radius,radius), br=corner(radius,radius);
        // Native overload takes bottom-left, bottom-right, top-left, top-right.
        quad(sprites+id*8,bl.first,bl.second,br.first,br.second,
             tl.first,tl.second,tr.first,tr.second,white);
    };
    for (int i=0;i<250;++i) {
        const auto* t=traces+i*48;
        std::uint16_t flags; std::memcpy(&flags,t+0x29,2);
        if (!(flags&2) || !(flags&0x200) || ((flags>>8)&3)<2) continue;
        const int type=(flags>>10)&15, sprite=t[0x28];
        if (visible && !visible(sprite,-99)) continue;
        GameSymbols::Vec3 at{};
        std::memcpy(&at,t+8,sizeof(at));
        if (type==1 || type==2 || type==3) {
            int ref; std::memcpy(&ref,t+4,4);
            void* entity=type==1?(vehicle?vehicle(ref):nullptr):type==2?(ped?ped(ref):nullptr):(object?object(ref):nullptr);
            if (!entity) continue;
            void* matrix=nullptr; std::memcpy(&matrix,static_cast<const char*>(entity)+0x18,sizeof(matrix));
            std::memcpy(&at,static_cast<const char*>(matrix?matrix:entity)+(matrix?0x30:8),sizeof(at));
        } else if (type==4 || type==5) {
            void* entry=nullptr; std::memcpy(&entry,t+0x20,sizeof(entry));
            if (entry && entryPosition) entryPosition(entry,at);
        } else continue;
        if (!std::isfinite(at.x)||!std::isfinite(at.y)||!std::isfinite(at.z)) continue;
        float x,y; project(at.x-player.x,at.y-player.y,x,y);
        if (sprite) icon(sprite,x,y,heading,12.0f);
        else {
            // Stock radar primitives have a black border and a coloured core.
            std::uint32_t color; std::memcpy(&color,t,4);
            if (traceColor) color=traceColor(color,1,0);
            unsigned char rgba[4];
            rgba[0]=color>>24;rgba[1]=color>>16;rgba[2]=color>>8;rgba[3]=255;
            const unsigned char black[4]={0,0,0,255};
            const Rect border{x-8,y+8,x+8,y-8}, core{x-5,y+5,x+5,y-5};
            rect(&border,black); rect(&core,rgba);
        }
    }
    float nx,ny; project(0,100000.0f,nx,ny);
    icon(4,nx,ny,heading,12.0f); // stock north sprite
    float playerHeading=g.FindPlayerHeading?g.FindPlayerHeading(0):heading;
    if (!std::isfinite(playerHeading)) playerHeading=heading;
    icon(2,400,400,playerHeading,14.0f); // stock player arrow
}
