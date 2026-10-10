#pragma once

#include "idroid_rig.hpp"
#include <span>

namespace mgs5vr {
// Retail p31_010010, ending left-hand hold. Handset resource
// 84a3472eb6970ed4; LHAND 3e2d77e5. Numeric contact measurements,
// not a mirrored right-hand equipment socket. Private source receipt:
// complete-left-hold/contact-frame-000240.json, native sample 2307.
constexpr Pose nativeIdroidLeftBodyInWrist{{0.668181504f,0.212643244f,-0.189869347f,0.687216094f},{0.066569098f,-0.021187740f,0.004362963f}};
struct IdroidHandJoint { uint16_t bone,parent; uint32_t name; Pose local; };
constexpr std::array<IdroidHandJoint,16> nativeIdroidLeftHand{{
    {21,8,0xb8bfcac4u,{{0.055208994f,-0.373867145f,0.089136226f,0.921536792f},{0.007999974f,-0.008999981f,0.025999973f}}},
    {22,21,0x9bfce3b5u,{{-0.007230178f,-0.043474020f,-0.044677676f,0.998028877f},{0.042999997f,-0.018999952f,0.007999983f}}},
    {23,22,0x23d7e167u,{{0.028273800f,0.150681299f,0.155964281f,0.975792438f},{0.029999932f,-0.010999928f,-0.000000011f}}},
    {24,8,0x16288b3bu,{{-0.001673245f,-0.078786243f,-0.085799283f,0.993191025f},{0.087000008f,0.017999999f,0.029000013f}}},
    {25,24,0x38fc8d2bu,{{0.000000780f,-0.004243394f,-0.106088328f,0.994347656f},{0.043000054f,-0.000000034f,0.000000014f}}},
    {26,25,0xd42c8c47u,{{-0.000000128f,-0.013026575f,-0.325675571f,0.945391840f},{0.027000028f,0.000000010f,-0.000000001f}}},
    {27,8,0x326d2a2u,{{-0.000000056f,-0.000000008f,-0.106394262f,0.994324022f},{0.089999926f,0.018000029f,0.005999985f}}},
    {28,27,0xba4bbe54u,{{0.000001929f,-0.009997962f,-0.249956000f,0.968205577f},{0.045999996f,0.000000037f,-0.000000013f}}},
    {29,28,0xe880c7cau,{{-0.000000094f,-0.013147069f,-0.328686976f,0.944347408f},{0.027000075f,0.000000021f,0.000000022f}}},
    {30,8,0x837de69eu,{{0.000000000f,-0.000000000f,-0.000000000f,1.000000000f},{0.024999997f,0.012000007f,-0.021000000f}}},
    {31,30,0x922737feu,{{0.001713497f,0.006586233f,-0.251715021f,0.967777471f},{0.060000002f,0.005000019f,0.004999979f}}},
    {32,31,0x9f2805d5u,{{0.000002107f,-0.009128772f,-0.276400243f,0.960999256f},{0.045999992f,-0.000000015f,-0.000000003f}}},
    {33,32,0x43f0c165u,{{-0.000000044f,-0.016331009f,-0.494411382f,0.869074613f},{0.026000065f,0.000000005f,0.000000039f}}},
    {34,30,0x83c7c2bbu,{{0.001169784f,0.007385432f,-0.397220321f,0.917692815f},{0.052999954f,0.000000027f,-0.016000008f}}},
    {35,34,0x5aac0a03u,{{0.000003416f,-0.014792989f,-0.447898057f,0.893962246f},{0.034000061f,0.000000029f,0.000000040f}}},
    {36,35,0x1104e6d8u,{{-0.000000098f,-0.010274543f,-0.311050419f,0.950337872f},{0.023999974f,0.000000006f,-0.000000003f}}},
}};
constexpr bool matchesNativeIdroidLeftHand(std::span<const uint32_t> names,
                                         std::span<const int32_t> parents) noexcept {
    if(names.size()!=parents.size()||names.size()<=36||names[8]!=0x3e2d77e5u)return false;
    for(const auto& joint:nativeIdroidLeftHand)
        if(names[joint.bone]!=joint.name||parents[joint.bone]!=joint.parent)return false;
    return true;
}
// Convert the tracked anatomical palm to the wrist of this authored hold.
// These knuckle positions come from the same native contact sample.
std::optional<Pose> nativeIdroidLeftWristFromGrip() noexcept;
// Place that same native cup at the tracked palm, with the device facing along
// the controller's pointing basis. Runtime grip cant is not a handset axis.
std::optional<Pose> nativeIdroidLeftHandheldWrist(Pose grip,Pose aim,Pose fit={}) noexcept;
}
