#include "mgs5vr/xr_runtime.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/head_camera.hpp"
#include <Xinput.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace mgs5vr {
namespace {
void xrCheck(XrResult result,const char* op) {
    if(XR_FAILED(result)) throw std::runtime_error(std::string(op)+" XrResult="+std::to_string(result));
}
Pose fromXr(XrPosef p) { return {{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},{p.position.x,p.position.y,p.position.z}}; }
XrPosef toXr(Pose p) { return {{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},{p.position.x,p.position.y,p.position.z}}; }
constexpr XrPosef identity{{0,0,0,1},{0,0,0}};
constexpr auto validPoseBits=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT;
bool sameLuid(LUID a,LUID b) { return a.LowPart==b.LowPart&&a.HighPart==b.HighPart; }

struct Instance {
    XrInstance handle{XR_NULL_HANDLE};
    XrSystemId system{XR_NULL_SYSTEM_ID};
    XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
    std::string runtime;
    Instance() {
        uint32_t count=0;
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr),"Enumerate OpenXR extensions");
        std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,extensions.data()),"Read OpenXR extensions");
        const bool d3d=std::any_of(extensions.begin(),extensions.end(),[](const auto& x){return std::strcmp(x.extensionName,XR_KHR_D3D11_ENABLE_EXTENSION_NAME)==0;});
        if(!d3d) throw std::runtime_error("OpenXR runtime does not expose XR_KHR_D3D11_enable");
        const char* enabled=XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(info.applicationInfo.applicationName,"MGS5VR native stereo experiment");
        strcpy_s(info.applicationInfo.engineName,"MGS5VR");
        info.applicationInfo.applicationVersion=1;
        info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        info.enabledExtensionCount=1; info.enabledExtensionNames=&enabled;
        xrCheck(xrCreateInstance(&info,&handle),"Create OpenXR instance");
        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
        const auto result=xrGetInstanceProperties(handle,&props);
        if(XR_FAILED(result)) { xrDestroyInstance(handle); handle=XR_NULL_HANDLE; xrCheck(result,"Read runtime properties"); }
        runtime=props.runtimeName;
    }
    void getSystem() {
        XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO}; info.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        xrCheck(xrGetSystem(handle,&info,&system),"Get headset (power on/connect headset if unavailable)");
        xrCheck(xrGetSystemProperties(handle,system,&properties),"Read headset properties");
    }
    ~Instance() {
        if(handle){
            log("OpenXR destroying instance");
            xrDestroyInstance(handle);
            log("OpenXR instance destroyed");
        }
    }
    Instance(const Instance&)=delete;
    Instance& operator=(const Instance&)=delete;
};

struct Session {
    Instance& instance;
    XrSession handle{XR_NULL_HANDLE};
    XrSpace local{XR_NULL_HANDLE}, view{XR_NULL_HANDLE};
    XrActionSet actions{XR_NULL_HANDLE};
    XrAction grip{XR_NULL_HANDLE},aim{XR_NULL_HANDLE},recenter{XR_NULL_HANDLE};
    XrAction sticks{},triggers{},squeezes{},thumbClick{},menu{};
    std::array<XrAction,4> face{}; // Native Xbox A, B, X, Y.
    std::array<XrSpace,2> gripSpaces{},aimSpaces{};
    std::array<XrPath,2> hands{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    LUID luid{};
    bool running{}, focused{}, recenterRequested{true}, exiting{};
    bool priorRecenter{}, priorFocused{};
    bool priorHeadToggle{};
    MenuButton menuButton;
    XrTime pendingLocalChange{};
    explicit Session(Instance& i):instance(i){}
    XrPath path(const char* p) { XrPath v; xrCheck(xrStringToPath(instance.handle,p,&v),"Create OpenXR path"); return v; }
    XrAction action(const char* name,const char* label,XrActionType type,bool withHands=false) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        strcpy_s(info.actionName,name); strcpy_s(info.localizedActionName,label);
        info.actionType=type;
        if(withHands){info.countSubactionPaths=2;info.subactionPaths=hands.data();}
        XrAction result; xrCheck(xrCreateAction(actions,&info,&result),"Create action"); return result;
    }
    void initialize() {
        PFN_xrGetD3D11GraphicsRequirementsKHR requirementsFn{};
        xrCheck(xrGetInstanceProcAddr(instance.handle,"xrGetD3D11GraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirementsFn)),"Get D3D11 requirements function");
        XrGraphicsRequirementsD3D11KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        xrCheck(requirementsFn(instance.handle,instance.system,&requirements),"Get XR GPU requirements");
        luid=requirements.adapterLuid;
        ComPtr<IDXGIFactory1> factory;
        checkHr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Create DXGI factory");
        ComPtr<IDXGIAdapter1> selected;
        for(UINT n=0;;++n){
            ComPtr<IDXGIAdapter1> adapter;
            const auto r=factory->EnumAdapters1(n,&adapter);
            if(r==DXGI_ERROR_NOT_FOUND)break;
            checkHr(r,"Enumerate XR GPU");
            DXGI_ADAPTER_DESC1 desc{}; checkHr(adapter->GetDesc1(&desc),"Read GPU LUID");
            if(sameLuid(desc.AdapterLuid,luid)){selected=adapter;break;}
        }
        if(!selected)throw std::runtime_error("Headset GPU not found; cannot create D3D11 session");
        const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL actual{};
        checkHr(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,2,D3D11_SDK_VERSION,&device,&actual,&context),"Create XR D3D11 device");
        if(actual<requirements.minFeatureLevel) throw std::runtime_error("GPU does not satisfy OpenXR feature level");
        uint32_t count=0;
        xrCheck(xrEnumerateEnvironmentBlendModes(instance.handle,instance.system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"Enumerate blend modes");
        std::vector<XrEnvironmentBlendMode> modes(count);
        xrCheck(xrEnumerateEnvironmentBlendModes(instance.handle,instance.system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,modes.data()),"Read blend modes");
        if(std::find(modes.begin(),modes.end(),XR_ENVIRONMENT_BLEND_MODE_OPAQUE)==modes.end())throw std::runtime_error("Opaque VR environment blend mode unavailable");
        XrGraphicsBindingD3D11KHR graphics{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR}; graphics.device=device.Get();
        XrSessionCreateInfo ci{XR_TYPE_SESSION_CREATE_INFO}; ci.next=&graphics; ci.systemId=instance.system;
        xrCheck(xrCreateSession(instance.handle,&ci,&handle),"Create D3D11 OpenXR session");
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO}; space.poseInReferenceSpace=identity;
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
        xrCheck(xrCreateReferenceSpace(handle,&space,&local),"Create LOCAL space");
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
        xrCheck(xrCreateReferenceSpace(handle,&space,&view),"Create VIEW space");
        hands={path("/user/hand/left"),path("/user/hand/right")};
        XrActionSetCreateInfo ai{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy_s(ai.actionSetName,"theatre");strcpy_s(ai.localizedActionSetName,"MGS5VR theatre");
        xrCheck(xrCreateActionSet(instance.handle,&ai,&actions),"Create action set");
        grip=action("grip_pose","Grip pose",XR_ACTION_TYPE_POSE_INPUT,true);
        aim=action("aim_pose","Aim pose",XR_ACTION_TYPE_POSE_INPUT,true);
        recenter=action("recenter","Recenter screen",XR_ACTION_TYPE_BOOLEAN_INPUT);
        sticks=action("sticks","Native gamepad sticks",XR_ACTION_TYPE_VECTOR2F_INPUT,true);
        triggers=action("triggers","Native gamepad triggers",XR_ACTION_TYPE_FLOAT_INPUT,true);
        squeezes=action("squeezes","Native gamepad shoulders",XR_ACTION_TYPE_FLOAT_INPUT,true);
        thumbClick=action("thumb_click","Native gamepad stick clicks",XR_ACTION_TYPE_BOOLEAN_INPUT,true);
        menu=action("menu","Tap iDroid; hold Pause",XR_ACTION_TYPE_BOOLEAN_INPUT);
        face={action("a","Native gamepad A",XR_ACTION_TYPE_BOOLEAN_INPUT),action("b","Native gamepad B",XR_ACTION_TYPE_BOOLEAN_INPUT),
            action("x","Native gamepad X",XR_ACTION_TYPE_BOOLEAN_INPUT),action("y","Native gamepad Y",XR_ACTION_TYPE_BOOLEAN_INPUT)};
        struct Profile { const char* name; const char* center; const char* menuButton; int layout; };
        const Profile profiles[]={
            {"/interaction_profiles/oculus/touch_controller","/user/hand/right/input/thumbstick/click","/user/hand/left/input/menu/click",0},
            {"/interaction_profiles/valve/index_controller","/user/hand/right/input/thumbstick/click","/user/hand/left/input/b/click",1},
            {"/interaction_profiles/htc/vive_controller","/user/hand/right/input/trackpad/click","/user/hand/left/input/menu/click",2},
            {"/interaction_profiles/microsoft/motion_controller","/user/hand/right/input/thumbstick/click","/user/hand/left/input/menu/click",3},
            {"/interaction_profiles/khr/simple_controller","/user/hand/right/input/select/click","/user/hand/left/input/menu/click",4}
        };
        for(const auto& p:profiles){
            std::vector<XrActionSuggestedBinding> bindings={{grip,path("/user/hand/left/input/grip/pose")},
                {grip,path("/user/hand/right/input/grip/pose")},{aim,path("/user/hand/left/input/aim/pose")},
                {aim,path("/user/hand/right/input/aim/pose")},{recenter,path(p.center)},{menu,path(p.menuButton)}};
            const auto bind=[&](XrAction a,const std::string& s){bindings.push_back({a,path(s.c_str())});};
            for(const char* hand:{"/user/hand/left/input/","/user/hand/right/input/"}){
                const std::string h=hand;
                if(p.layout<4){
                    bind(sticks,h+(p.layout==2?"trackpad":"thumbstick"));
                    bind(thumbClick,h+(p.layout==2?"trackpad/click":"thumbstick/click"));
                    bind(triggers,h+"trigger/value");
                    bind(squeezes,h+((p.layout==2||p.layout==3)?"squeeze/click":"squeeze/value"));
                }
            }
            if(p.layout<=1){
                bind(face[0],"/user/hand/right/input/a/click");bind(face[1],"/user/hand/right/input/b/click");
                bind(face[2],p.layout==0?"/user/hand/left/input/x/click":"/user/hand/left/input/a/click");
                // Index left B is reserved for Start. Its native Y mapping needs a custom binding.
                if(p.layout==0)bind(face[3],"/user/hand/left/input/y/click");
            }else if(p.layout==4){bind(face[0],"/user/hand/right/input/select/click");bind(face[1],"/user/hand/left/input/select/click");}
            else {bind(face[0],"/user/hand/right/input/trigger/value");bind(face[1],"/user/hand/right/input/menu/click");}
            XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggested.interactionProfile=path(p.name);suggested.countSuggestedBindings=static_cast<uint32_t>(bindings.size());suggested.suggestedBindings=bindings.data();
            const auto r=xrSuggestInteractionProfileBindings(instance.handle,&suggested);
            if(r==XR_ERROR_PATH_UNSUPPORTED)log(std::string("Runtime does not support controller profile: ")+p.name);
            else xrCheck(r,"Suggest controller bindings");
        }
        for(size_t n=0;n<2;++n){
            XrActionSpaceCreateInfo as{XR_TYPE_ACTION_SPACE_CREATE_INFO};as.poseInActionSpace=identity;as.subactionPath=hands[n];
            as.action=grip;xrCheck(xrCreateActionSpace(handle,&as,&gripSpaces[n]),"Create grip space");
            as.action=aim;xrCheck(xrCreateActionSpace(handle,&as,&aimSpaces[n]),"Create aim space");
        }
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actions;
        xrCheck(xrAttachSessionActionSets(handle,&attach),"Attach actions");
    }
    void poll(){
        for(;;){
            XrEventDataBuffer buffer{XR_TYPE_EVENT_DATA_BUFFER};
            const auto r=xrPollEvent(instance.handle,&buffer);
            if(r==XR_EVENT_UNAVAILABLE)break;
            xrCheck(r,"Poll OpenXR events");
            if(buffer.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){exiting=true;break;}
            if(buffer.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){
                const auto& change=*reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(&buffer);
                if(change.session==handle&&change.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL)
                    pendingLocalChange=change.changeTime;
            }
            if(buffer.type!=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)continue;
            const auto& e=*reinterpret_cast<const XrEventDataSessionStateChanged*>(&buffer);
            if(e.session!=handle)continue;
            log("OpenXR session state="+std::to_string(e.state));
            focused=e.state==XR_SESSION_STATE_FOCUSED;
            if(e.state==XR_SESSION_STATE_READY&&!running){
                XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                xrCheck(xrBeginSession(handle,&begin),"Begin XR session");running=true;recenterRequested=true;
            }else if(e.state==XR_SESSION_STATE_STOPPING&&running){xrCheck(xrEndSession(handle),"End XR session");running=false;}
            else if(e.state==XR_SESSION_STATE_EXITING||e.state==XR_SESSION_STATE_LOSS_PENDING)exiting=true;
        }
    }
    bool boolean(XrAction a,XrPath hand=XR_NULL_PATH){
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=a;info.subactionPath=hand;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        xrCheck(xrGetActionStateBoolean(handle,&info,&state),"Read controller button");
        return state.isActive&&state.currentState;
    }
    float scalar(XrAction a,XrPath hand){
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=a;info.subactionPath=hand;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        xrCheck(xrGetActionStateFloat(handle,&info,&state),"Read controller axis");
        return state.isActive&&std::isfinite(state.currentState)?std::clamp(state.currentState,0.0f,1.0f):0;
    }
    XrVector2f stick(XrPath hand){
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=sticks;info.subactionPath=hand;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        xrCheck(xrGetActionStateVector2f(handle,&info,&state),"Read controller stick");
        if(!state.isActive||!std::isfinite(state.currentState.x)||!std::isfinite(state.currentState.y))return {};
        return {std::clamp(state.currentState.x,-1.0f,1.0f),std::clamp(state.currentState.y,-1.0f,1.0f)};
    }
    bool trackedHand(size_t n,XrTime time){
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=grip;info.subactionPath=hands[n];
        XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};xrCheck(xrGetActionStatePose(handle,&info,&state),"Read grip tracking state");
        if(!state.isActive)return false;
        XrSpaceLocation pose{XR_TYPE_SPACE_LOCATION};xrCheck(xrLocateSpace(gripSpaces[n],local,time,&pose),"Locate controller grip");
        return (pose.locationFlags&validPoseBits)==validPoseBits&&valid(fromXr(pose.pose));
    }
    void syncInput(XrTime time){
        if(!focused){priorFocused=false;priorRecenter=false;menuButton.update(true,false,steadyMilliseconds());gamepadMailbox().publish({},false,steadyMilliseconds());return;}
        XrActiveActionSet active{actions,XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
        const auto r=xrSyncActions(handle,&sync);
        if(r==XR_SESSION_NOT_FOCUSED){priorFocused=false;menuButton.update(true,false,steadyMilliseconds());gamepadMailbox().publish({},false,steadyMilliseconds());return;}
        xrCheck(r,"Sync controller actions");
        const bool left=trackedHand(0,time),right=trackedHand(1,time);
        const float ls=left?scalar(squeezes,hands[0]):0,rs=right?scalar(squeezes,hands[1]):0;
        const bool center=boolean(recenter)&&ls>0.75f&&rs>0.75f;
        const bool headToggle=headCamera().available()&&left&&ls>0.75f&&boolean(thumbClick,hands[0]);
        if(priorFocused&&headToggle&&!priorHeadToggle){headCamera().toggle();log("Native head-camera toggle requested through OpenXR");}
        priorHeadToggle=headToggle;
        if(priorFocused&&center&&!priorRecenter)recenterRequested=true;
        if(priorFocused&&center&&!priorRecenter)log("OpenXR recenter chord accepted");
        GamepadSample pad{};
        const auto bit=[&](bool enabled,WORD mask){if(enabled)pad.buttons|=mask;};
        bit(right&&boolean(face[0]),XINPUT_GAMEPAD_A);bit(right&&boolean(face[1]),XINPUT_GAMEPAD_B);
        bit(left&&boolean(face[2]),XINPUT_GAMEPAD_X);bit(left&&boolean(face[3]),XINPUT_GAMEPAD_Y);
        pad.buttons|=menuButton.update(boolean(menu),left,steadyMilliseconds());
        bit(left&&boolean(thumbClick,hands[0])&&!headToggle,XINPUT_GAMEPAD_LEFT_THUMB);
        bit(right&&boolean(thumbClick,hands[1])&&!center,XINPUT_GAMEPAD_RIGHT_THUMB);
        bit(ls>0.5f&&!center&&!headToggle,XINPUT_GAMEPAD_LEFT_SHOULDER);bit(rs>0.5f&&!center,XINPUT_GAMEPAD_RIGHT_SHOULDER);
        pad.leftTrigger=static_cast<uint8_t>((left?scalar(triggers,hands[0]):0)*255);
        pad.rightTrigger=static_cast<uint8_t>((right?scalar(triggers,hands[1]):0)*255);
        const auto l=left?stick(hands[0]):XrVector2f{},rr=right?stick(hands[1]):XrVector2f{};
        pad.leftX=static_cast<int16_t>(l.x*32767);pad.leftY=static_cast<int16_t>(l.y*32767);
        pad.rightX=static_cast<int16_t>(rr.x*32767);pad.rightY=static_cast<int16_t>(rr.y*32767);
        gamepadMailbox().publish(pad,left||right,steadyMilliseconds());
        priorRecenter=center;priorFocused=true;
    }
    ~Session(){
        log("OpenXR releasing session resources");
        gamepadMailbox().publish({},false,steadyMilliseconds());
        for(auto s:aimSpaces)if(s)xrDestroySpace(s);
        for(auto s:gripSpaces)if(s)xrDestroySpace(s);
        if(view)xrDestroySpace(view);
        if(local)xrDestroySpace(local);
        if(handle){
            log("OpenXR destroying session");
            xrDestroySession(handle);
            log("OpenXR session destroyed");
        }
        if(actions)xrDestroyActionSet(actions);
    }
};

struct Screen {
    Session& session;
    XrSwapchain handle{XR_NULL_HANDLE};
    std::vector<XrSwapchainImageD3D11KHR> images;
    uint32_t width{},height{},pendingIndex{};
    DXGI_FORMAT sourceFormat{DXGI_FORMAT_UNKNOWN};
    bool pending{},ready{};
    FrameId copied{};
    explicit Screen(Session& s):session(s){}
    ~Screen(){reset();}
    void reset(){if(handle)xrDestroySwapchain(handle);handle=XR_NULL_HANDLE;images.clear();pending=ready=false;copied={};}
    void create(D3D11_TEXTURE2D_DESC desc){
        reset();
        if(desc.Width>session.instance.properties.graphicsProperties.maxSwapchainImageWidth
            ||desc.Height>session.instance.properties.graphicsProperties.maxSwapchainImageHeight)
            throw std::runtime_error("Game resolution exceeds the OpenXR swapchain limit; reduce game resolution");
        width=desc.Width;height=desc.Height;sourceFormat=desc.Format;
        uint32_t count=0;xrCheck(xrEnumerateSwapchainFormats(session.handle,0,&count,nullptr),"Enumerate XR formats");
        std::vector<int64_t> formats(count);xrCheck(xrEnumerateSwapchainFormats(session.handle,count,&count,formats.data()),"Read XR formats");
        const bool bgra=sourceFormat==DXGI_FORMAT_B8G8R8A8_UNORM||sourceFormat==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        const int64_t srgb=bgra?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        // The desktop backbuffer contains display-encoded pixels, even when tagged UNORM.
        // Use an sRGB XR swapchain so the compositor decodes those same bytes correctly.
        if(std::find(formats.begin(),formats.end(),srgb)==formats.end())
            throw std::runtime_error("Runtime lacks the matching sRGB theatre format; color conversion is required");
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        ci.format=srgb;ci.sampleCount=1;ci.width=width;ci.height=height;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;
        xrCheck(xrCreateSwapchain(session.handle,&ci,&handle),"Create theatre swapchain");
        xrCheck(xrEnumerateSwapchainImages(handle,0,&count,nullptr),"Count XR images");
        images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        xrCheck(xrEnumerateSwapchainImages(handle,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"Get XR D3D11 textures");
        log("Theatre swapchain "+std::to_string(width)+"x"+std::to_string(height));
    }
    void upload(ID3D11Texture2D* source,FrameId id,uint32_t sourceSlice=0){
        if(!source||!id.sequence)return;
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        if(sourceSlice>=desc.ArraySize)throw std::invalid_argument("Missing native eye texture slice");
        if(!handle||desc.Width!=width||desc.Height!=height||desc.Format!=sourceFormat||(copied.epoch!=id.epoch&&ready))create(desc);
        if(ready&&copied==id)return;
        if(!pending){
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrCheck(xrAcquireSwapchainImage(handle,&ai,&pendingIndex),"Acquire XR image");pending=true;
        }
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=50'000'000;
        const auto r=xrWaitSwapchainImage(handle,&wait);
        if(r==XR_TIMEOUT_EXPIRED)return; // Retain acquisition and wait again on the next XR frame.
        xrCheck(r,"Wait XR image");
        session.context->CopySubresourceRegion(images.at(pendingIndex).texture,0,0,0,0,source,sourceSlice,nullptr);
        session.context->Flush();
        checkHr(session.device->GetDeviceRemovedReason(),"XR device health");
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrCheck(xrReleaseSwapchainImage(handle,&release),"Release XR image");
        pending=false;ready=true;copied=id;
    }
};
struct EndFrameGuard {
    XrSession session;
    XrTime time;
    bool ended{};
    ~EndFrameGuard(){if(!ended){XrFrameEndInfo e{XR_TYPE_FRAME_END_INFO};e.displayTime=time;e.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;xrEndFrame(session,&e);}}
};
}

RuntimeProbe probeRuntime(){
    RuntimeProbe result;
    try {
        Instance instance;result.instanceAvailable=true;result.runtime=instance.runtime;
        instance.getSystem();result.headsetAvailable=true;result.system=instance.properties.systemName;
    }catch(const std::exception& e){result.error=e.what();}
    return result;
}

RuntimeStats runTheatre(TextureMailbox& source,const TheatreConfig& config,const std::atomic_bool& stop,std::chrono::seconds duration){
    if(!std::isfinite(config.widthMeters)||config.widthMeters<1||config.widthMeters>30
        ||!std::isfinite(config.distanceMeters)||config.distanceMeters<1||config.distanceMeters>30)
        throw std::invalid_argument("Theatre dimensions must be finite values from 1 to 30 meters");
    Instance instance;instance.getSystem();Session session(instance);session.initialize();
    log("OpenXR runtime="+instance.runtime+" headset="+instance.properties.systemName);
    TextureConsumer consumer(session.device.Get());Screen screen(session),leftEye(session),rightEye(session);RuntimeStats stats;
    const std::array<Screen*,2> eyeScreens{&leftEye,&rightEye};
    std::array<EyeFrame,2> eyeFrames{};
    uint64_t eyeEpoch{},projectionFrames{};
    Pose screenPose{};bool anchored=false;
    const auto start=std::chrono::steady_clock::now();
    bool closing=false;
    std::chrono::steady_clock::time_point closeDeadline{};
    while(!session.exiting){
        const auto now=std::chrono::steady_clock::now();
        if(!closing&&(stop.load()||(duration.count()>0&&now-start>=duration))){
            closing=true;closeDeadline=now+std::chrono::seconds(2);
            gamepadMailbox().publish({},false,steadyMilliseconds());
            headCamera().track({},false,steadyMilliseconds());
            if(!session.running)break;
            log("OpenXR requesting session exit");
            const auto result=xrRequestExitSession(session.handle);
            if(result==XR_ERROR_SESSION_NOT_RUNNING)break;
            xrCheck(result,"Request XR session exit");
        }
        session.poll();if(session.exiting||(closing&&!session.running))break;
        if(closing&&now>=closeDeadline){log("OpenXR STOPPING event deadline exceeded");break;}
        if(!session.running){std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
        XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};
        xrCheck(xrWaitFrame(session.handle,&wi,&frame),"Wait XR frame");
        XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};xrCheck(xrBeginFrame(session.handle,&bi),"Begin XR frame");
        EndFrameGuard guard{session.handle,frame.predictedDisplayTime};++stats.frames;
        // Keep paired empty frames until STOPPING; do not publish more native input during exit.
        if(closing)continue;
        if(session.pendingLocalChange&&frame.predictedDisplayTime>=session.pendingLocalChange){
            session.pendingLocalChange=0;session.recenterRequested=true;anchored=false;
        }
        session.syncInput(frame.predictedDisplayTime);
        XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
        xrCheck(xrLocateSpace(session.view,session.local,frame.predictedDisplayTime,&head),"Locate headset");
        const bool tracking=(head.locationFlags&validPoseBits)==validPoseBits&&valid(fromXr(head.pose));
        std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate.displayTime=frame.predictedDisplayTime;locate.space=session.local;
        XrViewState viewState{XR_TYPE_VIEW_STATE};uint32_t viewCount{};
        xrCheck(xrLocateViews(session.handle,&locate,&viewState,2,&viewCount,views.data()),"Locate native stereo views");
        constexpr auto viewValidBits=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
        const bool stereoTracked=viewCount==2&&(viewState.viewStateFlags&viewValidBits)==viewValidBits;
        std::array<EyeView,2> trackedViews{};
        for(size_t n=0;n<2;++n)trackedViews[n]={fromXr(views[n].pose),{views[n].fov.angleLeft,views[n].fov.angleRight,views[n].fov.angleUp,views[n].fov.angleDown}};
        headCamera().trackStereo(fromXr(head.pose),trackedViews,tracking&&stereoTracked&&session.focused,steadyMilliseconds());
        if(tracking&&(!anchored||session.recenterRequested)){
            screenPose=recenteredScreen(fromXr(head.pose),config.distanceMeters);anchored=true;session.recenterRequested=false;
        }
        if(!tracking)++stats.trackingInvalidFrames;
        auto channel=source.latest();
        if(channel&&!sameLuid(channel->adapterLuid,session.luid))throw std::runtime_error("Game and headset use different GPUs; shared capture is unavailable");
        const bool fresh=consumer.consume(channel);
        if(fresh)++stats.sourceFrames;
        if(eyeEpoch!=consumer.frame().epoch){eyeFrames={};eyeEpoch=consumer.frame().epoch;}
        const auto cameraStatus=headCamera().status();
        if(frame.shouldRender&&consumer.frame().sequence){
            if(!cameraStatus.active){screen.upload(consumer.texture(),consumer.frame());eyeFrames={};}
            else {
                const auto metadata=consumer.eyes();
                if(readyEyePair(metadata,cameraStatus.activation,steadyMilliseconds())){
                    for(size_t n=0;n<2;++n){auto& target=*eyeScreens[n];target.upload(consumer.texture(),consumer.frame(),static_cast<uint32_t>(n));
                        if(target.ready&&target.copied==consumer.frame())eyeFrames[n]=metadata[n];
                    }
                }
            }
        }
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        quad.space=session.local;quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;quad.pose=toXr(screenPose);
        quad.subImage.swapchain=screen.handle;
        quad.subImage.imageRect.extent={static_cast<int32_t>(screen.width),static_cast<int32_t>(screen.height)};
        quad.size={config.widthMeters,screen.width?config.widthMeters*static_cast<float>(screen.height)/static_cast<float>(screen.width):1};
        const auto* layer=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
        std::array<XrCompositionLayerProjectionView,2> projectionViews{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
        XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};projection.space=session.local;
        projection.viewCount=2;projection.views=projectionViews.data();
        for(size_t n=0;n<2;++n){auto& v=projectionViews[n];const auto& sourceEye=eyeFrames[n];
            v.pose=toXr(sourceEye.view.pose);v.fov={sourceEye.view.fov.left,sourceEye.view.fov.right,sourceEye.view.fov.up,sourceEye.view.fov.down};
            v.subImage.swapchain=eyeScreens[n]->handle;
            v.subImage.imageRect.extent={static_cast<int32_t>(eyeScreens[n]->width),static_cast<int32_t>(eyeScreens[n]->height)};
        }
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        if(frame.shouldRender&&tracking){
            if(cameraStatus.active){
                if(readyEyePair(eyeFrames,cameraStatus.activation,steadyMilliseconds())){
                    layer=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);end.layerCount=1;end.layers=&layer;++projectionFrames;
                }
            }else if(anchored&&screen.ready){end.layerCount=1;end.layers=&layer;++stats.submittedScreens;}
        }
        xrCheck(xrEndFrame(session.handle,&end),"Submit XR frame");guard.ended=true;
        if(stats.frames%300==0)log("XR frames="+std::to_string(stats.frames)+" theatre submissions="+std::to_string(stats.submittedScreens)
            +" projection submissions="+std::to_string(projectionFrames)+" captured frames="+std::to_string(stats.sourceFrames));
    }
    log("OpenXR frame loop stopped; releasing graphics resources");
    return stats;
}
}
