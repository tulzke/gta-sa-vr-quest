#pragma once
namespace savr::vehicleclimb {
struct Profile {int model;float pipeX;};
// Retail DFF front-door/wheel positions: door height >=1.5 m above
// estimated wheel contact. Excludes automatic/special doors and ladders.
inline const Profile* FindProfile(int model){
    static constexpr Profile profiles[]={
        {403,1.659f}, // linerun: door height ~2.16 m
        {428,1.882f}, // securica: door height ~1.57 m
        {433,1.830f}, // barracks: door height ~1.90 m
        {443,1.763f}, // packer: door height ~2.16 m
        {444,1.938f}, // monster: door height ~2.00 m
        {455,1.830f}, // flatbed: door height ~1.90 m
        {514,1.739f}, // petro: door height ~2.06 m
        {515,1.962f}, // rdtrain: door height ~2.20 m
        {524,1.819f}, // cement: door height ~1.80 m
        {556,1.938f}, // monstera: door height ~2.01 m
        {557,1.953f}, // monsterb: door height ~1.88 m
        {573,1.803f}, // duneride: door height ~1.88 m
    };
    for(const auto& p:profiles)if(p.model==model)return &p;
    return nullptr;
}
}
