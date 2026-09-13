#include "mgs5vr/xr_runtime.hpp"
#include "mgs5vr/log.hpp"
#include "mgs5vr/input_bridge.hpp"
#include "mgs5vr/controls.hpp"
#include "mgs5vr/native_controls.hpp"
#include "mgs5vr/head_camera.hpp"
#include "mgs5vr/controller_rig.hpp"
#include "mgs5vr/ui_renderer.hpp"
#include "mgs5vr/native_video.hpp"
#include <Xinput.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
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
    bool refreshControl{};
    Instance() {
        log("OpenXR loading runtime and enumerating extensions");
        uint32_t count=0;
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr),"Enumerate OpenXR extensions");
        std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
        xrCheck(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,extensions.data()),"Read OpenXR extensions");
        const bool d3d=std::any_of(extensions.begin(),extensions.end(),[](const auto& x){return std::strcmp(x.extensionName,XR_KHR_D3D11_ENABLE_EXTENSION_NAME)==0;});
        if(!d3d) throw std::runtime_error("OpenXR runtime does not expose XR_KHR_D3D11_enable");
        std::vector<const char*> enabled{XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
        refreshControl=std::any_of(extensions.begin(),extensions.end(),[](const auto& x){return std::strcmp(x.extensionName,XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME)==0;});
        if(refreshControl)enabled.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(info.applicationInfo.applicationName,"MGS5VR native stereo experiment");
        strcpy_s(info.applicationInfo.engineName,"MGS5VR");
        info.applicationInfo.applicationVersion=1;
        info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        info.enabledExtensionCount=static_cast<uint32_t>(enabled.size());info.enabledExtensionNames=enabled.data();
        log("OpenXR creating instance");
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
    XrAction sticks{},triggers{},squeezes{},thumbClick{},triggerTouch{},thumbTouch{},menu{};
    XrAction vibration{};
    std::array<XrAction,4> face{}; // Native Xbox A, B, X, Y.
    XrAction fallbackSelect{},fallbackBack{};
    std::array<ControllerFaceLayout,2> faceLayouts{};
    std::array<XrSpace,2> gripSpaces{},aimSpaces{};
    std::array<XrPath,2> hands{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    LUID luid{};
    bool running{}, focused{}, recenterRequested{true}, exiting{};
    bool priorRecenter{}, priorFocused{};
    bool priorHeadToggle{};
    bool automaticEntryDone{};
    bool manualScreenSelected{};
    bool priorTitleMenu{};
    ControlBindings controls;
    LiveControls liveControls;
    std::filesystem::path controlsPath;
    std::optional<std::filesystem::file_time_type> observedControlsWrite;
    std::optional<std::filesystem::file_time_type> handledControlsWrite;
    uint64_t controlsPollAt{};
    NativeControls nativeControls;
    RigInput rigControls;
    RigOptics opticsControls;
    bool binocularSelected{};
    WeaponScopeZoomInput weaponZoomInput;
    SnapTurn snapControls;
    NativeSmoothTurn smoothControls;
    OpticStabilizer opticStabilizer;
    float snapYaw{};
    OpticGate opticGate;
    bool priorBinocularSelected{};
    bool priorBButton{},priorOpticTracked{};
    std::array<std::array<char,96>,4> equipmentLabels{};
    uint64_t opticMarkSequence{};
    uint64_t opticClearSequence{};
    RigCommands commandsControls;
    bool simpleControllerProfile{};
    float reportedMagnification{1};
    uint64_t hapticAt{};
    bool wheelHeld{};
    XrTime pendingLocalChange{};
    uint64_t referenceEpoch{1};
    ControllerFrame controllerFrame{};
    explicit Session(Instance& i):instance(i){}
    void requestRefreshRate(){
        if(!instance.refreshControl){log("OpenXR refresh rate is controlled by the headset runtime; 90 Hz target must be set there");return;}
        PFN_xrEnumerateDisplayRefreshRatesFB enumerate{};PFN_xrRequestDisplayRefreshRateFB request{};
        if(XR_FAILED(xrGetInstanceProcAddr(instance.handle,"xrEnumerateDisplayRefreshRatesFB",reinterpret_cast<PFN_xrVoidFunction*>(&enumerate)))
           ||XR_FAILED(xrGetInstanceProcAddr(instance.handle,"xrRequestDisplayRefreshRateFB",reinterpret_cast<PFN_xrVoidFunction*>(&request)))||!enumerate||!request)return;
        uint32_t count{};if(XR_FAILED(enumerate(handle,0,&count,nullptr))||!count||count>32)return;
        std::vector<float> rates(count);if(XR_FAILED(enumerate(handle,count,&count,rates.data())))return;
        if(std::any_of(rates.begin(),rates.end(),[](float hz){return std::abs(hz-90.f)<.01f;}))
            log("OpenXR requested 90 Hz; result="+std::to_string(request(handle,90.f)));
        else log("OpenXR runtime does not offer 90 Hz; retaining its supported refresh rate");
    }
    XrPath path(const char* p) { XrPath v; xrCheck(xrStringToPath(instance.handle,p,&v),"Create OpenXR path"); return v; }
    XrAction action(const char* name,const char* label,XrActionType type,bool withHands=false) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        strcpy_s(info.actionName,name); strcpy_s(info.localizedActionName,label);
        info.actionType=type;
        if(withHands){info.countSubactionPaths=2;info.subactionPaths=hands.data();}
        XrAction result; xrCheck(xrCreateAction(actions,&info,&result),"Create action"); return result;
    }
    void refreshControlLabels(){
        constexpr std::array<std::string_view,4> categories{"equipment.primary","equipment.secondary","equipment.support","equipment.items"};
        for(size_t n=0;n<categories.size();++n){
            const auto label=controls.label(categories[n]);
            strncpy_s(equipmentLabels[n].data(),equipmentLabels[n].size(),label.c_str(),_TRUNCATE);
        }
    }
    void reloadControls(const PhysicalControls& physical,uint64_t now){
        if(!controlsPath.empty()&&now>=controlsPollAt){
            controlsPollAt=now+750;
            std::error_code error;
            const auto write=std::filesystem::last_write_time(controlsPath,error);
            if(error){observedControlsWrite.reset();liveControls.cancel();}
            else if(observedControlsWrite!=write){
                // Wait for two stable observations. Editors can truncate then
                // rewrite a file, or replace it using an atomic rename.
                observedControlsWrite=write;liveControls.cancel();
            }else if(handledControlsWrite!=write){
                const auto size=std::filesystem::file_size(controlsPath,error);
                if(!error&&size<=262144){
                    std::ifstream file(controlsPath,std::ios::binary);
                    if(file){
                        std::string contents(static_cast<size_t>(size),'\0');
                        file.read(contents.data(),static_cast<std::streamsize>(contents.size()));
                        const auto after=std::filesystem::last_write_time(controlsPath,error);
                        if(file&&!error&&after==write){
                            handledControlsWrite=write;
                            std::istringstream input(contents);
                            const auto errors=liveControls.stage(input);
                            if(errors.empty())log("Controls edit validated; release all buttons and center both sticks to apply live");
                            else {log("Controls edit rejected; keeping the last working layout");for(const auto& message:errors)log("Controls: "+message);}
                        }
                    }
                }else if(!error){
                    handledControlsWrite=write;liveControls.cancel();
                    log("Controls edit rejected: file exceeds 256 KiB; keeping the last working layout");
                }
            }
        }
        if(liveControls.apply(controls,physical)){
            nativeControls.suspend();rigControls.suspend();commandsControls.suspend();
            opticsControls.reset();opticGate.reset();snapControls.reset();smoothControls.reset();
            weaponZoomInput.update(false,false);refreshControlLabels();
            log("Controls applied LIVE: "+controlsPath.string()+"; no restart required");
        }
    }
    void initialize() {
        refreshControlLabels();
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
        triggerTouch=action("trigger_touch","Index finger contact",XR_ACTION_TYPE_BOOLEAN_INPUT,true);
        thumbTouch=action("thumb_touch","Thumb contact",XR_ACTION_TYPE_BOOLEAN_INPUT,true);
        vibration=action("vibration","Native feedback and wheel contact",XR_ACTION_TYPE_VIBRATION_OUTPUT,true);
        menu=action("menu","Tap iDroid; hold Pause",XR_ACTION_TYPE_BOOLEAN_INPUT);
        // Read each face button through its owning hand. An unscoped read can
        // aggregate left-select from the simple profile into Touch's right B
        // in an operator override, making left X also press Cancel.
        face={action("a","Native gamepad A",XR_ACTION_TYPE_BOOLEAN_INPUT,true),action("b","Native gamepad B",XR_ACTION_TYPE_BOOLEAN_INPUT,true),
            action("x","Native gamepad X",XR_ACTION_TYPE_BOOLEAN_INPUT,true),action("y","Native gamepad Y",XR_ACTION_TYPE_BOOLEAN_INPUT,true)};
        // Do not bind Vive/WMR trigger-to-confirm or simple-controller select
        // to the Touch face actions. The operator can override every suggested
        // source, even when its profile is not the active physical controller.
        fallbackSelect=action("fallback_select","Legacy controller select",XR_ACTION_TYPE_BOOLEAN_INPUT,true);
        fallbackBack=action("fallback_back","Legacy controller back",XR_ACTION_TYPE_BOOLEAN_INPUT,true);
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
            bind(vibration,"/user/hand/left/output/haptic");bind(vibration,"/user/hand/right/output/haptic");
            for(const char* hand:{"/user/hand/left/input/","/user/hand/right/input/"}){
                const std::string h=hand;
                if(p.layout<4){
                    bind(sticks,h+(p.layout==2?"trackpad":"thumbstick"));
                    bind(thumbClick,h+(p.layout==2?"trackpad/click":"thumbstick/click"));
                    bind(triggers,h+"trigger/value");
                    bind(squeezes,h+((p.layout==2||p.layout==3)?"squeeze/click":"squeeze/value"));
                }
                if(p.layout<=1){
                    bind(triggerTouch,h+"trigger/touch");
                    bind(thumbTouch,h+"thumbstick/touch");
                    if(p.layout==0)bind(thumbTouch,h+"thumbrest/touch");
                    const bool leftHand=h=="/user/hand/left/input/";
                    bind(thumbTouch,h+(p.layout==0&&leftHand?"x/touch":"a/touch"));
                    bind(thumbTouch,h+(p.layout==0&&leftHand?"y/touch":"b/touch"));
                }
            }
            if(p.layout<=1){
                bind(face[0],"/user/hand/right/input/a/click");bind(face[1],"/user/hand/right/input/b/click");
                bind(face[2],p.layout==0?"/user/hand/left/input/x/click":"/user/hand/left/input/a/click");
                // Index left B is reserved for Start. Its native Y mapping needs a custom binding.
                if(p.layout==0)bind(face[3],"/user/hand/left/input/y/click");
            }else if(p.layout==4){bind(fallbackSelect,"/user/hand/right/input/select/click");bind(fallbackSelect,"/user/hand/left/input/select/click");}
            else {bind(fallbackSelect,"/user/hand/right/input/trigger/value");bind(fallbackBack,"/user/hand/right/input/menu/click");}
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
            if(buffer.type==XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED)priorFocused=false;
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
                xrCheck(xrBeginSession(handle,&begin),"Begin XR session");running=true;recenterRequested=true;requestRefreshRate();
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
    TrackedHand trackedHand(size_t n,XrTime time){
        TrackedHand result;
        const auto locate=[&](XrAction action,XrSpace space,Pose& output){
            XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=action;info.subactionPath=hands[n];
            XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};xrCheck(xrGetActionStatePose(handle,&info,&state),"Read controller tracking state");
            if(!state.isActive)return false;
            XrSpaceLocation pose{XR_TYPE_SPACE_LOCATION};xrCheck(xrLocateSpace(space,local,time,&pose),"Locate controller pose");
            constexpr auto trackedBits=validPoseBits|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
            if((pose.locationFlags&trackedBits)!=trackedBits||!valid(fromXr(pose.pose)))return false;
            output=fromXr(pose.pose);return true;
        };
        result.gripTracked=locate(grip,gripSpaces[n],result.grip);
        result.aimTracked=locate(aim,aimSpaces[n],result.aim);
        return result;
    }
    void syncInput(XrTime time,Pose& head,std::array<EyeView,2>& views,bool stereoTracked){
        controllerFrame={};controllerFrame.snapYaw=snapYaw;
        if(!focused){nativeControls.suspend();snapControls.reset();rigControls.suspend();controls.suspend();opticsControls.reset();opticGate.reset();commandsControls.suspend();priorFocused=false;priorRecenter=false;gamepadMailbox().publish({},false,steadyMilliseconds());return;}
        XrActiveActionSet active{actions,XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
        const auto r=xrSyncActions(handle,&sync);
        if(r==XR_SESSION_NOT_FOCUSED){nativeControls.suspend();snapControls.reset();rigControls.suspend();controls.suspend();opticsControls.reset();opticGate.reset();commandsControls.suspend();priorFocused=false;gamepadMailbox().publish({},false,steadyMilliseconds());return;}
        xrCheck(r,"Sync controller actions");
        if(!priorFocused){
            snapControls.reset();
            smoothControls.reset();
            for(size_t side=0;side<hands.size();++side){
                faceLayouts[side]=ControllerFaceLayout::standard;
                XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
                if(XR_SUCCEEDED(xrGetCurrentInteractionProfile(handle,hands[side],&profile))&&profile.interactionProfile){
                    char name[XR_MAX_PATH_LENGTH]{};uint32_t length{};
                    xrCheck(xrPathToString(instance.handle,profile.interactionProfile,sizeof(name),&length,name),"Read controller profile name");
                    faceLayouts[side]=controllerFaceLayout(name);
                    log(std::string(side?"Right":"Left")+" controller profile="+name+"; face inputs isolated from legacy fallbacks");
                }
            }
            simpleControllerProfile=faceLayouts[1]==ControllerFaceLayout::simple;
        }
        controllerFrame={{trackedHand(0,time),trackedHand(1,time)},time,referenceEpoch};
        const bool right=controllerFrame.hands[1].gripTracked;
        // OpenXR action activity is independent of optical pose tracking.
        // Occluding a controller must not release B or the held wrist trigger.
        const float ls=scalar(squeezes,hands[0]),rs=scalar(squeezes,hands[1]);
        const float lt=scalar(triggers,hands[0]),rt=scalar(triggers,hands[1]);
        for(size_t n=0;n<2;++n){
            auto& hand=controllerFrame.hands[n];
            if(!hand.gripTracked)continue;
            hand.trigger=n?rt:lt;hand.squeeze=n?rs:ls;
            hand.triggerTouched=boolean(triggerTouch,hands[n]);
            hand.thumbTouched=boolean(thumbTouch,hands[n]);
        }
        auto nativeStatus=headCamera().status();
        const auto mode=controllerRigEnabled()?nativeTravelMode():TravelMode::unknown;
        const bool title=controllerRigEnabled()&&nativeTitleMenuOpen();
        const bool loading=controllerRigEnabled()&&nativeLoadingTipsOpen();
        controllerFrame.frontEnd=title;
        if(title!=priorTitleMenu){
            priorTitleMenu=title;headCamera().cancel();automaticEntryDone=manualScreenSelected;
            nativeStatus=headCamera().status();
            log(title?"Native Title presentation active":"Native Title presentation closed; preserving VR/screen choice");
        }
        // Enter tracked gameplay as soon as the real player is available.
        // Front-end/loading states retain their native menu controls. A manual
        // screen/VR choice is respected for the remainder of this XR session.
        const auto now=steadyMilliseconds();
        const auto l=stick(hands[0]),rr=stick(hands[1]);
        const auto faceButtons=isolatedFaceButtons(faceLayouts,
            {boolean(face[0],hands[1]),boolean(face[1],hands[1]),boolean(face[2],hands[0]),boolean(face[3],hands[0])},
            {boolean(fallbackSelect,hands[0]),boolean(fallbackSelect,hands[1])},
            {boolean(fallbackBack,hands[0]),boolean(fallbackBack,hands[1])});
        PhysicalControls physical;
        physical.buttons={faceButtons[0]?1.f:0.f,faceButtons[1]?1.f:0.f,
            faceButtons[2]?1.f:0.f,faceButtons[3]?1.f:0.f,
            boolean(menu)?1.f:0.f,boolean(thumbClick,hands[0])?1.f:0.f,
            boolean(thumbClick,hands[1])?1.f:0.f,ls,rs,lt,rt};
        physical.leftStick={l.x,l.y};physical.rightStick={rr.x,rr.y};
        reloadControls(physical,now);
        // Label/geometry changes belong to this same input publication.
        controllerFrame.equipmentLabels=equipmentLabels;
        controllerFrame.wristSurfaceLift=controls.setting("settings.wrist_surface_lift_cm")*.01f;
        controllerFrame.wristSelectorHeight=controls.setting("settings.wrist_selector_height_cm")*.01f;
        controllerFrame.wristPickerWidth=controls.setting("settings.wrist_picker_width_cm")*.01f;
        controllerFrame.hudMode=static_cast<HudMode>(static_cast<unsigned>(controls.setting("settings.hud_mode")));
        controllerFrame.scopeEyeRelief=controls.setting("settings.scope_eye_relief_cm")*.01f;
        controllerFrame.binocularAutoMark=controls.setting("settings.binocular_auto_mark")>=.5f;
        controllerFrame.binocularActorGlow=controls.setting("settings.binocular_actor_glow")>=.5f;
        controllerFrame.binocularMarkDwellMs=static_cast<uint64_t>(controls.setting("settings.binocular_mark_dwell_ms"));
        const bool gameplayContext=controllerRigEnabled()&&mode!=TravelMode::unknown&&!title
            &&(nativeStatus.active||nativeStatus.pending)&&!nativeStatus.nativeMenuOpen;
        const auto controlContext=nativeControls.selected()?ControlContext::nativeButtons:!gameplayContext?ControlContext::menus:
            commandsControls.active()?ControlContext::commands:
            rigControls.equipmentPhase()!=0?ControlContext::equipment:
            binocularSelected?ControlContext::binoculars:mode==TravelMode::vehicle?ControlContext::vehicle:
            mode==TravelMode::horse?ControlContext::horse:ControlContext::gameplay;
        controls.update(physical,controlContext,now);
        const auto nativeInput=nativeControls.update(controls,physical);
        if(nativeInput.changed){
            controls.suspend();rigControls.suspend();commandsControls.suspend();opticsControls.reset();opticGate.reset();snapControls.reset();
            log(nativeInput.selected?"Native buttons ON: all XInput controls; release inputs before use":"Native buttons OFF: normal VR bindings; release inputs before use");
        }
        const auto activeControl=[&](std::string_view name){return controls.active(name);};
        const bool manualToggle=headCamera().available()&&activeControl("system.toggle_vr");
        if(manualToggle||nativeStatus.active||nativeStatus.pending||nativeStatus.awaitingPlayer)automaticEntryDone=true;
        if(loading&&!title&&(nativeStatus.active||nativeStatus.pending)){
            headCamera().cancel();automaticEntryDone=manualScreenSelected;nativeStatus=headCamera().status();
        }
        if(!automaticEntryDone&&!loading&&stereoTracked&&right&&(mode!=TravelMode::unknown||title)
           &&nativeStatus.enabled&&headCamera().available()&&!nativeStatus.nativeMenuOpen){
            automaticEntryDone=true;headCamera().toggle();nativeStatus=headCamera().status();
            log(title?"Tracked VR entered for the native Title menu":"Tracked VR entered automatically on the first playable character");
        }
        const bool rigInput=controllerRigEnabled()&&mode!=TravelMode::unknown&&!title
            &&(nativeStatus.active||nativeStatus.pending)&&!nativeStatus.nativeMenuOpen;
        const bool menuPressed=activeControl("system.idroid")||activeControl("system.pause");
        const bool equipmentHeld=controls.value(mode==TravelMode::vehicle?"vehicle.equipment_open":"equipment.open")
            >(rigControls.equipmentPhase()!=0?.25f:.5f);
        const bool binocularAvailable=rigInput&&!nativeInput.exclusive&&mode==TravelMode::onFoot&&!simpleControllerProfile
            &&!manualToggle&&!menuPressed&&!commandsControls.active()
            &&!equipmentHeld&&rigControls.equipmentPhase()==0;
        const bool wasBinocularSelected=binocularSelected;
        binocularSelected=updateBinocularSelection(binocularSelected,activeControl("gameplay.equip_binoculars"),
            activeControl("binoculars.stow"),binocularAvailable);
        const bool binocularEquipped=binocularSelected&&!wasBinocularSelected;
        if((physical.buttons[1]>.5f)!=priorBButton){
            priorBButton=physical.buttons[1]>.5f;
            std::ostringstream s;s<<"B input="<<priorBButton<<" mode="<<static_cast<int>(controlContext)
                <<" equip_allowed="<<binocularAvailable<<" right_grip_tracked="<<right
                <<" right_aim_tracked="<<controllerFrame.hands[1].aimTracked<<" left_trigger="<<lt
                <<" picker_phase="<<rigControls.equipmentPhase();log(s.str());
        }
        if(binocularSelected!=priorBinocularSelected){
            priorBinocularSelected=binocularSelected;
            if(binocularSelected)log("Physical binoculars equipped by configured input; selection latched independently of tracking");
            else log(std::string("Physical binoculars stowed: ")+(activeControl("binoculars.stow")?"configured stow input":
                equipmentHeld||rigControls.equipmentPhase()?"equipment selection":commandsControls.active()?"Commands":
                menuPressed?"menu input":mode!=TravelMode::onFoot?"left on-foot mode":"left gameplay"));
        }
        // Recenter is its own configured action; R3 remains available for zoom.
        const bool centerChord=activeControl("system.recenter");
        const bool headToggle=manualToggle;
        const uint16_t menuBits=static_cast<uint16_t>((activeControl("system.idroid")?XINPUT_GAMEPAD_START:0)
            |(activeControl("system.pause")?XINPUT_GAMEPAD_BACK:0));
        const bool utilityCenter=centerChord;
        const bool opticSupport=activeControl("binoculars.support_grip");
        const auto binocularRotation=binocularGripRotation(controls.setting("settings.binocular_pitch_degrees"),
            controls.setting("settings.binocular_yaw_degrees"),controls.setting("settings.binocular_roll_degrees"));
        const bool opticAvailable=rigInput&&mode==TravelMode::onFoot&&right
            &&binocularSelected
            &&!commandsControls.active()&&!headToggle
            &&(opticGate.active()||!centerChord)&&stereoTracked;
        if(opticAvailable){
            auto& primary=controllerFrame.hands[1];const auto& support=controllerFrame.hands[0];
            if(const auto raw=solveBinocularPose(support.grip,primary.grip,support.aim,primary.aim,
                support.gripTracked,primary.gripTracked,support.aimTracked,primary.aimTracked,opticSupport,binocularSelected,binocularRotation)){
                const auto steady=opticStabilizer.update(head,primary.grip,*raw,true,now,referenceEpoch);
                primary.aim=compose(compose(steady,inverse(primary.grip)),primary.aim);primary.grip=steady;
            }else opticStabilizer.reset();
        }else opticStabilizer.reset();
        const auto optic=opticGate.update(head,views,
            controllerFrame.hands[0].grip,controllerFrame.hands[1].grip,
            controllerFrame.hands[0].aim,controllerFrame.hands[1].aim,
            controllerFrame.hands[0].gripTracked,controllerFrame.hands[1].gripTracked,
            controllerFrame.hands[0].aimTracked,controllerFrame.hands[1].aimTracked,
            opticSupport,binocularSelected,opticAvailable,now,referenceEpoch,binocularRotation);
        controllerFrame.optic=optic;
        // Bounded fit telemetry makes the eye-relief decision auditable from
        // the same LOCAL poses that drive the gate. It stays in the runtime
        // layer so the core contract tests remain dependency-free.
        static uint64_t fitTraceCalls{};
        if(right&&binocularSelected&&optic.pose.tracked&&(++fitTraceCalls%120u)==0u){
            const auto distance=[](Vec3 a,Vec3 b){const auto d=a-b;return std::sqrt(dot(d,d));};
            const auto facing=[](Pose a,Pose b){return dot(rotate(a.orientation,{0,0,-1}),rotate(b.orientation,{0,0,-1}));};
            std::ostringstream message;
            message<<"Optic fit socket="<<binocularOcularCenter.x<<","<<binocularOcularCenter.y<<","<<binocularOcularCenter.z<<" primary=real-ocular"
                <<" leftEye="<<optic.pose.leftEyepiece.position.x<<","<<optic.pose.leftEyepiece.position.y<<","<<optic.pose.leftEyepiece.position.z
                <<" rightEye="<<optic.pose.rightEyepiece.position.x<<","<<optic.pose.rightEyepiece.position.y<<","<<optic.pose.rightEyepiece.position.z
                <<" view0="<<views[0].pose.position.x<<","<<views[0].pose.position.y<<","<<views[0].pose.position.z
                <<" view1="<<views[1].pose.position.x<<","<<views[1].pose.position.y<<","<<views[1].pose.position.z
                <<" distances="<<distance(optic.pose.leftEyepiece.position,views[0].pose.position)<<","<<distance(optic.pose.rightEyepiece.position,views[1].pose.position)
                <<" facing="<<facing(optic.pose.leftEyepiece,views[0].pose)<<","<<facing(optic.pose.rightEyepiece,views[1].pose)
                <<" aligned="<<optic.aligned<<" active="<<optic.active;
            log(message.str());
        }
        controllerFrame.optic.held=optic.pose.tracked&&opticAvailable;
        if(binocularSelected&&controllerFrame.optic.held!=priorOpticTracked)
            log(controllerFrame.optic.held?"Binocular tracking restored; equipped presentation resumed":"Binocular tracking missing; selection retained");
        priorOpticTracked=controllerFrame.optic.held;
        if(optic.active&&!validateBinocularViews(optic,head,views)){
            opticGate.reset();controllerFrame.optic={};
            log("Physical binocular view rejected: invalid tracking");
        }
        const bool center=centerChord;
        if(optic.opened)log("Physical binocular ocular entered eye relief");
        if(optic.closed)log("Physical binoculars left eye relief; physical optic closed");
        if(priorFocused&&headToggle&&!priorHeadToggle){
            // The same chord works at Title and during gameplay. Scene
            // transitions may rebind the camera, but cannot override this choice.
            manualScreenSelected=nativeStatus.active||nativeStatus.pending||nativeStatus.awaitingPlayer;
            headCamera().toggle();
            log(manualScreenSelected?"Manual presentation: large quad":"Manual presentation: immersive VR");
        }
        priorHeadToggle=headToggle;
        if(priorFocused&&center&&!priorRecenter){recenterRequested=true;headCamera().recenter();}
        if(priorFocused&&center&&!priorRecenter)log("OpenXR recenter chord accepted");
        GamepadSample pad{};
        const auto bit=[&](bool enabled,WORD mask){if(enabled)pad.buttons|=mask;};
        const auto mappedButton=[&](std::string_view name,WORD mask){bit(activeControl(name),mask);};
        const auto triggerValue=[&](std::string_view name){return static_cast<uint8_t>(controls.value(name)*255);};
        auto move=controls.axis("axes.move",physical);
        auto navigation=controls.axis("axes.equipment",physical);
        if(nativeInput.exclusive){
            pad=nativeInput.gamepad;move={};navigation={};
        }else if(!rigInput){
            mappedButton("menus.confirm",XINPUT_GAMEPAD_A);mappedButton("menus.back",XINPUT_GAMEPAD_B);
            mappedButton("menus.action_x",XINPUT_GAMEPAD_X);mappedButton("menus.action_y",XINPUT_GAMEPAD_Y);
            mappedButton("menus.previous_tab",XINPUT_GAMEPAD_LEFT_SHOULDER);mappedButton("menus.next_tab",XINPUT_GAMEPAD_RIGHT_SHOULDER);
            mappedButton("menus.left_click",XINPUT_GAMEPAD_LEFT_THUMB);mappedButton("menus.right_click",XINPUT_GAMEPAD_RIGHT_THUMB);
            mappedButton("menus.dpad_up",XINPUT_GAMEPAD_DPAD_UP);mappedButton("menus.dpad_down",XINPUT_GAMEPAD_DPAD_DOWN);
            mappedButton("menus.dpad_left",XINPUT_GAMEPAD_DPAD_LEFT);mappedButton("menus.dpad_right",XINPUT_GAMEPAD_DPAD_RIGHT);
            pad.leftTrigger=triggerValue("menus.left_trigger");pad.rightTrigger=triggerValue("menus.right_trigger");
            move=controls.axis("axes.menu",physical);navigation=controls.axis("axes.map",physical);
        }else if(mode==TravelMode::vehicle){
            mappedButton("vehicle.native_a",XINPUT_GAMEPAD_A);mappedButton("vehicle.native_b",XINPUT_GAMEPAD_B);
            mappedButton("vehicle.weapon_or_call",XINPUT_GAMEPAD_X);mappedButton("vehicle.interact",XINPUT_GAMEPAD_Y);
            mappedButton("vehicle.left_click",XINPUT_GAMEPAD_LEFT_THUMB);mappedButton("vehicle.right_click",XINPUT_GAMEPAD_RIGHT_THUMB);
            pad.leftTrigger=triggerValue("vehicle.brake_reverse");pad.rightTrigger=triggerValue("vehicle.accelerate");
            move=controls.axis("axes.vehicle_steering",physical);
        }else{
            mappedButton("gameplay.native_a",XINPUT_GAMEPAD_A);mappedButton("gameplay.reload",XINPUT_GAMEPAD_B);
            mappedButton("gameplay.native_x",XINPUT_GAMEPAD_X);mappedButton("gameplay.interact",XINPUT_GAMEPAD_Y);
            mappedButton("gameplay.native_left_shoulder",XINPUT_GAMEPAD_LEFT_SHOULDER);
            mappedButton("gameplay.native_right_shoulder",XINPUT_GAMEPAD_RIGHT_SHOULDER);
            mappedButton("gameplay.native_right_click",XINPUT_GAMEPAD_RIGHT_THUMB);
            mappedButton("gameplay.native_dpad_up",XINPUT_GAMEPAD_DPAD_UP);mappedButton("gameplay.native_dpad_down",XINPUT_GAMEPAD_DPAD_DOWN);
            mappedButton("gameplay.native_dpad_left",XINPUT_GAMEPAD_DPAD_LEFT);mappedButton("gameplay.native_dpad_right",XINPUT_GAMEPAD_DPAD_RIGHT);
            mappedButton("horse.stance",XINPUT_GAMEPAD_A);mappedButton("horse.reload",XINPUT_GAMEPAD_B);
            mappedButton("horse.gallop",XINPUT_GAMEPAD_X);mappedButton("horse.interact",XINPUT_GAMEPAD_Y);
            mappedButton("horse.left_click",XINPUT_GAMEPAD_LEFT_THUMB);mappedButton("horse.right_click",XINPUT_GAMEPAD_RIGHT_THUMB);
            pad.leftTrigger=triggerValue("equipment.open");
            pad.rightTrigger=right&&controllerFrame.hands[1].aimTracked?triggerValue("gameplay.fire_or_cqc"):0;
        }
        if(rigInput&&controlContext==ControlContext::equipment){
            mappedButton("equipment.back",XINPUT_GAMEPAD_B);mappedButton("equipment.use",XINPUT_GAMEPAD_A);
            if(rigControls.equipmentPhase()==1){
                navigation={activeControl("equipment.support")?1.f:activeControl("equipment.items")?-1.f:0.f,
                    activeControl("equipment.primary")?1.f:activeControl("equipment.secondary")?-1.f:0.f};
            }
        }
        pad.buttons|=menuBits;
        if(!nativeInput.exclusive){
            pad.leftX=static_cast<int16_t>(move[0]*32767);pad.leftY=static_cast<int16_t>(move[1]*32767);
            pad.rightX=static_cast<int16_t>(navigation[0]*32767);pad.rightY=static_cast<int16_t>(navigation[1]*32767);
        }
        if(utilityCenter||headToggle)pad={};
        bool stickNavigation=false;
        bool locomotionAvailable=false;
        // The handheld optic owns zoom and trigger input. Native waypoint
        // placement consumes its aim on the game's marker-update job.
        if(nativeInput.exclusive){
            rigControls.suspend();commandsControls.suspend();opticsControls.reset();opticGate.reset();
            controllerFrame.weaponReady=rigInput&&mode!=TravelMode::vehicle&&pad.leftTrigger>127;
            controllerFrame.vehicleControls=rigInput&&mode==TravelMode::vehicle;
            controllerFrame.commandControls=rigInput&&(pad.buttons&XINPUT_GAMEPAD_LEFT_SHOULDER);
            controllerFrame.equipmentCategory=(pad.buttons&1)?1:(pad.buttons&2)?2:(pad.buttons&8)?3:(pad.buttons&4)?4:0;
            controllerFrame.equipmentOpen=controllerFrame.equipmentCategory!=0;
            controllerFrame.allowMotionMelee=controllerFrame.allowAnimalTouch=false;
            stickNavigation=true;
        }else if(rigInput){
            GamepadSample opticPad{menuBits,0,0,pad.leftX,pad.leftY};
            if(activeControl("binoculars.zoom"))opticPad.buttons|=XINPUT_GAMEPAD_RIGHT_THUMB;
            if(activeControl("binoculars.mark"))opticPad.buttons|=XINPUT_GAMEPAD_X;
            if(activeControl("binoculars.clear_mark"))opticPad.rightY=-32767;
            const auto optical=opticsControls.update(binocularSelected?opticPad:pad,mode==TravelMode::onFoot&&!commandsControls.active()&&!center&&!headToggle,
                binocularSelected,controllerFrame.optic.active);
            if(optical.markRequested&&controllerFrame.optic.pose.ray.tracked)++opticMarkSequence;
            if(optical.clearRequested&&controllerFrame.optic.pose.ray.tracked)++opticClearSequence;
            controllerFrame.opticMarkSequence=opticMarkSequence;
            controllerFrame.opticClearSequence=opticClearSequence;
            const auto wasCommands=commandsControls.active();
            GamepadSample commandPad{menuBits,triggerValue(mode==TravelMode::horse?"commands.mounted_keep_open":"commands.keep_open"),
                static_cast<uint8_t>(activeControl("commands.confirm")?255:0),pad.leftX,pad.leftY};
            if(activeControl("commands.open")||activeControl("commands.mounted_open"))commandPad.buttons|=XINPUT_GAMEPAD_X;
            if(activeControl("commands.back"))commandPad.buttons|=XINPUT_GAMEPAD_B;
            const auto commandAxis=controls.axis("axes.commands",physical);
            commandPad.rightX=static_cast<int16_t>(commandAxis[0]*32767);commandPad.rightY=static_cast<int16_t>(commandAxis[1]*32767);
            const bool carryCombat=mode==TravelMode::onFoot&&right&&controllerFrame.hands[1].aimTracked
                &&!activeControl("gameplay.dive")&&!activeControl("gameplay.stance")&&!activeControl("gameplay.pickup_carry");
            const auto commands=commandsControls.update(commandPad,(mode==TravelMode::onFoot||mode==TravelMode::horse)
                &&!optical.exclusive&&!center&&!headToggle,now,nativeCommandsDrawTime(),
                carryCombat?triggerValue("gameplay.ready_weapon"):0,carryCombat?triggerValue("gameplay.fire_or_cqc"):0);
            if(wasCommands!=commands.active)log("Wrist Commands open="+std::to_string(commands.active));
            controllerFrame.commandControls=commands.active;
            // One hand gets the full lens resolution and 2x/4x power. The
            // other hand is optional physical support, never a zoom unlock.
            const float effectiveMagnification=optical.magnification;
            controllerFrame.magnification=effectiveMagnification;
            if(reportedMagnification!=effectiveMagnification){
                reportedMagnification=effectiveMagnification;log("Stereo magnification="+std::to_string(effectiveMagnification));
            }
            const auto beforePhase=rigControls.equipmentPhase();
            RigInputSample mapped{};
            if(optical.exclusive||commands.exclusive){
                rigControls.reset();mapped.gamepad=optical.exclusive?optical.gamepad:commands.gamepad;
                mapped.weaponReady=!optical.exclusive&&commands.weaponReady;
                // A trigger pressed inside Commands must not become a shot on
                // close. Only an uninterrupted pre-existing CQC hold survives.
                if(commands.exclusive&&!commands.gamepad.rightTrigger)rigControls.requireAttackRelease();
            }else mapped=rigControls.update(pad,activeControl("vehicle.wheel_grip")&&!center&&!headToggle,
                mode==TravelMode::vehicle?equipmentHeld:right&&controllerFrame.hands[1].aimTracked&&activeControl("gameplay.ready_weapon"),mode,
                now,nativeEquipmentPickerDrawTime(),controllerThrowReady());
            if(beforePhase!=rigControls.equipmentPhase())log("Wrist picker phase="+std::to_string(rigControls.equipmentPhase())
                +" category="+std::to_string(rigControls.equipmentCategory()));
            pad=mapped.gamepad;controllerFrame.weaponReady=mapped.weaponReady;
            controllerFrame.supportGrip=activeControl("gameplay.support_grip");
            controllerFrame.vehicleControls=mode==TravelMode::vehicle;
            controllerFrame.wheelGrip=activeControl("vehicle.wheel_grip")&&!center&&!headToggle;
            controllerFrame.allowMotionMelee=controls.setting("settings.motion_melee")!=0;
            controllerFrame.allowAnimalTouch=controls.setting("settings.animal_touch")!=0;
            controllerFrame.equipmentOpen=rigControls.equipmentPhase()!=0;
            controllerFrame.equipmentCategory=rigControls.equipmentPhase()>=2?rigControls.equipmentCategory()+1:0;
            stickNavigation=rigControls.equipmentPhase()!=0||commands.exclusive
                ||(pad.buttons&(XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_BACK));
            locomotionAvailable=mode==TravelMode::onFoot&&!stickNavigation
                &&!commands.exclusive&&!center&&!headToggle&&!utilityCenter;
            // Native on-foot movement already consumes the published camera
            // basis. Rotating this stick again doubles physical/snap heading.
        }else if(nativeStatus.nativeMenuOpen||nativeStatus.awaitingPlayer){rigControls.suspend();opticsControls.reset();opticGate.reset();commandsControls.suspend();}
        else {rigControls.reset();opticsControls.reset();opticGate.reset();commandsControls.suspend();}
        if(locomotionAvailable){
            const bool loweringAction=activeControl("gameplay.dive")||activeControl("binoculars.dive")
                ||activeControl("gameplay.stance")||activeControl("binoculars.stance")||activeControl("gameplay.pickup_carry");
            if(loweringAction)rigControls.requireAttackRelease();
            applyOnFootActions(pad,controllerFrame.weaponReady,{
                activeControl("gameplay.run")||activeControl("binoculars.run"),
                activeControl("gameplay.stance")||activeControl("binoculars.stance"),
                activeControl("gameplay.dive")||activeControl("binoculars.dive"),
                activeControl("gameplay.pickup_carry"),activeControl("gameplay.switch_weapon")});
        }
        if(rigInput&&mode==TravelMode::onFoot&&!stickNavigation&&!binocularSelected)
            commandsControls.observeGameplay(pad,controllerFrame.weaponReady);
        else if(!rigInput||mode!=TravelMode::onFoot||controllerFrame.equipmentOpen||binocularSelected)
            commandsControls.observeGameplay({},false);
        // A configured press spans several XR frames. Count its rising edge,
        // not every active frame (nine frames at 90 Hz wrapped a 3-power sight).
        // Native R3 is not forwarded: it also changes the desktop camera.
        controllerFrame.weaponZoomSequence=weaponZoomInput.update(activeControl("gameplay.zoom"),
            locomotionAvailable&&controllerFrame.weaponReady);
        if(rigInput&&!nativeInput.exclusive&&!stickNavigation){pad.rightX=0;pad.rightY=0;}
        const bool turnAvailable=rigInput&&!nativeInput.exclusive&&!stickNavigation&&!center&&!headToggle&&!utilityCenter;
        const auto turnMode=controls.setting("settings.turn_mode");
        const auto smoothAxis=controls.axis("axes.turn",physical);
        // Mounted locomotion owns a native vehicle/horse heading. An artificial
        // head-camera-only snap decouples that heading from what the rider sees.
        const bool mounted=mode==TravelMode::horse||mode==TravelMode::vehicle;
        const bool nativeTurn=turnMode==1||(mounted&&turnMode==0);
        const auto smooth=smoothControls.update(smoothAxis[0],smoothAxis[1],turnAvailable&&nativeTurn);
        if(turnAvailable&&nativeTurn)pad.rightX=smooth;
        const float turnAxis=activeControl("turn.right")?1.f:activeControl("turn.left")?-1.f:0.f;
        const auto turn=snapControls.update(turnAxis,0,turnAvailable&&turnMode==0&&!mounted)
            *controls.setting("settings.snap_turn_degrees")/30.f;
        if(!nativeStatus.active&&!nativeStatus.pending&&!nativeStatus.awaitingPlayer)snapYaw=0;
        if(mounted)snapYaw=0;
        if(turn){snapYaw=std::remainder(snapYaw+turn,6.283185307f);log("Physical snap turn degrees="+std::to_string(-turn*57.2957795f));}
        controllerFrame.snapYaw=snapYaw;
        gamepadMailbox().publish(pad,true,steadyMilliseconds());
        const auto hapticNow=steadyMilliseconds();
        const auto wheel=wheelMailbox().read(hapticNow);
        const bool holding=rigInput&&controllerFrame.vehicleControls&&wheel.gripped&&controllerFrame.wheelGrip;
        const bool tookWheel=holding&&!wheelHeld;wheelHeld=holding;
        if(tookWheel||binocularEquipped||nativeInput.changed||hapticNow-hapticAt>=50){
            hapticAt=hapticNow;
            const auto rumble=rumbleMailbox().read(hapticNow);
            for(size_t side=0;side<2;++side){
                const float native=side?rumble.high:rumble.low;
                const float contact=nativeInput.changed?.4f:side&&binocularEquipped?.3f:!side&&tookWheel?.25f:0.f;
                const float amplitude=std::clamp(std::max(native,contact),0.f,.75f);
                XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=vibration;info.subactionPath=hands[side];
                if(amplitude>0&&controllerFrame.hands[side].gripTracked){
                    XrHapticVibration pulse{XR_TYPE_HAPTIC_VIBRATION};pulse.amplitude=amplitude;
                    pulse.duration=60*1000*1000;pulse.frequency=XR_FREQUENCY_UNSPECIFIED;
                    xrApplyHapticFeedback(handle,&info,reinterpret_cast<const XrHapticBaseHeader*>(&pulse));
                }else xrStopHapticFeedback(handle,&info);
            }
        }
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
    bool pending{},waited{},ready{};
    FrameId copied{};
    explicit Screen(Session& s):session(s){}
    ~Screen(){reset();}
    void reset(){if(handle)xrDestroySwapchain(handle);handle=XR_NULL_HANDLE;images.clear();pending=waited=ready=false;copied={};}
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
    bool prepare(ID3D11Texture2D* source,FrameId id,uint32_t sourceSlice=0){
        if(!source||!id.sequence)return false;
        D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
        if(sourceSlice>=desc.ArraySize)throw std::invalid_argument("Missing native eye texture slice");
        if(!handle||desc.Width!=width||desc.Height!=height||desc.Format!=sourceFormat||(copied.epoch!=id.epoch&&ready))create(desc);
        if(ready&&copied==id)return true;
        if(!pending){
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrCheck(xrAcquireSwapchainImage(handle,&ai,&pendingIndex),"Acquire XR image");pending=true;waited=false;
        }
        if(waited)return true;
        // A busy swapchain must not block controller/head sampling for two
        // consecutive 50 ms eye waits. Keep the acquisition and retry next frame.
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=0;
        const auto r=xrWaitSwapchainImage(handle,&wait);
        if(r==XR_TIMEOUT_EXPIRED)return false; // Retain acquisition for the next XR frame.
        xrCheck(r,"Wait XR image");
        waited=true;
        return true;
    }
    void publishPrepared(ID3D11Texture2D* source,FrameId id,uint32_t sourceSlice=0){
        if(ready&&copied==id)return;
        if(!pending||!waited)throw std::logic_error("XR image publication requires a waited acquisition");
        session.context->CopySubresourceRegion(images.at(pendingIndex).texture,0,0,0,0,source,sourceSlice,nullptr);
        session.context->Flush();
        checkHr(session.device->GetDeviceRemovedReason(),"XR device health");
        XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrCheck(xrReleaseSwapchainImage(handle,&release),"Release XR image");
        pending=waited=false;ready=true;copied=id;
    }
    void upload(ID3D11Texture2D* source,FrameId id,uint32_t sourceSlice=0){
        if(prepare(source,id,sourceSlice))publishPrepared(source,id,sourceSlice);
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
        uint32_t count{};
        xrCheck(xrEnumerateViewConfigurationViews(instance.handle,instance.system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0,&count,nullptr),"Count stereo view recommendations");
        if(count!=2)throw std::runtime_error("Runtime did not expose two primary stereo views");
        std::array<XrViewConfigurationView,2> views{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
        xrCheck(xrEnumerateViewConfigurationViews(instance.handle,instance.system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            count,&count,views.data()),"Read stereo view recommendations");
        result.recommendedWidth=std::max(views[0].recommendedImageRectWidth,views[1].recommendedImageRectWidth);
        result.recommendedHeight=std::max(views[0].recommendedImageRectHeight,views[1].recommendedImageRectHeight);
        result.maximumWidth=std::min(views[0].maxImageRectWidth,views[1].maxImageRectWidth);
        result.maximumHeight=std::min(views[0].maxImageRectHeight,views[1].maxImageRectHeight);
    }catch(const std::exception& e){result.error=e.what();}
    return result;
}

RuntimeStats runTheatre(TextureMailbox& source,const TheatreConfig& config,const std::atomic_bool& stop,std::chrono::seconds duration){
    if(!std::isfinite(config.widthMeters)||config.widthMeters<1||config.widthMeters>30
        ||!std::isfinite(config.distanceMeters)||config.distanceMeters<1||config.distanceMeters>30)
        throw std::invalid_argument("Theatre dimensions must be finite values from 1 to 30 meters");
    Instance instance;instance.getSystem();Session session(instance);
    if(!config.controlsPath.empty()){
        session.controlsPath=config.controlsPath;
        std::ifstream file(config.controlsPath);
        if(file){
            const auto errors=session.controls.load(file);
            if(errors.empty())log("Controls loaded: "+config.controlsPath.string());
            else {log("Controls file rejected; using built-in defaults");for(const auto& error:errors)log("Controls: "+error);}
        }else log("Controls file absent; using built-in defaults: "+config.controlsPath.string());
        std::error_code error;
        const auto write=std::filesystem::last_write_time(config.controlsPath,error);
        if(!error)session.observedControlsWrite=session.handledControlsWrite=write;
    }
    session.initialize();
    log("OpenXR runtime="+instance.runtime+" headset="+instance.properties.systemName);
    TextureConsumer consumer(session.device.Get());Screen screen(session),leftEye(session),rightEye(session);RuntimeStats stats;
    NativeVideoRecorder video;
    const std::array<Screen*,2> eyeScreens{&leftEye,&rightEye};
    std::array<EyeFrame,2> eyeFrames{},surroundFrames{};
    bool haveSurround{},surroundTransition{};
    uint64_t eyeEpoch{},projectionFrames{},emptyStereoFrames{},retainedStereoFrames{};
    uint64_t performanceAt{},windowSubmissions{},windowNewPairs{},windowEmpty{},lastProjectionSource{};
    bool priorEmptyStereo{},layoutLogged{};
    Pose screenPose{};bool anchored=false,menuReturnPending=false;
    const auto start=std::chrono::steady_clock::now();
    bool closing=false;
    std::chrono::steady_clock::time_point closeDeadline{};
    while(!session.exiting){
        const auto cycleStart=steadyMilliseconds();
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
        const auto waitDone=steadyMilliseconds();
        XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};xrCheck(xrBeginFrame(session.handle,&bi),"Begin XR frame");
        EndFrameGuard guard{session.handle,frame.predictedDisplayTime};++stats.frames;
        // Keep paired empty frames until STOPPING; do not publish more native input during exit.
        if(closing)continue;
        if(session.pendingLocalChange&&frame.predictedDisplayTime>=session.pendingLocalChange){
            session.pendingLocalChange=0;session.recenterRequested=true;anchored=false;
            ++session.referenceEpoch;
        }
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
        auto trackedHead=fromXr(head.pose);
        session.syncInput(frame.predictedDisplayTime,trackedHead,trackedViews,tracking&&stereoTracked);
        if(!stats.haveViews&&tracking&&stereoTracked){
            stats.firstHead=trackedHead;stats.firstViews=trackedViews;stats.haveViews=true;
        }
        headCamera().trackStereo(trackedHead,trackedViews,tracking&&stereoTracked&&session.focused,steadyMilliseconds(),session.controllerFrame);
        const auto trackingDone=steadyMilliseconds();
        if(tracking&&(!anchored||session.recenterRequested)){
            screenPose=recenteredScreen(fromXr(head.pose),config.distanceMeters);anchored=true;session.recenterRequested=false;
        }
        if(!tracking)++stats.trackingInvalidFrames;
        auto channel=source.latest();
        if(channel&&!sameLuid(channel->adapterLuid,session.luid))throw std::runtime_error("Game and headset use different GPUs; shared capture is unavailable");
        const bool fresh=consumer.consume(channel);
        const auto consumeDone=steadyMilliseconds();
        if(fresh)++stats.sourceFrames;
        if(eyeEpoch!=consumer.frame().epoch){eyeFrames={};eyeEpoch=consumer.frame().epoch;layoutLogged=false;}
        const auto cameraStatus=headCamera().status();
        // Once the player has entered stereo, native loading screens and
        // noninteractive surfaces live in front of the last accepted stereo
        // surroundings. Keep their original eye poses: this is an explicitly
        // frozen scene, not an old frame relabeled as current gameplay.
        if(!mayRetainStereoSurround(cameraStatus)){
            if(surroundTransition)screenPose=recenteredScreen(trackedHead,config.distanceMeters);
            haveSurround=false;surroundFrames={};surroundTransition=false;
        }
        if(session.manualScreenSelected)surroundTransition=false;
        else if(haveSurround&&!cameraStatus.active&&!surroundTransition){
            surroundTransition=true;screenPose=recenteredScreen(trackedHead,1.3f);
        }
        if(cameraStatus.awaitingPlayer)menuReturnPending=true;
        else if(!cameraStatus.active&&!cameraStatus.pending)menuReturnPending=false;
        if(frame.shouldRender&&consumer.frame().sequence){
            if(!cameraStatus.active&&!cameraStatus.pending){screen.upload(consumer.texture(),consumer.frame());eyeFrames={};}
            else if(cameraStatus.active&&!cameraStatus.suspended){
                const auto metadata=consumer.eyes();
                if(readyEyePair(metadata,cameraStatus.activation,steadyMilliseconds())){
                    // Acquire/wait BOTH eyes before releasing either new image.
                    // Backpressure retains the preceding complete pair instead
                    // of advancing one swapchain and invalidating that pair.
                    const bool leftReady=leftEye.prepare(consumer.texture(),consumer.frame(),0);
                    const bool rightReady=rightEye.prepare(consumer.texture(),consumer.frame(),1);
                    if(leftReady&&rightReady){
                        for(size_t n=0;n<2;++n)eyeScreens[n]->publishPrepared(consumer.texture(),consumer.frame(),static_cast<uint32_t>(n));
                        eyeFrames=metadata;
                        surroundFrames=metadata;haveSurround=true;surroundTransition=false;
                    }
                }
            }
        }
        const auto uploadDone=steadyMilliseconds();
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        quad.space=session.local;quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;quad.pose=toXr(screenPose);
        quad.subImage.swapchain=screen.handle;
        quad.subImage.imageRect.extent={static_cast<int32_t>(screen.width),static_cast<int32_t>(screen.height)};
        const float panelWidth=surroundTransition?1.6f:config.widthMeters;
        quad.size={panelWidth,screen.width?panelWidth*static_cast<float>(screen.height)/static_cast<float>(screen.width):1};
        const auto* layer=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
        std::array<XrCompositionLayerProjectionView,2> projectionViews{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
        XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};projection.space=session.local;
        projection.viewCount=2;projection.views=projectionViews.data();
        bool regionsValid=true;
        for(size_t n=0;n<2;++n){auto& v=projectionViews[n];const auto& sourceEye=surroundTransition?surroundFrames[n]:eyeFrames[n];
            // The centered source covers the full native sky. Select this
            // eye's requested angular region without stretching or changing
            // its rays. Optical zoom is already inside the world-space lens.
            const auto region=eyeImageRegion(sourceEye.view.fov,sourceEye.displayFov,
                eyeScreens[n]->width,eyeScreens[n]->height);
            if(!region){regionsValid=false;continue;}
            v.pose=toXr(sourceEye.view.pose);v.fov={region->fov.left,region->fov.right,
                region->fov.up,region->fov.down};
            v.subImage.swapchain=eyeScreens[n]->handle;
            v.subImage.imageRect.offset={region->x,region->y};
            v.subImage.imageRect.extent={region->width,region->height};
            if(!layoutLogged)log("XR eye "+std::to_string(n)+" image crop="
                +std::to_string(region->x)+","+std::to_string(region->y)+","
                +std::to_string(region->width)+"x"+std::to_string(region->height));
        }
        if(regionsValid)layoutLogged=true;
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        const std::array<const XrCompositionLayerBaseHeader*,2> transitionLayers{
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection),
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad)};
        if(frame.shouldRender&&tracking){
            if(surroundTransition&&haveSurround&&regionsValid&&leftEye.ready&&rightEye.ready){
                end.layerCount=screen.ready?2u:1u;end.layers=transitionLayers.data();
                ++projectionFrames;++retainedStereoFrames;
            }else if(cameraStatus.active||cameraStatus.pending){
                // Runtime wait/end calls can stall beyond the native tracking
                // freshness window. Reproject the last ACCEPTED complete pair
                // for at most 500 ms while current headset tracking is valid.
                // Keep its original poses/FOVs; never relabel old pixels with
                // current tracking or use this allowance to accept new sources.
                const bool recoverable=!cameraStatus.suspended||cameraStatus.reason==HeadCameraStop::staleTracking;
                const bool freshEyes=readyEyePair(eyeFrames,cameraStatus.activation,steadyMilliseconds());
                if(cameraStatus.active&&recoverable&&regionsValid&&readyEyePair(eyeFrames,cameraStatus.activation,steadyMilliseconds(),500)){
                    layer=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);end.layerCount=1;end.layers=&layer;++projectionFrames;
                    if(!freshEyes||cameraStatus.suspended)++retainedStereoFrames;
                    menuReturnPending=false;
                }else if(menuReturnPending&&cameraStatus.active&&!cameraStatus.suspended&&anchored&&screen.ready){
                    // Keep the last native menu quad until this return has a
                    // complete new stereo pair. Never use old gameplay eyes or
                    // fall back to a screen after gameplay tracking is lost.
                    end.layerCount=1;end.layers=&layer;++stats.submittedScreens;
                }
            }else if(anchored&&screen.ready){end.layerCount=1;end.layers=&layer;++stats.submittedScreens;}
        }
        xrCheck(xrEndFrame(session.handle,&end),"Submit XR frame");guard.ended=true;
        const bool recordedStereo=end.layerCount&&layer==reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
        const bool recordable=!surroundTransition&&(recordedStereo?consumer.frame().sequence&&consumer.eyes()[1].sourceSequence==eyeFrames[1].sourceSequence
            :end.layerCount&&consumer.frame().sequence&&!cameraStatus.active&&!cameraStatus.pending);
        video.frame(session.device.Get(),session.context.Get(),recordable?consumer.texture():nullptr,recordedStereo?1u:0u,
            recordedStereo?&consumer.eyes()[1]:nullptr);
        const auto cycleEnd=steadyMilliseconds();
        if(cycleEnd-cycleStart>100)log("XR slow frame ms: wait="+std::to_string(waitDone-cycleStart)
            +" tracking="+std::to_string(trackingDone-waitDone)+" consume="+std::to_string(consumeDone-trackingDone)
            +" upload="+std::to_string(uploadDone-consumeDone)+" submit="+std::to_string(cycleEnd-uploadDone));
        const bool emptyStereo=frame.shouldRender&&tracking&&cameraStatus.active&&!end.layerCount;
        if(!performanceAt)performanceAt=cycleEnd;
        if(cameraStatus.active&&frame.shouldRender){
            ++windowSubmissions;
            if(emptyStereo)++windowEmpty;
            if(end.layerCount&&layer==reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection)
               &&eyeFrames[0].sourceSequence!=lastProjectionSource){++windowNewPairs;lastProjectionSource=eyeFrames[0].sourceSequence;}
        }
        if(cycleEnd-performanceAt>=5000){
            const double seconds=static_cast<double>(cycleEnd-performanceAt)/1000.0;
            if(windowSubmissions)log("XR performance runtime_hz="+std::to_string(frame.predictedDisplayPeriod>0?1e9/static_cast<double>(frame.predictedDisplayPeriod):0)
                +" submissions_fps="+std::to_string(static_cast<double>(windowSubmissions)/seconds)
                +" new_pairs_fps="+std::to_string(static_cast<double>(windowNewPairs)/seconds)
                +" repeated_or_empty="+std::to_string(windowSubmissions-windowNewPairs)+" empty="+std::to_string(windowEmpty));
            performanceAt=cycleEnd;windowSubmissions=windowNewPairs=windowEmpty=0;
        }
        if(emptyStereo){
            ++emptyStereoFrames;
            if(!priorEmptyStereo){
                const auto tick=steadyMilliseconds();
                log("XR stereo waiting: suspended="+std::to_string(cameraStatus.suspended)+" regions="+std::to_string(regionsValid)
                    +" source="+std::to_string(eyeFrames[0].sourceSequence)+","+std::to_string(eyeFrames[1].sourceSequence)
                    +" age_ms="+std::to_string(tick>=eyeFrames[0].sampleTime?tick-eyeFrames[0].sampleTime:0)
                    +" activation="+std::to_string(cameraStatus.activation));
            }
        }
        priorEmptyStereo=emptyStereo;
        if(stats.frames%300==0)log("XR frames="+std::to_string(stats.frames)+" theatre submissions="+std::to_string(stats.submittedScreens)
            +" projection submissions="+std::to_string(projectionFrames)+" captured frames="+std::to_string(stats.sourceFrames)
            +" empty stereo="+std::to_string(emptyStereoFrames)+" retained stereo="+std::to_string(retainedStereoFrames));
    }
    log("OpenXR frame loop stopped; releasing graphics resources");
    return stats;
}
}
