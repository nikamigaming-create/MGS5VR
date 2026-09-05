#include "mgs5vr/xr_runtime.hpp"
#include "mgs5vr/log.hpp"
#include <iostream>
#include <string>

static std::string json(const std::string& s){
    std::string out="\"";
    for(unsigned char c:s){
        if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}
        else if(c=='\n')out+="\\n";
        else if(c=='\r')out+="\\r";
        else if(c=='\t')out+="\\t";
        else if(c<32)out+='?';
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
int main(int argc,char** argv){
    using namespace mgs5vr;
    int seconds=0;
    if(argc==3&&std::string(argv[1])=="--session-seconds"){
        try{size_t used{};seconds=std::stoi(argv[2],&used);if(used!=std::string(argv[2]).size())throw std::invalid_argument("duration");}
        catch(...){std::cerr<<"Invalid duration\n";return 64;}
        if(seconds<1||seconds>60){std::cerr<<"Duration must be 1..60 seconds\n";return 64;}
    }else if(argc!=1){std::cerr<<"Usage: mgs5vr_probe [--session-seconds 1..60]\n";return 64;}
    const auto p=probeRuntime();
    std::cout<<"{\"instance_available\":"<<(p.instanceAvailable?"true":"false")
        <<",\"headset_available\":"<<(p.headsetAvailable?"true":"false")
        <<",\"runtime\":"<<json(p.runtime)<<",\"system\":"<<json(p.system)<<",\"error\":"<<json(p.error)<<"}\n";
    if(!p.headsetAvailable)return 2;
    if(seconds)try{
        TextureMailbox mailbox;std::atomic_bool stop{false};
        const auto stats=runTheatre(mailbox,{},stop,std::chrono::seconds(seconds));
        std::cout<<"{\"xr_frames\":"<<stats.frames<<",\"screen_submissions\":"<<stats.submittedScreens<<",\"note\":\"Session-only probe, no game pixels\"}\n";
        return stats.frames?0:3;
    }catch(const std::exception& e){std::cout<<"{\"session_error\":"<<json(e.what())<<"}\n";return 4;}
    return 0;
}
