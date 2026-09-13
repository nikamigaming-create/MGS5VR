#include "mgs5vr/mailbox.hpp"
#include "mgs5vr/optic_renderer.hpp"
#include "mgs5vr/optic_markers.hpp"
#include "mgs5vr/input_bridge.hpp"
#include <iostream>
#include <stdexcept>

int opticRendererChecks(){
    using namespace mgs5vr;
    try{
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
        const D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
        checkHr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,D3D11_SDK_VERSION,
            &device,nullptr,&context),"Create asset-free optic fixture");
        D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=128;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> target,scene;ComPtr<ID3D11RenderTargetView> targetView,sceneView;
        checkHr(device->CreateTexture2D(&desc,nullptr,&target),"Create optic target");
        checkHr(device->CreateTexture2D(&desc,nullptr,&scene),"Create optic scene");
        checkHr(device->CreateRenderTargetView(target.Get(),nullptr,&targetView),"Create optic target view");
        checkHr(device->CreateRenderTargetView(scene.Get(),nullptr,&sceneView),"Create optic source view");
        const float blue[]{0,0,1,1},green[]{0,1,0,1};
        context->ClearRenderTargetView(targetView.Get(),blue);context->ClearRenderTargetView(sceneView.Get(),green);
        auto* bound=targetView.Get();context->OMSetRenderTargets(1,&bound,nullptr);
        const D3D11_VIEWPORT viewport{0,0,128,128,0,1};context->RSSetViewports(1,&viewport);
        // Native-style +Z view with an ocular normal toward the eye. The
        // fixture tests real shader compilation, pixels and restored state;
        // it is not a substitute for the equipped game's sight geometry.
        const std::array<float,16> world{1,0,0,0,0,1,0,0,0,0,-1,0,0,0,.1f,1};
        const std::array<float,16> view{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const std::array<float,16> projection{2,0,0,0,0,2,0,0,0,0,0,1,0,0,.03f,0};
        if(!drawPhysicalWeaponScope(context.Get(),world,view,projection,.02f,4,scene.Get()))
            throw std::runtime_error("Weapon lens shaders or draw failed");
        ComPtr<ID3D11RenderTargetView> restored;ComPtr<ID3D11DepthStencilView> restoredDepth;
        context->OMGetRenderTargets(1,&restored,&restoredDepth);
        if(restored.Get()!=bound||restoredDepth)throw std::runtime_error("Optic changed native target/depth binding");
        desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> readback;checkHr(device->CreateTexture2D(&desc,nullptr,&readback),"Create optic readback");
        context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        checkHr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"Read optic pixels");
        const auto sample=[&](UINT x,UINT y){
            const auto* p=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
            return std::array<unsigned char,4>{p[0],p[1],p[2],p[3]};
        };
        const bool image=sample(74,54)[1]>240&&sample(74,54)[2]<10;
        const bool outside=sample(20,20)==std::array<unsigned char,4>{0,0,255,255};
        const bool reticle=sample(64,54)[1]<180;
        context->Unmap(readback.Get(),0);stopPhysicalOpticRenderer();
        if(!image||!outside||!reticle)throw std::runtime_error("Weapon lens image, aperture or reticle pixels are incorrect");
        headCamera().configure(true);headCamera().track({},true,steadyMilliseconds());headCamera().toggle();
        const auto frame=headCamera().resolveCurrent(1,{});
        OpticWaypoints markers;markers.activation=frame.activation;markers.count=markers.points.size();
        for(auto& marker:markers.points)marker={{0,0,3},1};
        const auto markerCenter=[&](float translation,bool expired){
            context->ClearRenderTargetView(targetView.Get(),blue);
            auto shifted=view;shifted[12]=translation;
            markers.sampleTime=steadyMilliseconds()-(expired?1000:0);
            drawWorldWaypoints(context.Get(),shifted,projection,{},markers);
            restored.Reset();restoredDepth.Reset();context->OMGetRenderTargets(1,&restored,&restoredDepth);
            if(restored.Get()!=bound||restoredDepth)throw std::runtime_error("World markers changed native target/depth binding");
            context->CopyResource(readback.Get(),target.Get());
            checkHr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"Read world marker pixels");
            unsigned count{};double total{};
            for(UINT y=0;y<128;++y)for(UINT x=0;x<128;++x){const auto pixel=sample(x,y);
                if(pixel[0]>180&&pixel[1]>100&&pixel[2]<100){++count;total+=x;}
            }
            const bool clearCorner=sample(4,4)==std::array<unsigned char,4>{0,0,255,255};
            context->Unmap(readback.Get(),0);
            if(!clearCorner)throw std::runtime_error("World marker draw touched unrelated background");
            return count?total/count:-1.;
        };
        const auto left=markerCenter(.3f,false),right=markerCenter(-.3f,false),stale=markerCenter(0,true);
        headCamera().configure(false);stopPhysicalOpticRenderer();
        if(left<0||right<0||left-right<20||stale!=-1)
            throw std::runtime_error("World marker projection, crowded marker upload or stale-source rejection failed");
        std::cout<<"Asset-free scope shader compilation, aperture/reticle pixels and target restoration passed.\n";
        std::cout<<"World marker per-eye motion, crowded uploads, stale-source rejection and target restoration passed.\n";
        return 0;
    }catch(const std::exception& e){
        stopPhysicalOpticRenderer();std::cerr<<"Optic renderer: "<<e.what()<<'\n';return 1;
    }
}
