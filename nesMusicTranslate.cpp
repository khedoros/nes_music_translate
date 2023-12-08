#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<array>
#include<cstdint>
#include<string>
#include<format>

struct event_t {
    uint32_t timestamp;
    uint16_t reg;
    uint16_t data;
};

// Map of length values to ticks clocked at about 96Hz
lengths[32] = {10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

int main(int argc, char* argv[]) {
    std::vector<event_t> audioEvents;
    std::ifstream in;
    if(argc == 2) {
        in.open(argv[1]);
    }
    else {
        std::cout<<"Provide a filename to open\n";
        return 1;
    }
    if(!in.is_open()) {
        std::cout<<"Couldn't open the file at "<<argv[1]<<"\n";
        return 1;
    }
    std::string inLine;
    while(std::getline(in, inLine)) {
        std::stringstream ss(inLine);
        uint32_t t;
        uint16_t r;
        uint16_t d;
        ss<<std::dec;
        ss>>t;
        ss<<std::hex;
        ss>>r;
        ss>>d;
        if(r < 0x4010) {
            // std::cout<<inLine<<"\t->\t"<<t<<'\t'<<r<<'\t'<<d<<'\n';
            audioEvents.emplace_back(event_t{t,r,d});
        }
    }
    std::array<uint16_t,32> prevVals;
    prevVals.fill(0);
    for(const auto& a:audioEvents) {
        std::cout<<"Cyc: "<<std::dec<<a.timestamp<<": \tAPU["<<std::hex<<a.reg<<"] = "<<a.data<<'\t';
        if(prevVals[a.reg % 0x20] != a.data) {
            std::cout<<"(Old: "<<prevVals[a.reg % 0x20]<<")";
        } else {
            std::cout<<"(No change)";
        }
        prevVals[a.reg % 0x20] = a.data;
        std::string description{""};
        const float cpuFreq = 1789773.0;
        float freq = 0.0;
        float wavelength = 0.0;
        switch(a.reg) {
            // sq1
            case 0x4000:
                description = std::format(" SQ1: Duty: {} Use envelope: {} Constant volume: {} Volume/Rate: {}", (a.data >> 6), ((a.data >> 5) & 1), ((a.data>>4) & 1), (a.data & 0xf));
                break;
            case 0x4001:
                description = std::format(" SQ1: Sweep enabled: {}", (a.data >> 7));
                if(a.data & 0x80) {
                    description += std::format(" Period: {} Direction: {} Shift: {}", ((a.data>>4)&0x7)+1, ((a.data>>3)&1), (a.data&0x7));
                }
                break;
            case 0x4002: 
                wavelength = ((prevVals[3] & 0x7)<<8) + a.data;
                freq = cpuFreq / (16.0 * (wavelength + 1));
                description = std::format(" SQ1: Timer low bits, period set to {} ({} Hz)", wavelength, freq);
                break;
            case 0x4003: 
                wavelength = ((a.data & 0x7)<<8) + prevVals[2];
                freq = cpuFreq / (16.0 * (wavelength + 1));
                description = std::format(" SQ1: TRIGGER Length: {} period: {} ({} Hz)", (a.data >> 3), wavelength, freq);
                break;
            // sq2
            case 0x4004: 
                description = std::format(" SQ2: Duty: {} Use envelope: {} Constant volume: {} Volume/Rate: {}", (a.data >> 6), ((a.data >> 5) & 1), ((a.data>>4) & 1), (a.data & 0xf));
                break;
            case 0x4005: 
                description = std::format(" SQ2: Sweep enabled: {}", (a.data >> 7));
                if(a.data & 0x80) {
                    description += std::format(" Period: {} Direction: {} Shift: {}", ((a.data>>4)&0x7)+1, ((a.data>>3)&1), (a.data&0x7));
                }
                break;
            case 0x4006: 
                wavelength = ((prevVals[7] & 0x7)<<8) + a.data;
                freq = cpuFreq / (16.0 * (wavelength + 1));
                description = std::format(" SQ2: Timer low bits, period set to {} ({} Hz)", wavelength, freq);
                break;
            case 0x4007: 
                wavelength = ((a.data & 0x7)<<8) + prevVals[6];
                freq = cpuFreq / (16.0 * (wavelength + 1));
                description = std::format(" SQ2: TRIGGER Length: {} period: {} ({} Hz)", (a.data >> 3), wavelength, freq);
                break;
            // tri
            case 0x4008: 
                description = std::format(" TRI: reg0");
                break;
            case 0x400a: 
                description = std::format(" TRI: reg2");
                break;
            case 0x400b: 
                description = std::format(" TRI: reg3");
                break;
            // noise
            case 0x400c: 
                description = std::format(" NOISE: reg0");
                break;
            case 0x400e: 
                description = std::format(" NOISE: reg2");
                break;
            case 0x400f: 
                description = std::format(" NOISE: reg3");
                break;
        }
        std::cout<<description<<'\n';
    }
}
