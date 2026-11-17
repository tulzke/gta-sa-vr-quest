#define _CRT_SECURE_NO_WARNINGS
#include "WheelHaptics.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
using namespace savr;
// SETTINGS_DECLARATIONS
FILE* file{};
void EnsureInit() {}
void ResetInteraction() {}
void Save() {
    if(!file)return;
    // SETTINGS_SAVE
}
void Load() {
    char line[128];
    while(std::fgets(line,sizeof(line),file)) {
        // SETTINGS_LOAD
    }
    // SETTINGS_NORMALIZE
}
void Read(const char* text) {
    file=std::tmpfile();CHECK(file);std::fputs(text,file);std::rewind(file);
    Load();std::fclose(file);file=nullptr;
}
// SETTINGS_ACTIONS
int main() {
    Read("WheelHintFrequency=1\n");
    CHECK(GetWheelHapticFrequency()==75);
    CHECK(GetWheelHapticSetting(7)==1&&GetWheelHapticSetting(8)==40&&GetWheelHapticSetting(9)==200);
    Read("WheelHintFrequencyRamp=0\nWheelHintFrequencyLowHz=55\nWheelHintFrequencyHighHz=175\nWheelHintFreeCm=0\n");
    CHECK(GetWheelHapticSetting(7)==0&&GetWheelHapticSetting(8)==55&&GetWheelHapticSetting(9)==175);
    CHECK(GetWheelHapticSetting(1)==0);
    int saved[kWheelHapticSettingCount];for(int i=0;i<kWheelHapticSettingCount;++i)saved[i]=GetWheelHapticSetting(i);
    file=std::tmpfile();CHECK(file);Save();std::rewind(file);
    for(auto& v:g_wheelHapticSettings)v=0;
    Load();std::fclose(file);file=nullptr;
    for(int i=0;i<kWheelHapticSettingCount;++i)CHECK(GetWheelHapticSetting(i)==saved[i]);
    Read("WheelHintFrequencyLowHz=500\nWheelHintFrequencyHighHz=-1\n");
    CHECK(GetWheelHapticSetting(8)==20&&GetWheelHapticSetting(9)==200);
    Read("WheelHintFrequencyLowHz=75\nWheelHintFrequencyHighHz=75\n");
    AdjustWheelHapticSetting(8,1);CHECK(GetWheelHapticSetting(8)==80&&GetWheelHapticSetting(9)==80);
    AdjustWheelHapticSetting(9,-1);CHECK(GetWheelHapticSetting(8)==75&&GetWheelHapticSetting(9)==75);
    AdjustWheelHapticSetting(7,1);CHECK(GetWheelHapticSetting(7)==1);
    for(int i=0;i<100;++i)AdjustWheelHapticSetting(8,1);
    CHECK(GetWheelHapticSetting(8)==200&&GetWheelHapticSetting(9)==200);
    for(int i=0;i<100;++i)AdjustWheelHapticSetting(9,-1);
    CHECK(GetWheelHapticSetting(8)==20&&GetWheelHapticSetting(9)==20);
    std::puts("production haptic settings: old configs, new range, persistence, clamping and controls: PASS");
}
