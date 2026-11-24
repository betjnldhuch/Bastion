// ---------------------------------------------------------------------------
// Bastion: our textures on the phone. The mobile game keeps textures in texdb
// (not in .txd), so replacing a .txd like on PC does nothing. The launcher copies
// assets/bastion-mods/tex/*.btx to <game>/SAMP/bastion/tex/ (built by
// client-mods/tools/build_mobile.py from the PC client pack); here every file
// becomes a texture with the same name (file name, lower case), and the texdb
// lookup returns ours instead of the stock one: radar icons (hud), interiors of
// the bank, bar, 24/7 and LSPD (gta_int) and the pickup markers (bst_*).
// .btx: 'BTX1', u32 width, u32 height, u32 packed size, zlib(RGBA rows).
// Textures are created once on the render thread (after RenderWare starts) and
// never destroyed, the same way texdb keeps its loaded textures.
// ---------------------------------------------------------------------------
#include <zlib.h>
#include <dirent.h>
#include <strings.h>
#include <ctype.h>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, RwTexture*> s_bastionTex;
static bool s_bastionTexLoaded = false;

static std::string BastionTexKey(const char* name)
{
    std::string key;
    for (; name && *name; ++name) key += (char)tolower((unsigned char)*name);
    return key;
}

static RwTexture* BastionTexFromFile(const char* path, const std::string& name)
{
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    char magic[4];
    uint32_t hdr[3];
    RwTexture* tex = nullptr;
    if (fread(magic, 1, 4, f) == 4 && !memcmp(magic, "BTX1", 4) && fread(hdr, 4, 3, f) == 3
        && hdr[0] > 0 && hdr[0] <= 4096 && hdr[1] > 0 && hdr[1] <= 4096 && hdr[2] > 0 && hdr[2] < (64u << 20))
    {
        uint32_t w = hdr[0], h = hdr[1];
        std::string packed(hdr[2], '\0');
        std::string rgba((size_t)w * h * 4, '\0');
        uLongf outLen = rgba.size();
        if (fread(&packed[0], 1, packed.size(), f) == packed.size()
            && uncompress((Bytef*)&rgba[0], &outLen, (const Bytef*)packed.data(), packed.size()) == Z_OK
            && outLen == rgba.size())
        {
            RwImage* img = RwImageCreate(w, h, 32);
            if (img && RwImageAllocatePixels(img))
            {
                for (uint32_t y = 0; y < h; y++)
                    memcpy(img->cpPixels + y * img->stride, rgba.data() + (size_t)y * w * 4, w * 4);
                RwInt32 rw, rh, rd, rf;
                RwImageFindRasterFormat(img, rwRASTERTYPETEXTURE, &rw, &rh, &rd, &rf);
                RwRaster* raster = RwRasterCreate(rw, rh, rd, rf);
                if (raster && RwRasterSetFromImage(raster, img))
                {
                    tex = RwTextureCreate(raster);
                    if (tex)
                    {
                        RwTextureSetName(tex, name.c_str());
                        tex->filterAddressing = (tex->filterAddressing & ~0xFF) | rwFILTERLINEAR;
                        tex->refCount = 0x10000000;   // shared like a texdb texture, never freed
                    }
                    else RwRasterDestroy(raster);
                }
                else if (raster) RwRasterDestroy(raster);
            }
            if (img) RwImageDestroy(img);
        }
    }
    fclose(f);
    return tex;
}

static void BastionLoadTextures()
{
    if (s_bastionTexLoaded) return;
    s_bastionTexLoaded = true;
    std::string dir = std::string(BastionStoragePath()) + "SAMP/bastion/tex/";
    DIR* d = opendir(dir.c_str());
    if (!d) { Log("Bastion textures: no %s", dir.c_str()); return; }
    int ok = 0, bad = 0;
    while (dirent* e = readdir(d))
    {
        size_t len = strlen(e->d_name);
        if (len < 5 || strcasecmp(e->d_name + len - 4, ".btx")) continue;
        std::string key = BastionTexKey(std::string(e->d_name, len - 4).c_str());
        RwTexture* tex = BastionTexFromFile((dir + e->d_name).c_str(), key);
        if (tex) { s_bastionTex[key] = tex; ok++; } else bad++;
    }
    closedir(d);
    Log("Bastion textures: %d loaded, %d failed", ok, bad);
}

static RwTexture* BastionFindTexture(const char* name)
{
    if (!name || s_bastionTex.empty()) return nullptr;
    auto it = s_bastionTex.find(BastionTexKey(name));
    return it != s_bastionTex.end() ? it->second : nullptr;
}

// texdb lookup: models, txd stubs, HUD/radar sprites (GetTexture() of samp goes here too)
static RwTexture* (*BastionDbGetTexture)(const char* name);
static RwTexture* BastionDbGetTexture_hook(const char* name)
{
    if (RwTexture* tex = BastionFindTexture(name)) return tex;
    return BastionDbGetTexture(name);
}

// Material textures of a model that the txd does not have (our markers use bst_*):
// only when the game finds nothing itself.
static RwTexture* (*BastionTextureRead)(const char* name, const char* mask);
static RwTexture* BastionTextureRead_hook(const char* name, const char* mask)
{
    RwTexture* tex = BastionTextureRead(name, mask);
    if (tex) return tex;
    if ((tex = BastionFindTexture(name)) != nullptr) tex->refCount++;
    return tex;
}

static void BastionInstallTextureHooks()
{
    CHook::InlineHook("_ZN22TextureDatabaseRuntime10GetTextureEPKc", &BastionDbGetTexture_hook, &BastionDbGetTexture);
    CHook::InlineHook("_Z13RwTextureReadPKcS0_", &BastionTextureRead_hook, &BastionTextureRead);
}
