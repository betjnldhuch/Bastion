#include "overlay.h"

#include <cstring>
#include <vector>

#include "browser.h"
#include "log.h"

namespace overlay {
namespace {
IDirect3DTexture9* g_tex = nullptr;
IDirect3DStateBlock9* g_state = nullptr;
int g_texW = 0, g_texH = 0;
int g_bbW = 0, g_bbH = 0;
bool g_hasFrame = false;
int g_lastW = 0, g_lastH = 0;  // размер последнего кадра в g_buf (после Reset заливаем заново)
std::vector<uint8_t> g_buf;

struct Vertex {
    float x, y, z, rhw;
    float u, v;
};

void Release() {
    if (g_tex) g_tex->Release();
    if (g_state) g_state->Release();
    g_tex = nullptr;
    g_state = nullptr;
    g_texW = g_texH = 0;
    g_hasFrame = false;
}

bool Upload(IDirect3DDevice9* dev, int w, int h) {
    if (!g_tex || g_texW != w || g_texH != h) {
        if (g_tex) g_tex->Release();
        g_tex = nullptr;
        if (FAILED(dev->CreateTexture(w, h, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_tex, nullptr))) {
            Log("CreateTexture %dx%d failed", w, h);
            return false;
        }
        g_texW = w;
        g_texH = h;
    }
    D3DLOCKED_RECT lr;
    if (FAILED(g_tex->LockRect(0, &lr, nullptr, D3DLOCK_DISCARD))) return false;
    const size_t row = static_cast<size_t>(w) * 4;
    for (int y = 0; y < h; y++)
        memcpy(static_cast<uint8_t*>(lr.pBits) + static_cast<size_t>(y) * lr.Pitch, g_buf.data() + y * row, row);
    g_tex->UnlockRect(0);
    return true;
}

void Draw(IDirect3DDevice9* dev) {
    if (!g_state && FAILED(dev->CreateStateBlock(D3DSBT_ALL, &g_state))) return;
    g_state->Capture();

    dev->SetPixelShader(nullptr);
    dev->SetVertexShader(nullptr);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    dev->SetTexture(0, g_tex);
    dev->SetTexture(1, nullptr);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);  // CEF отдаёт альфу, уже умноженную на цвет
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    const float w = static_cast<float>(g_texW) - 0.5f, h = static_cast<float>(g_texH) - 0.5f;
    Vertex q[4] = {
        {-0.5f, -0.5f, 0.f, 1.f, 0.f, 0.f},
        {w, -0.5f, 0.f, 1.f, 1.f, 0.f},
        {-0.5f, h, 0.f, 1.f, 0.f, 1.f},
        {w, h, 0.f, 1.f, 1.f, 1.f},
    };
    if (SUCCEEDED(dev->BeginScene())) {
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(Vertex));
        dev->EndScene();
    }
    g_state->Apply();
}
}  // namespace

void Present(IDirect3DDevice9* dev, bool visible) {
    IDirect3DSurface9* bb = nullptr;
    if (SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb) {
        D3DSURFACE_DESC d;
        if (SUCCEEDED(bb->GetDesc(&d))) {
            g_bbW = static_cast<int>(d.Width);
            g_bbH = static_cast<int>(d.Height);
        }
        bb->Release();
    }
    if (g_bbW > 0 && g_bbH > 0) browser::Resize(g_bbW, g_bbH);

    int w = 0, h = 0;
    if (browser::TakeFrame(g_buf, w, h) && w > 0 && h > 0 && g_buf.size() >= static_cast<size_t>(w) * h * 4) {
        g_lastW = w;
        g_lastH = h;
        g_hasFrame = Upload(dev, w, h);
    } else if (!g_tex && g_lastW > 0) {
        g_hasFrame = Upload(dev, g_lastW, g_lastH);
    }

    if (visible && g_hasFrame && g_texW == g_bbW && g_texH == g_bbH) Draw(dev);
}

void OnLost() { Release(); }

void BackBufferSize(int& w, int& h) {
    w = g_bbW;
    h = g_bbH;
}

}  // namespace overlay
