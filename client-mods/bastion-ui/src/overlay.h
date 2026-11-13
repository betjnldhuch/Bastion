#pragma once
#include <d3d9.h>

// Рисует кадр браузера поверх игры (полноэкранный прямоугольник с альфа-смешиванием).
namespace overlay {
void Present(IDirect3DDevice9* dev, bool visible);  // перед IDirect3DDevice9::Present
void OnLost();                                       // перед IDirect3DDevice9::Reset
void BackBufferSize(int& w, int& h);
}
