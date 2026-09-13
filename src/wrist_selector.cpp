#include "mgs5vr/wrist_selector.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <string>

namespace mgs5vr {
WristSelectorImage makeWristSelectorImage(const EquipmentLabels& labels){
    constexpr int width=1440,height=320;
    WristSelectorImage image{width,height,std::vector<uint32_t>(width*height)};
    struct Canvas {
        HDC dc{CreateCompatibleDC(nullptr)};
        HBITMAP bitmap{};HGDIOBJ prior{};
        ~Canvas(){if(prior)SelectObject(dc,prior);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    } canvas;
    if(!canvas.dc)return {};
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels{};
    canvas.bitmap=CreateDIBSection(canvas.dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!canvas.bitmap||!pixels)return {};
    canvas.prior=SelectObject(canvas.dc,canvas.bitmap);
    SetBkMode(canvas.dc,TRANSPARENT);
    const auto fill=[&](RECT rect,COLORREF color){auto brush=CreateSolidBrush(color);FillRect(canvas.dc,&rect,brush);DeleteObject(brush);};
    const auto text=[&](std::wstring value,RECT rect,int size,COLORREF color,bool bold=false){
        auto font=CreateFontW(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        const auto prior=SelectObject(canvas.dc,font);SetTextColor(canvas.dc,color);
        DrawTextW(canvas.dc,value.c_str(),static_cast<int>(value.size()),&rect,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);
        SelectObject(canvas.dc,prior);DeleteObject(font);
    };
    fill({0,0,width,height},RGB(14,22,25));
    text(L"CHOOSE EQUIPMENT",{20,12,width-20,64},38,RGB(235,199,110),true);
    constexpr const wchar_t* names[]{L"PRIMARY",L"SECONDARY",L"SUPPORT",L"ITEMS"};
    for(int n=0;n<4;++n){
        const int left=16+n*356,right=left+340;
        fill({left,70,right,252},RGB(35,50,55));
        fill({left,70,right,74},RGB(143,195,198));
        text(names[n],{left+8,87,right-8,145},40,RGB(247,247,237),true);
        const auto length=strnlen_s(labels[n].data(),labels[n].size());
        const std::string label(labels[n].data(),length);
        text(std::wstring(label.begin(),label.end()),{left+12,156,right-12,242},30,
            label=="UNBOUND"?RGB(130,140,140):RGB(166,218,220));
    }
    text(L"Choose a category. Release the trigger to close.",{20,270,width-20,315},27,RGB(207,220,221));
    GdiFlush();std::memcpy(image.bgra.data(),pixels,image.bgra.size()*sizeof(uint32_t));
    for(auto& pixel:image.bgra)pixel|=0xff000000u;
    return image;
}
}
