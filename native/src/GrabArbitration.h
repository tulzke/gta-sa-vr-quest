#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

// GameThread only. Probe every eligible interaction before any fresh grab.
// Distances are metres to the actual contact point/surface, never pool order.
namespace savr::grabarbitration {
enum class Kind { None, Holster, Door, Bike, Ledge, VehicleClimb, Npc, Ball, Weapon, Dropped, Pickup, Count };
struct Candidate {
    Kind kind{Kind::None};
    std::uint64_t id{};
    float distance{std::numeric_limits<float>::infinity()};
};
struct Selection {
    bool enabled{};
    unsigned occupied{};
    Candidate hands[2]{};
    Candidate offers[2][static_cast<int>(Kind::Count)]{};
    void Begin(bool active,unsigned busy=0) {
        enabled=active;occupied=busy;
        for(int h=0;h<2;++h){hands[h]={};for(auto& offer:offers[h])offer={};}
    }
    void Select(int hand) {
        auto& best=hands[hand];best={};
        for(const auto& offer:offers[hand]) {
            if(offer.kind==Kind::None)continue;
            const bool holster=offer.kind==Kind::Holster, previous=best.kind==Kind::Holster;
            if(best.kind==Kind::None || (holster&&!previous) ||
               (holster==previous && (offer.distance<best.distance ||
                (offer.distance==best.distance && (offer.kind<best.kind ||
                 (offer.kind==best.kind&&offer.id<best.id))))))best=offer;
        }
    }
    void Offer(int hand,Kind kind,std::uint64_t id,float distance) {
        if(!enabled||hand<0||hand>1||(occupied&(1u<<hand))||kind==Kind::None||kind>=Kind::Count||!std::isfinite(distance)||distance<0) return;
        auto& best=offers[hand][static_cast<int>(kind)];
        if(best.kind==Kind::None||distance<best.distance||(distance==best.distance&&id<best.id))best={kind,id,distance};
        Select(hand);
    }
    void Reject(int hand,Kind kind) {
        if(hand<0||hand>1||kind<=Kind::None||kind>=Kind::Count)return;
        offers[hand][static_cast<int>(kind)]={};Select(hand);
    }
    bool Allows(int hand,Kind kind) const {
        return !enabled || (hand>=0&&hand<2&&hands[hand].kind==kind);
    }
};
inline Selection current;
inline bool Allows(int hand,Kind kind) { return current.Allows(hand,kind); }
} // namespace savr::grabarbitration
