#pragma once
namespace savr::vehicleweapon {
constexpr bool Allowed(int type,bool passenger) {
 if(passenger)return type>=22&&type<=34;
 return type==22||type==23||type==24||type==26||type==28||type==32;
}
}
