#pragma once
#include "controls_editor.hpp"
#include <memory>
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
namespace mgs5vr {
struct CabinStudioFrame;
class ControlsPanelRenderer {
public:
    ControlsPanelRenderer();
    ~ControlsPanelRenderer();
    ID3D11Texture2D* draw(ID3D11Device* device,ID3D11DeviceContext* context,
        const ControlsEditorFrame& frame,const std::filesystem::path& game);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
class NativeTextPanelRenderer {
public:
    NativeTextPanelRenderer();
    ~NativeTextPanelRenderer();
    ID3D11Texture2D* draw(ID3D11Device* device,ID3D11DeviceContext* context,const CabinStudioFrame& frame);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
class NativeCanvasRenderer {
public:
    NativeCanvasRenderer();
    ~NativeCanvasRenderer();
    ID3D11Texture2D* crop(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
