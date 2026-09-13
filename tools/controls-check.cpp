#include "mgs5vr/controls.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int wmain(int argc,wchar_t** argv){
    if(argc==2&&std::wstring_view(argv[1])==L"--list"){
        for(const auto& entry:mgs5vr::controlDefinitions())std::cout<<entry.name<<" = "<<entry.binding<<'\n';return 0;
    }
    if(argc!=3||std::wstring_view(argv[1])!=L"--check"){
        std::cerr<<"Usage: mgs5vr_controls --check mgs5vr-controls.ini\n       mgs5vr_controls --list\n";return 2;
    }
    std::ifstream file{std::filesystem::path(argv[2])};
    if(!file){std::cerr<<"Cannot open controls file.\n";return 2;}
    mgs5vr::ControlBindings controls;const auto errors=controls.load(file);
    for(const auto& error:errors)std::cerr<<error<<'\n';
    if(!errors.empty())return 1;
    std::cout<<"Controls valid. Save beside the game DLL, release all buttons/grips/triggers and center both sticks for two seconds to apply live.\n";return 0;
}
