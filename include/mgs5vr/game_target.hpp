#pragma once
#include <array>
#include <string_view>

namespace mgs5vr {
struct GameTarget {
    std::string_view id,name,sha256;
    std::wstring_view executable;
    bool nativeAdapter;
};
inline constexpr std::array<GameTarget,2> gameTargets{{
    {"tpp-1.0.15.4","The Phantom Pain",
     "085c2f82d1c963c40b3d2d55786661dfee2b18cbbf388a710c00fa76c5e9bb45",L"mgsvtpp.exe",true},
    {"gz-1.0.0.5","Ground Zeroes",
     "7460d9dba9b6fe34893b5d330aca201983bb1734844f3688acffcc0ab22a1815",L"MgsGroundZeroes.exe",false}
}};
inline const GameTarget* gameTarget(std::string_view sha256) noexcept {
    for(const auto& target:gameTargets)if(target.sha256==sha256)return &target;
    return nullptr;
}
}
