#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

namespace fs=std::filesystem;
using namespace Gdiplus;
namespace {
constexpr int designW=1080,designH=820;
constexpr UINT timer=1;
const Color paper(255,242,239,229),ink(255,37,38,37),red(255,175,38,36),muted(255,102,104,96);
enum Id { tpp=101,gz,browse,launch,install,controls,guide,folder,remove,pathBox,logBox,
    detect,applyDisplay,accountConfig,whatsNew,presetBox,scaleBox,widthBox,heightBox };
struct Button {Id id;RectF rect;std::wstring label;HWND window{};};
std::array<Button,13> buttons{{
    {tpp,{36,266,331,45},L"01   THE PHANTOM PAIN"},
    {gz,{377,266,331,45},L"02   GROUND ZEROES"},
    {browse,{590,361,118,37},L"BROWSE"},
    {launch,{36,463,672,64},L"LAUNCH IN STEAM   >"},
    {install,{36,543,331,48},L"INSTALL VR"},
    {controls,{377,543,331,48},L"EDIT CONTROLS"},
    {guide,{36,607,218,44},L"FIELD GUIDE"},
    {folder,{264,607,218,44},L"GAME FOLDER"},
    {remove,{492,607,216,44},L"REMOVE MOD"},
    {detect,{762,518,123,34},L"DETECT XR"},
    {applyDisplay,{895,518,123,34},L"APPLY SIZE"},
    {accountConfig,{762,610,256,30},L"ACCOUNT CONFIG..."},
    {whatsNew,{735,670,309,27},L"WHAT'S NEW / STILL OPEN"}
}};
HWND window{},pathControl{},logControl{};
HWND presetControl{},scaleControl{},widthControl{},heightControl{};
std::array<fs::path,2> graphicsConfigs;
std::wstring actual=L"ACTUAL: waiting for game";
uint64_t actualPolled{};
HINSTANCE instance{};
HFONT uiFont{};
IStream* artStream{};
std::unique_ptr<Bitmap> artwork;
fs::path package;
std::array<fs::path,2> games;
unsigned selected{};
bool busy{},preview{};
bool smoke{};
float scale=1,offsetX{},offsetY{},phase{};
std::wstring status=L"Select your game executable to get started.",transcript=L"Ready. No game or headset is launched automatically.\r\n";
HANDLE child{},pipeRead{};
bool failed{};
int displayPreset(){return static_cast<int>(SendMessageW(presetControl,CB_GETCURSEL,0,0));}
void displayEnabled(){
    const bool available=!busy&&!selected;
    EnableWindow(presetControl,available);
    EnableWindow(scaleControl,available&&displayPreset()==1);
    EnableWindow(widthControl,available&&displayPreset()==2);EnableWindow(heightControl,available&&displayPreset()==2);
}
void actualSize(){
    if(smoke||GetTickCount64()-actualPolled<1000)return;actualPolled=GetTickCount64();
    uint64_t tick{};unsigned pid{},width{},height{},pcWidth{},pcHeight{},windowed{};
    std::ifstream input(games[selected].parent_path()/L"mgs5vr-render-size.txt");
    std::wstring next=L"ACTUAL: waiting for game";
    if(input>>pid>>tick>>width>>height;input&&tick<=GetTickCount64()&&GetTickCount64()-tick<10000){
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);DWORD code{};
        if(process&&GetExitCodeProcess(process,&code)&&code==STILL_ACTIVE){
            next=L"EYE: "+std::to_wstring(width)+L" x "+std::to_wstring(height);
            if(input>>pcWidth>>pcHeight>>windowed)
                next+=L"\nPC: "+std::to_wstring(pcWidth)+L" x "+std::to_wstring(pcHeight)+(windowed?L" windowed":L" FULLSCREEN");
        }
        if(process)CloseHandle(process);
    }
    if(next!=actual){actual=next;InvalidateRect(window,nullptr,FALSE);}
}

std::wstring quote(const std::wstring& value){
    std::wstring out=L"\"";size_t slashes=0;
    for(const auto c:value){
        if(c==L'\\'){++slashes;continue;}
        if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');
        slashes=0;out+=c;
    }
    out.append(slashes*2,L'\\');out+=L'\"';return out;
}
std::wstring systemError(DWORD code){
    wchar_t* message{};FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,code,0,reinterpret_cast<wchar_t*>(&message),0,nullptr);
    std::wstring result=message?message:L"Windows could not complete the request.";LocalFree(message);return result;
}
void append(const std::wstring& value){
    transcript+=value;
    if(transcript.size()>24000)transcript.erase(0,transcript.size()-24000);
    if(logControl){SetWindowTextW(logControl,transcript.c_str());SendMessageW(logControl,EM_SETSEL,transcript.size(),transcript.size());SendMessageW(logControl,EM_SCROLLCARET,0,0);}
}
bool file(const fs::path& p){std::error_code ec;return !p.empty()&&fs::is_regular_file(p,ec);}
fs::path exePath(){std::array<wchar_t,32768> p{};GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size()));return p.data();}
bool installed(){const auto dir=games[selected].parent_path();return file(dir/L"mgs5vr-install.json")&&file(dir/L"dinput8.dll");}
bool hasPackage(){return file(package/L"tools/install.ps1")&&file(package/L"tools/launcher-maintenance.ps1")
    &&(file(package/L"dinput8.dll")||file(package/L"build/Release/dinput8.dll"));}
std::wstring registry(HKEY key,const wchar_t* sub,const wchar_t* name){
    std::array<wchar_t,32768> value{};DWORD bytes=static_cast<DWORD>(sizeof(value));
    return RegGetValueW(key,sub,name,RRF_RT_REG_SZ,nullptr,value.data(),&bytes)==ERROR_SUCCESS?value.data():L"";
}
void loadGames(){
    const auto steam=registry(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath");
    for(unsigned i=0;i<games.size();++i){
        games[i]=registry(HKEY_CURRENT_USER,L"Software\\Nikami\\MGS5VR\\Launcher",i?L"GzExe":L"TppExe");
        if(!file(games[i])&&!steam.empty())games[i]=fs::path(steam)/L"steamapps/common"/
            (i?L"Metal Gear Solid Ground Zeroes":L"MGS_TPP")/(i?L"MgsGroundZeroes.exe":L"mgsvtpp.exe");
        if(!file(games[i]))games[i].clear();
    }
}
void refresh(){
    const bool chosen=file(games[selected]);
    SetWindowTextW(pathControl,chosen?games[selected].c_str():L"Choose mgsvtpp.exe or MgsGroundZeroes.exe...");
    for(auto& b:buttons){
        bool enabled=!busy;
        if(b.id==launch)enabled&=chosen&&installed();
        if(b.id==install)enabled&=chosen&&selected==0&&hasPackage();
        if(b.id==controls)enabled&=chosen&&file(games[selected].parent_path()/L"mgs5vr-controls.ini")&&file(package/L"tools/edit-controls.ps1");
        if(b.id==folder)enabled&=chosen;
        if(b.id==remove)enabled&=chosen&&selected==0&&installed()&&hasPackage();
        if(b.id==applyDisplay)enabled&=chosen&&selected==0&&file(package/L"tools/launcher-display.ps1");
        if(b.id==detect)enabled&=file(package/L"tools/launcher-display.ps1");
        if(b.id==accountConfig)enabled&=selected==0;
        if(b.id==install)b.label=selected?L"FIRST-PERSON IN DEVELOPMENT":installed()?L"UPDATE / KEEP MY SETTINGS":L"INSTALL VR";
        if(b.window){EnableWindow(b.window,enabled);SetWindowTextW(b.window,b.label.c_str());InvalidateRect(b.window,nullptr,FALSE);}
    }
    displayEnabled();
    InvalidateRect(window,nullptr,FALSE);
}
void text(Graphics& g,float x,float y,float width,float height,const std::wstring& value,float size=18,Color color=ink,int style=FontStyleRegular){
    Font font(L"Bahnschrift",size,style,UnitPixel);SolidBrush brush(color);StringFormat format;
    format.SetTrimming(StringTrimmingEllipsisWord);
    g.DrawString(value.c_str(),-1,&font,RectF(x,y,width,height),&format,&brush);
}
void fill(Graphics& g,const RectF& r,Color color){SolidBrush brush(color);g.FillRectangle(&brush,r);}
void crop(Graphics& g,const RectF& dst,const RectF& src){
    if(!artwork)return;
    const float sx=static_cast<float>(artwork->GetWidth())/1086.f,sy=static_cast<float>(artwork->GetHeight())/1448.f;
    g.DrawImage(artwork.get(),dst,src.X*sx,src.Y*sy,src.Width*sx,src.Height*sy,UnitPixel);
}
void buttonPaint(Graphics& g,const Button& b,bool active,bool pressed,bool enabled,bool focused){
    const bool primary=b.id==launch;
    Color background=primary||active?red:paper,foreground=primary||active?paper:ink;
    if(!enabled){background=Color(255,223,221,212);foreground=Color(255,133,132,124);}
    else if(pressed)background=primary||active?Color(255,133,26,25):Color(255,225,219,202);
    fill(g,b.rect,background);
    if(!primary&&!active){Pen edge(Color(255,170,169,158));g.DrawRectangle(&edge,b.rect.X,b.rect.Y,b.rect.Width-1,b.rect.Height-1);}
    text(g,b.rect.X+14,b.rect.Y+(b.rect.Height-23)*.5f,b.rect.Width-24,28,b.label,primary?24.f:16.f,foreground,FontStyleBold);
    if(focused){Pen focus(foreground);focus.SetDashStyle(DashStyleDash);g.DrawRectangle(&focus,b.rect.X+4,b.rect.Y+4,b.rect.Width-9,b.rect.Height-9);}
}
void render(Graphics& g,bool includeControls){
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    fill(g,{0,0,designW,designH},paper);
    crop(g,{0,0,1080,218},{0,0,1086,219});
    fill(g,{0,142,595,76},paper);
    text(g,28,145,550,45,L"FIELD TERMINAL",34,ink,FontStyleBold);
    text(g,31,187,550,25,L"NATIVE WINDOWS  /  OPENXR  /  YOUR OWN GAME",14,muted);
    fill(g,{0,220,1080,29},ink);fill(g,{0,220,12,29},red);
    text(g,31,225,750,23,L"MGS5VR   //   EXPERIMENTAL PC VR MOD",15,paper,FontStyleBold);
    text(g,863,225,193,23,L"NIKAMI / FIELD DIVISION",12,paper);
    text(g,37,327,670,26,selected?L"GROUND ZEROES  /  EXPERIMENTAL STEREO ONLY":L"THE PHANTOM PAIN  /  TRACKED VR",19,ink,FontStyleBold);
    fill(g,{36,415,672,35},Color(255,229,226,215));fill(g,{36,415,4,35},failed?red:muted);
    text(g,49,423,647,24,busy?L"WORKING — keep this window open until maintenance finishes.":status,14,failed?red:ink);
    fill(g,{735,266,309,385},ink);fill(g,{735,266,6,385},red);
    text(g,762,286,256,30,L"VR RENDER SIZE",23,paper,FontStyleBold);
    text(g,762,324,256,22,L"PER EYE / NOT DESKTOP OR SBS",12,paper);
    text(g,762,390,256,21,L"HEADSET SCALE (WIDTH + HEIGHT)",12,paper);
    text(g,762,452,256,21,L"CUSTOM WIDTH  x  HEIGHT",12,paper);
    text(g,762,563,260,41,selected?L"GZ: existing graphics settings.\nFirst-person rig still in development.":actual,14,paper);
    text(g,37,673,683,25,L"TRANSMISSION LOG  /  OUTPUT & RECOVERY DETAILS",14,muted,FontStyleBold);
    fill(g,{36,704,1008,76},Color(255,230,227,216));
    text(g,37,792,1000,20,L"NO TELEMETRY  /  NO ACCOUNTS  /  ORIGINAL ARTWORK PRESERVED  /  NOT AFFILIATED WITH KONAMI",11,muted);
    if(includeControls){
        fill(g,{36,361,542,37},Color(255,255,255,255));text(g,46,370,517,26,L"C:\\SteamLibrary\\steamapps\\common\\MGS_TPP\\mgsvtpp.exe",14);
        for(const auto& b:buttons)buttonPaint(g,b,b.id==(selected?gz:tpp),false,b.id!=remove,false);
        fill(g,{762,350,256,33},paper);text(g,768,357,240,23,L"Headset recommended (OpenXR)",14);
        fill(g,{762,413,256,31},paper);text(g,768,419,240,23,L"100% - runtime recommendation",14);
        fill(g,{762,474,123,31},paper);fill(g,{895,474,123,31},paper);
        text(g,768,480,113,23,L"2560",14);text(g,902,480,110,23,L"2560",14);
        text(g,47,715,980,57,L"Connect Virtual Desktop / PC VR first, then Detect XR. Set quality in your runtime.\nLauncher applies native dimensions before launch. Windows desktop and DSR stay untouched.",14);
    }
}
void layout(){
    RECT r{};GetClientRect(window,&r);scale=std::min(float(r.right)/designW,float(r.bottom)/designH);
    offsetX=(r.right-designW*scale)*.5f;offsetY=(r.bottom-designH*scale)*.5f;
    const auto place=[](HWND w,RectF bounds){SetWindowPos(w,nullptr,int(offsetX+bounds.X*scale),int(offsetY+bounds.Y*scale),
        int(bounds.Width*scale),int(bounds.Height*scale),SWP_NOZORDER|SWP_NOACTIVATE);};
    if(uiFont)DeleteObject(uiFont);
    uiFont=CreateFontW(-std::max(11,int(15*scale)),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(auto& b:buttons)place(b.window,b.rect);
    place(pathControl,{36,361,542,37});place(logControl,{45,710,990,64});
    place(presetControl,{762,350,256,180});place(scaleControl,{762,413,256,170});
    place(widthControl,{762,474,123,31});place(heightControl,{895,474,123,31});
    for(auto control:{presetControl,scaleControl,widthControl,heightControl})SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont),TRUE);
    SendMessageW(pathControl,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont),TRUE);SendMessageW(logControl,WM_SETFONT,reinterpret_cast<WPARAM>(uiFont),TRUE);
}
bool open(const std::wstring& target,const wchar_t* verb=L"open"){
    const auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(window,verb,target.c_str(),nullptr,nullptr,SW_SHOWNORMAL));
    if(result>32)return true;
    failed=true;status=L"Windows could not open that item.";append(status+L"\r\n");refresh();return false;
}
void choose(){
    std::array<wchar_t,32768> path{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=window;dialog.lpstrFile=path.data();
    dialog.nMaxFile=static_cast<DWORD>(path.size());dialog.lpstrTitle=L"Select your legally owned MGSV executable";
    dialog.lpstrFilter=L"MGSV games\0mgsvtpp.exe;MgsGroundZeroes.exe\0\0";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
    if(!GetOpenFileNameW(&dialog))return;
    const fs::path chosen=path.data();const auto name=chosen.filename().wstring();
    if(_wcsicmp(name.c_str(),L"mgsvtpp.exe")&&_wcsicmp(name.c_str(),L"MgsGroundZeroes.exe")){
        MessageBoxW(window,L"Choose mgsvtpp.exe or MgsGroundZeroes.exe.",L"Select a supported game",MB_OK|MB_ICONWARNING);return;
    }
    selected=_wcsicmp(name.c_str(),L"mgsvtpp.exe")?1u:0u;games[selected]=chosen;
    HKEY key{};
    if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Nikami\\MGS5VR\\Launcher",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS){
        const auto value=chosen.wstring();RegSetValueExW(key,selected?L"GzExe":L"TppExe",0,REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));RegCloseKey(key);
    }
    failed=false;status=installed()?L"Installation record found. Ready to launch or update.":L"Game selected. Install the mod before launching in VR.";refresh();
}
bool startPowerShell(const fs::path& script,const std::vector<std::wstring>& args,bool monitor){
    if(!file(script)){failed=true;status=L"Package incomplete: extract all files beside the launcher.";append(status+L"\r\n");refresh();return false;}
    std::array<wchar_t,32768> system{};GetSystemDirectoryW(system.data(),static_cast<UINT>(system.size()));
    const auto exe=fs::path(system.data())/L"WindowsPowerShell/v1.0/powershell.exe";
    std::wstring command=quote(exe.wstring())+L" -NoLogo -NoProfile -NonInteractive -STA -ExecutionPolicy Bypass -File "+quote(script.wstring());
    for(const auto& arg:args)command+=L" "+quote(arg);
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};HANDLE write{};
    // CREATE_NO_WINDOW hides PowerShell's console. SW_HIDE also hides the
    // editor's first WinForms window, making Edit controls appear broken.
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=monitor?STARTF_USESHOWWINDOW:0;startup.wShowWindow=SW_HIDE;
    HANDLE nullInput{};
    if(monitor){
        if(!CreatePipe(&pipeRead,&write,&security,0)||!SetHandleInformation(pipeRead,HANDLE_FLAG_INHERIT,0)){
            const auto error=GetLastError();if(pipeRead){CloseHandle(pipeRead);pipeRead=nullptr;}if(write)CloseHandle(write);
            failed=true;status=L"Could not connect installer output.";append(systemError(error));refresh();return false;
        }
        nullInput=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        startup.dwFlags|=STARTF_USESTDHANDLES;startup.hStdInput=nullInput;startup.hStdOutput=startup.hStdError=write;
    }
    PROCESS_INFORMATION info{};
    const BOOL ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,monitor,CREATE_NO_WINDOW,nullptr,package.c_str(),&startup,&info);
    const auto error=GetLastError();if(write)CloseHandle(write);if(nullInput&&nullInput!=INVALID_HANDLE_VALUE)CloseHandle(nullInput);
    if(!ok){if(pipeRead){CloseHandle(pipeRead);pipeRead=nullptr;}failed=true;status=L"Could not start the installer/editor.";append(systemError(error));refresh();return false;}
    CloseHandle(info.hThread);
    if(monitor){child=info.hProcess;busy=true;failed=false;append(L"\r\nWorking. Keep this window open; detailed output follows.\r\n");refresh();}
    else CloseHandle(info.hProcess);
    return true;
}
void poll(){
    if(!child)return;
    DWORD available{},drained{};
    while(drained<65536&&PeekNamedPipe(pipeRead,nullptr,0,nullptr,&available,nullptr)&&available){
        std::array<char,4096> bytes{};DWORD received{};if(!ReadFile(pipeRead,bytes.data(),std::min<DWORD>(available,4095),&received,nullptr)||!received)break;
        drained+=received;
        UINT cp=CP_UTF8;int count=MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,bytes.data(),int(received),nullptr,0);
        if(!count){cp=CP_ACP;count=MultiByteToWideChar(cp,0,bytes.data(),int(received),nullptr,0);}
        std::wstring value(static_cast<size_t>(count),L' ');MultiByteToWideChar(cp,0,bytes.data(),int(received),value.data(),count);append(value);
    }
    DWORD code{};if(GetExitCodeProcess(child,&code)&&code!=STILL_ACTIVE){
        // A child may exit with more than one tick's output still buffered.
        // Drain that on following ticks before closing the pipe.
        if(PeekNamedPipe(pipeRead,nullptr,0,nullptr,&available,nullptr)&&available)return;
        CloseHandle(child);child=nullptr;CloseHandle(pipeRead);pipeRead=nullptr;busy=false;failed=code!=0;
        status=failed?L"Operation stopped. Details and any recovery path are in the log.":L"Complete. Your game archives and saves were not changed.";
        append(L"\r\n"+status+L"\r\n");refresh();
    }
}
void action(Id id){
    if(busy)return;
    if(id==tpp||id==gz){selected=id==gz?1u:0u;failed=false;status=selected?L"GZ first-person adapters are not ready. Existing installs only.":installed()?L"Installation record found. Ready to launch or update.":L"Select your game executable to get started.";refresh();}
    else if(id==browse)choose();
    else if(id==launch||id==applyDisplay||id==detect){
        if(id!=detect&&(!file(games[selected])||(id==launch&&!installed())))return;
        const wchar_t* preset=selected||displayPreset()==0?L"Current":displayPreset()==1?L"Headset":L"Custom";
        const auto value=[](HWND control){std::array<wchar_t,64> buffer{};GetWindowTextW(control,buffer.data(),static_cast<int>(buffer.size()));return std::wstring(buffer.data());};
        const std::array<const wchar_t*,5> scales{L"50",L"75",L"100",L"125",L"150"};
        auto scaleIndex=static_cast<int>(SendMessageW(scaleControl,CB_GETCURSEL,0,0));if(scaleIndex<0||scaleIndex>4)scaleIndex=2;
        std::vector<std::wstring> args{L"-Mode",id==detect?L"Detect":id==launch?L"Launch":L"Apply",
            L"-GameExe",games[selected].wstring(),L"-Preset",preset,L"-Scale",scales[scaleIndex],
            L"-Width",value(widthControl),L"-Height",value(heightControl)};
        if(!graphicsConfigs[selected].empty()){args.push_back(L"-GraphicsConfig");args.push_back(graphicsConfigs[selected].wstring());}
        startPowerShell(package/L"tools/launcher-display.ps1",args,true);
    }else if(id==accountConfig){
        std::array<wchar_t,32768> path{};OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=window;dialog.lpstrFile=path.data();
        dialog.nMaxFile=static_cast<DWORD>(path.size());dialog.lpstrTitle=L"Steam userdata / your account / 287700 / local / TPP_GRAPHICS_CONFIG";
        dialog.lpstrFilter=L"TPP graphics settings\0TPP_GRAPHICS_CONFIG\0\0";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&dialog)){graphicsConfigs[selected]=path.data();append(L"Graphics account: "+graphicsConfigs[selected].wstring()+L"\r\n");}
    }else if(id==whatsNew){
        MessageBoxW(window,L"NEW IN THIS LAUNCHER\n\nOpenXR per-eye recommendation, 50-150% scale, custom native width/height, pre-launch Apply, live actual render size and recoverable graphics backups. Windows resolution and DSR are untouched.\n\nCURRENT TPP CHANGES\n\nBinocular side-cup grip, pistol support acquisition, lens-only recon visibility with a glow switch, dwell acquisition, and native title/menu presentation.\n\nSTILL OPEN\n\nTrue mirrored left-handed rig; remaining HUD/cinematic/effect cases; complete weapons, gadgets and buddy interactions; Ground Zeroes first-person rig and wrist HUD. Physical headset coverage and the full showcase are not complete.",L"MGS5VR / Changes and remaining work",MB_OK|MB_ICONINFORMATION);
    }else if(id==controls){
        startPowerShell(package/L"tools/edit-controls.ps1",{L"-Path",(games[selected].parent_path()/L"mgs5vr-controls.ini").wstring()},false);
    }else if(id==guide){
        const auto local=package/L"docs/images/control-modes.svg";
        open(file(local)?local.wstring():L"https://github.com/nikamigaming-create/MGS5VR/blob/main/docs/CONTROLS.md");
    }else if(id==folder){if(file(games[selected]))open(games[selected].parent_path().wstring(),L"explore");}
    else if(id==install||id==remove){
        if(selected||!file(games[selected]))return;
        const std::wstring mode=id==remove?L"Remove":installed()?L"Update":L"Install";
        if(mode==L"Remove"&&MessageBoxW(window,L"Remove MGS5VR from game startup?\n\nMod files and settings are moved to a recoverable backup. Game archives and saves stay untouched.",L"Remove MGS5VR",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
        startPowerShell(package/L"tools/launcher-maintenance.ps1",{L"-Mode",mode,L"-GameExe",games[selected].wstring()},true);
    }
}
LRESULT CALLBACK procedure(HWND w,UINT message,WPARAM a,LPARAM b){
    switch(message){
    case WM_CREATE:
        window=w;
        for(auto& item:buttons)item.window=CreateWindowExW(0,L"BUTTON",item.label.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
            0,0,1,1,w,reinterpret_cast<HMENU>(static_cast<INT_PTR>(item.id)),instance,nullptr);
        pathControl=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_READONLY|ES_AUTOHSCROLL,0,0,1,1,w,reinterpret_cast<HMENU>(pathBox),instance,nullptr);
        logControl=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,0,0,1,1,w,reinterpret_cast<HMENU>(logBox),instance,nullptr);
        presetControl=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,1,1,w,reinterpret_cast<HMENU>(presetBox),instance,nullptr);
        scaleControl=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,1,1,w,reinterpret_cast<HMENU>(scaleBox),instance,nullptr);
        for(const auto label:{L"Keep current game settings",L"Headset recommended (OpenXR)",L"Custom native size"})SendMessageW(presetControl,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        for(const auto label:{L"50% - performance",L"75% - balanced",L"100% - runtime recommendation",L"125% - supersampling",L"150% - very demanding"})SendMessageW(scaleControl,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendMessageW(presetControl,CB_SETCURSEL,0,0);SendMessageW(scaleControl,CB_SETCURSEL,2,0);
        widthControl=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"2560",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,0,0,1,1,w,reinterpret_cast<HMENU>(widthBox),instance,nullptr);
        heightControl=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"2560",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,0,0,1,1,w,reinterpret_cast<HMENU>(heightBox),instance,nullptr);
        SendMessageW(widthControl,EM_SETLIMITTEXT,4,0);SendMessageW(heightControl,EM_SETLIMITTEXT,4,0);
        append(L"");layout();refresh();SetTimer(w,timer,50,nullptr);return 0;
    case WM_SIZE:if(pathControl)layout();InvalidateRect(w,nullptr,FALSE);return 0;
    case WM_DPICHANGED:{const auto* r=reinterpret_cast<RECT*>(b);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
    case WM_COMMAND:if(LOWORD(a)==presetBox&&HIWORD(a)==CBN_SELCHANGE)displayEnabled();else if(HIWORD(a)==BN_CLICKED)action(static_cast<Id>(LOWORD(a)));return 0;
    case WM_TIMER:poll();actualSize();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_DRAWITEM:{auto* draw=reinterpret_cast<DRAWITEMSTRUCT*>(b);
        const auto found=std::find_if(buttons.begin(),buttons.end(),[&](const auto& item){return static_cast<UINT>(item.id)==draw->CtlID;});
        if(found==buttons.end())break;
        Graphics g(draw->hDC);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.ScaleTransform(scale,scale);
        auto item=*found;item.rect.X=item.rect.Y=0;
        buttonPaint(g,item,item.id==(selected?gz:tpp),(draw->itemState&ODS_SELECTED)!=0,(draw->itemState&ODS_DISABLED)==0,(draw->itemState&ODS_FOCUS)!=0);return TRUE;}
    case WM_PAINT:{PAINTSTRUCT p{};const auto dc=BeginPaint(w,&p);RECT r{};GetClientRect(w,&r);
        if(r.right>0&&r.bottom>0){Bitmap buffer(r.right,r.bottom,PixelFormat32bppPARGB);Graphics g(&buffer);g.Clear(paper);g.TranslateTransform(offsetX,offsetY);g.ScaleTransform(scale,scale);render(g,false);Graphics screen(dc);screen.DrawImage(&buffer,0,0);}
        EndPaint(w,&p);return 0;}
    case WM_GETMINMAXINFO:{auto* info=reinterpret_cast<MINMAXINFO*>(b);info->ptMinTrackSize={760,610};return 0;}
    case WM_CLOSE:if(busy){MessageBoxW(w,L"The installer is still running. Let it finish so the previous installation stays recoverable.",L"Maintenance in progress",MB_OK|MB_ICONINFORMATION);return 0;}DestroyWindow(w);return 0;
    case WM_DESTROY:KillTimer(w,timer);if(uiFont)DeleteObject(uiFont);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,message,a,b);
}
bool loadArt(){
    const auto resource=FindResourceW(instance,MAKEINTRESOURCEW(101),RT_RCDATA);if(!resource)return false;
    const auto data=LockResource(LoadResource(instance,resource));const auto size=SizeofResource(instance,resource);
    artStream=SHCreateMemStream(static_cast<const BYTE*>(data),size);if(!artStream)return false;
    artwork.reset(Bitmap::FromStream(artStream));return artwork&&artwork->GetLastStatus()==Ok;
}
int savePreview(const wchar_t* path){
    Bitmap bitmap(designW,designH,PixelFormat32bppARGB);Graphics g(&bitmap);status=L"Your field terminal is ready. Connect PC VR before launch.";render(g,true);
    UINT count{},size{};GetImageEncodersSize(&count,&size);std::vector<BYTE> memory(size);auto* codecs=reinterpret_cast<ImageCodecInfo*>(memory.data());
    GetImageEncoders(count,size,codecs);
    for(UINT i=0;i<count;++i)if(!wcscmp(codecs[i].MimeType,L"image/png"))return bitmap.Save(path,&codecs[i].Clsid,nullptr)==Ok?0:1;
    return 1;
}
bool selfTest(){
    const std::vector<std::wstring> values{L"",L"simple",L"Player's game [test]",L"C:\\path with spaces\\",L"quotes\"inside",L"a\\\"b",L"$(&no execution);"};
    for(const auto& value:values){int count{};const auto command=L"launcher "+quote(value);auto** args=CommandLineToArgvW(command.c_str(),&count);
        const bool ok=args&&count==2&&value==args[1];if(args)LocalFree(args);if(!ok)return false;}
    return artwork&&artwork->GetWidth()>1000&&artwork->GetHeight()>1000;
}
}
int WINAPI wWinMain(HINSTANCE current,HINSTANCE,PWSTR,int show){
    instance=current;GdiplusStartupInput startup;ULONG_PTR token{};
    if(GdiplusStartup(&token,&startup,nullptr)!=Ok)return 1;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int exitCode=1;
    try{
        if(!loadArt())throw std::runtime_error("Original artwork resource could not be loaded");
        int count{};auto** args=CommandLineToArgvW(GetCommandLineW(),&count);
        if(count==3&&!wcscmp(args[1],L"--render-preview")){preview=true;exitCode=savePreview(args[2]);}
        else if(count==2&&!wcscmp(args[1],L"--self-test")){preview=true;exitCode=selfTest()?0:1;}
        else if(count==2&&!wcscmp(args[1],L"--ui-self-test"))smoke=true;
        if(args)LocalFree(args);
        if(!preview){
            package=exePath().parent_path();if(!file(package/L"tools/install.ps1")){const auto dev=package.parent_path().parent_path();if(file(dev/L"tools/install.ps1"))package=dev;}
            if(!smoke)loadGames();WNDCLASSEXW cls{sizeof(cls)};cls.hInstance=instance;cls.lpfnWndProc=procedure;cls.lpszClassName=L"MGS5VRFieldTerminal";
            cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassExW(&cls);
            RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);const auto dpi=GetDpiForSystem();
            const float fit=std::min({float(dpi)/96.f,float(work.right-work.left-40)/designW,float(work.bottom-work.top-65)/designH});
            RECT bounds{0,0,int(designW*fit),int(designH*fit)};AdjustWindowRectEx(&bounds,WS_OVERLAPPEDWINDOW,FALSE,0);
            window=CreateWindowExW(0,cls.lpszClassName,L"MGS5VR / Field Terminal",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
            if(!window)throw std::runtime_error("Could not create the launcher window");
            if(smoke){
                // Exercise this program's actual native controls without showing
                // a window, selecting real files or launching another process.
                const bool created=std::all_of(buttons.begin(),buttons.end(),[](const auto& item){return IsWindow(item.window)!=FALSE;})&&pathControl&&logControl&&presetControl&&scaleControl&&widthControl&&heightControl;
                SendMessageW(presetControl,CB_SETCURSEL,2,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(presetBox,CBN_SELCHANGE),0);
                const bool custom=IsWindowEnabled(widthControl)&&IsWindowEnabled(heightControl)&&!IsWindowEnabled(scaleControl);
                SendMessageW(presetControl,CB_SETCURSEL,1,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(presetBox,CBN_SELCHANGE),0);
                const bool automatic=!IsWindowEnabled(widthControl)&&IsWindowEnabled(scaleControl);
                SendMessageW(window,WM_COMMAND,MAKEWPARAM(gz,BN_CLICKED),0);
                const bool gzSafe=selected==1&&!IsWindowEnabled(buttons[4].window)&&!child;
                SendMessageW(window,WM_COMMAND,MAKEWPARAM(tpp,BN_CLICKED),0);
                const bool tppSafe=selected==0&&!IsWindowEnabled(buttons[3].window)&&!IsWindowEnabled(buttons[4].window)&&!child;
                DestroyWindow(window);exitCode=created&&custom&&automatic&&gzSafe&&tppSafe?0:1;
            }else{
                ShowWindow(window,show);UpdateWindow(window);MSG message{};
                while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
                exitCode=static_cast<int>(message.wParam);
            }
        }
    }catch(...){if(!preview)MessageBoxW(nullptr,L"The launcher could not initialize. Extract the complete package and try again.",L"MGS5VR",MB_OK|MB_ICONERROR);}
    artwork.reset();if(artStream)artStream->Release();CoUninitialize();GdiplusShutdown(token);return exitCode;
}
