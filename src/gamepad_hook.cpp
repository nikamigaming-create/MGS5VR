#include <windows.h>
#include <Xinput.h>
#include <MinHook.h>
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/mailbox.hpp"
#include "mgs5vr/motion_melee.hpp"
#include "mgs5vr/animal_interaction.hpp"
#include <cstring>
#include <stdexcept>
#include <mutex>
namespace mgs5vr {
namespace {
using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
using SetState=DWORD(WINAPI*)(DWORD,XINPUT_VIBRATION*);
GetState original{};
SetState originalSet{};
std::mutex stateMutex;
GamepadSample previous{};
DWORD packet{};
bool reported{};
DWORD WINAPI setState(DWORD index,XINPUT_VIBRATION* vibration){
    if(!vibration)return ERROR_BAD_ARGUMENTS;
    bool active{};
    gamepadMailbox().read(steadyMilliseconds(),&active);
    if(index==0&&active){
        rumbleMailbox().publish({vibration->wLeftMotorSpeed/65535.f,vibration->wRightMotorSpeed/65535.f,steadyMilliseconds()});
        return ERROR_SUCCESS;
    }
    return originalSet(index,vibration);
}
DWORD WINAPI getState(DWORD index,XINPUT_STATE* state){
    if(!state)return ERROR_BAD_ARGUMENTS;
    if(index==0){
        bool freshActive{};
        const auto sample=gamepadMailbox().read(steadyMilliseconds(),&freshActive);
        if(sample){
            if(freshActive){consumeAnimalTouch();consumeMeleeSweep();}
            // Give an attached physical pad back when XR is inactive. If none is
            // attached, synthesize neutral success to release the previous XR state.
            if(!freshActive&&original(index,state)==ERROR_SUCCESS)return ERROR_SUCCESS;
            std::lock_guard guard(stateMutex);
            if(*sample!=previous){++packet;previous=*sample;}
            *state={};state->dwPacketNumber=packet;
            state->Gamepad.wButtons=sample->buttons;
            state->Gamepad.bLeftTrigger=sample->leftTrigger;state->Gamepad.bRightTrigger=sample->rightTrigger;
            state->Gamepad.sThumbLX=sample->leftX;state->Gamepad.sThumbLY=sample->leftY;
            state->Gamepad.sThumbRX=sample->rightX;state->Gamepad.sThumbRY=sample->rightY;
            if(!reported){reported=true;log("Game consumed OpenXR controller state through its XInputGetState import");}
            return ERROR_SUCCESS;
        }
    }
    return original(index,state);
}
}
void installGamepadHook(){
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)throw std::runtime_error("Missing game PE header");
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        throw std::runtime_error("Invalid game PE64 header");
    const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!directory.VirtualAddress||directory.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR))throw std::runtime_error("Game import directory unavailable");
    const auto* entries=reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base+directory.VirtualAddress);
    for(size_t n=0;n<directory.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR)&&entries[n].Name;++n){
        const char* dll=reinterpret_cast<const char*>(base+entries[n].Name);
        if(_stricmp(dll,"XINPUT1_3.dll")!=0)continue;
        const auto module=GetModuleHandleA(dll);
        if(!module)throw std::runtime_error("Game XINPUT1_3 module unavailable");
        const auto fn=GetProcAddress(module,"XInputGetState");
        if(!fn)throw std::runtime_error("Native XInputGetState export unavailable");
        auto* thunk=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+entries[n].FirstThunk);
        const auto end=reinterpret_cast<uintptr_t>(base)+nt->OptionalHeader.SizeOfImage;
        const auto setFn=GetProcAddress(module,"XInputSetState");
        original=reinterpret_cast<GetState>(fn);originalSet=reinterpret_cast<SetState>(setFn);
        bool installed{},rumbleInstalled{};
        for(;reinterpret_cast<uintptr_t>(thunk)+sizeof(*thunk)<=end&&thunk->u1.Function;++thunk){
            const bool input=thunk->u1.Function==reinterpret_cast<ULONGLONG>(fn);
            const bool rumble=setFn&&thunk->u1.Function==reinterpret_cast<ULONGLONG>(setFn);
            if(!input&&!rumble)continue;
            const auto expected=input?fn:setFn;
            const auto replacement=input?reinterpret_cast<void*>(&getState):reinterpret_cast<void*>(&setState);
            DWORD old{};
            if(!VirtualProtect(&thunk->u1.Function,sizeof(void*),PAGE_READWRITE,&old))throw std::runtime_error("Cannot update game XInput import");
            const auto prior=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(&thunk->u1.Function),
                replacement,reinterpret_cast<void*>(expected));
            DWORD unused{};VirtualProtect(&thunk->u1.Function,sizeof(void*),old,&unused);
            if(prior!=reinterpret_cast<void*>(expected))throw std::runtime_error("Game input import changed concurrently");
            if(input)installed=true;
            if(rumble)rumbleInstalled=true;
            log(input?"Verified XInputGetState import routed to OpenXR gamepad; no OS input injection":"Native XInput rumble routed to tracked-controller haptics");
        }
        if(installed){
            // Some engine builds resolve vibration dynamically instead of
            // importing it. Hook that same process-local XInput export too.
            if(!rumbleInstalled&&setFn){
                const auto target=reinterpret_cast<void*>(setFn);
                if(MH_CreateHook(target,reinterpret_cast<void*>(&setState),reinterpret_cast<void**>(&originalSet))==MH_OK
                    &&MH_EnableHook(target)==MH_OK)log("Native dynamic XInput rumble routed to tracked-controller haptics");
                else log("Native XInput rumble adapter unavailable; wheel contact haptics remain active");
            }
            return;
        }
        throw std::runtime_error("XInput import does not resolve to XInputGetState; input bridge disabled");
    }
    throw std::runtime_error("Game XINPUT1_3 import not found");
}
}
