#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <iostream>
int main(){
    wchar_t folder[MAX_PATH]{};GetSystemDirectoryW(folder,MAX_PATH);
    const std::wstring path=std::wstring(folder)+L"\\dinput8.dll";
    const auto library=LoadLibraryW(path.c_str());
    if(!library)return 1;
    using Fn=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
    const auto system=reinterpret_cast<Fn>(GetProcAddress(library,"DirectInput8Create"));
    if(!system)return 1;
    IDirectInput8W *original{},*forwarded{};
    const auto a=system(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&original),nullptr);
    const auto b=DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&forwarded),nullptr);
    bool ok=a==b&&SUCCEEDED(a)&&original&&forwarded;
    if(original&&forwarded)ok=ok&&original->GetDeviceStatus(GUID_SysKeyboard)==forwarded->GetDeviceStatus(GUID_SysKeyboard);
    if(original)original->Release();if(forwarded)forwarded->Release();
    void *invalidA{},*invalidB{};
    const auto failA=system(GetModuleHandleW(nullptr),0,IID_IDirectInput8W,&invalidA,nullptr);
    const auto failB=DirectInput8Create(GetModuleHandleW(nullptr),0,IID_IDirectInput8W,&invalidB,nullptr);
    ok=ok&&FAILED(failA)&&failA==failB&&invalidA==nullptr&&invalidB==nullptr;
    FreeLibrary(library);
    std::cout<<(ok?"PASS":"FAIL")<<" proxy preserves system DirectInput success and failure behavior\n";
    return ok?0:1;
}
