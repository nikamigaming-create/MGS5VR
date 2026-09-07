#include "mgs5vr/gpu_timing.hpp"

namespace mgs5vr {
bool GpuTiming::sameDevice(ID3D11DeviceContext* context) const noexcept {
    if(!context)return false;
    ComPtr<ID3D11Device> device;context->GetDevice(&device);return device.Get()==device_.Get();
}
bool GpuTiming::begin(ID3D11DeviceContext* context) noexcept {
    if(!context||pending_)return false;
    ComPtr<ID3D11Device> device;context->GetDevice(&device);
    if(device.Get()!=device_.Get()){clock_.Reset();start_.Reset();finish_.Reset();device_=device;}
    if(!clock_){
        D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
        if(FAILED(device_->CreateQuery(&desc,&clock_)))return false;
        desc.Query=D3D11_QUERY_TIMESTAMP;
        if(FAILED(device_->CreateQuery(&desc,&start_))||FAILED(device_->CreateQuery(&desc,&finish_))){clock_.Reset();start_.Reset();finish_.Reset();return false;}
    }
    pending_=true;started_=ended_=false;
    if(context->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE)beginExecution(context);
    context->End(start_.Get());return true;
}
void GpuTiming::end(ID3D11DeviceContext* context) noexcept {
    if(!pending_||!sameDevice(context))return;
    context->End(finish_.Get());
    if(context->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE)endExecution(context);
}
void GpuTiming::beginExecution(ID3D11DeviceContext* context) noexcept {
    if(!pending_||started_||!sameDevice(context)||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    context->Begin(clock_.Get());started_=true;
}
void GpuTiming::endExecution(ID3D11DeviceContext* context) noexcept {
    if(!started_||ended_||!sameDevice(context)||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    context->End(clock_.Get());ended_=true;
}
std::optional<double> GpuTiming::poll(ID3D11DeviceContext* context) noexcept {
    if(!pending_||!ended_||!sameDevice(context)||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return {};
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock{};UINT64 start{},finish{};
    const auto result=context->GetData(clock_.Get(),&clock,sizeof(clock),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(result==S_FALSE)return {};
    if(FAILED(result)||clock.Disjoint||!clock.Frequency){pending_=false;return {};}
    const auto first=context->GetData(start_.Get(),&start,sizeof(start),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    const auto last=context->GetData(finish_.Get(),&finish,sizeof(finish),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(first==S_FALSE||last==S_FALSE)return {};
    pending_=false;
    if(SUCCEEDED(first)&&SUCCEEDED(last)&&finish>=start)return 1000.0*static_cast<double>(finish-start)/static_cast<double>(clock.Frequency);
    return {};
}
bool GpuTiming::pending() const noexcept {return pending_;}
}
