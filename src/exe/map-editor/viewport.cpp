#include "scene.hpp"
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <d3dcompiler.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <unordered_map>

/*
 * The 3D view: the scene (scene.cpp) drawn into its own 4x MSAA target, resolved into a texture the panel shows.
 * Game space as the engine's: x forward, y left, z up (right handed), angles pitch / yaw / roll in degrees with a
 * positive pitch looking down. Meshes are lit by one sun and a sky / ground ambient, both sides; textured with their
 * material's color map. Markers (entities without a drawable model) are cubes of their class's color.
 */
namespace editor {
    namespace {
        using namespace DirectX;
        using scene::Instance;
        using scene::Kind;

        /*
         * The game look ("Game lighting", the map's lighting json and state): the sun from the state's block 0 (its
         * pitch / yaw at +0x00 / +0x04 as the game's AnglesToForward, the travel direction; its color at +0x08) with a
         * shadow map, the ambient the probe color tinted by the sky, a haze toward the sky's color with distance, the
         * sky the lighting json's first sky image (a lat-long upper hemisphere, BC6H HDR, its filler painted over and
         * exposed to its sky's median: CleanSky).
         */
        const char* SHADERS{ R"(
#pragma pack_matrix(row_major)
cbuffer Frame : register(b0) {
    float4x4 viewProj; float4x4 lightViewProj; float4x4 invViewProj;
    float4 lightDir; float4 camPos; float4 sunColor; float4 ambient;
    float4 sky;   // x u offset, y u sign, z exposure, w shadow texel
    float4 flags; // x game lighting, y shadows, z haze, w a sky image
    float4 haze;  // x start, y density, z most, w the sky's filler painted over (skyFix)
    float4 horizon; // the sky's color at the horizon (the haze's)
};
cbuffer Object : register(b1) { float4x4 world; float4 color; float4 params; };
Texture2D tex0 : register(t0);
Texture2D skyTex : register(t1);
Texture2D<float> shadowTex : register(t2);
Texture2D skyFix : register(t3);
SamplerState samp0 : register(s0);
SamplerComparisonState shadowSamp : register(s1);
SamplerState skySamp : register(s2);

float3 SkyColor(float3 d) {
    if (flags.w < 0.5) {
        // no image: a plain day gradient
        float t = saturate(d.z);
        return lerp(float3(0.62, 0.68, 0.74), float3(0.28, 0.45, 0.72), t);
    }
    float2 uv = float2(sky.y * atan2(d.y, d.x) / 6.2831853 + sky.x, saturate(1 - asin(clamp(d.z, 0, 1)) / 1.5707963));
    float3 c = skyTex.SampleLevel(skySamp, uv, 0).rgb;
    // the image's flat filler painted over (CleanSky): its alpha is where the image is sky
    if (haze.w > 0.5) {
        float4 f = skyFix.SampleLevel(skySamp, uv, 0);
        c = lerp(f.rgb, c, f.a);
    }
    c *= sky.z;
    return saturate(c / (1 + c) * 1.6);
}

float Shadow(float3 wpos, float3 n) {
    float4 lp = mul(float4(wpos + n * 2.0, 1), lightViewProj);
    float3 p = lp.xyz / lp.w;
    float2 uv = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
    if (any(uv < 0) || any(uv > 1) || p.z > 1) return 1;
    float s = 0;
    [unroll] for (int y = -1; y <= 1; y++) {
        [unroll] for (int x = -1; x <= 1; x++) s += shadowTex.SampleCmpLevelZero(shadowSamp, uv + float2(x, y) * sky.w, p.z - 0.0015);
    }
    return s / 9;
}

struct MeshIn { float3 pos : POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; };
struct MeshOut { float4 pos : SV_POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; float3 wpos : TEXCOORD1; };

MeshOut MeshVS(MeshIn i) {
    MeshOut o;
    float4 w = mul(float4(i.pos, 1), world);
    o.wpos = w.xyz;
    o.pos = mul(w, viewProj);
    o.nrm = mul(float4(i.nrm, 0), world).xyz;
    o.uv = i.uv;
    return o;
}

float4 MeshPS(MeshOut i) : SV_TARGET {
    float len = length(i.nrm);
    float3 n = len > 1e-5 ? i.nrm / len : float3(0, 0, 1);
    // both sides lit: the normal turned to the camera
    if (dot(n, camPos.xyz - i.wpos) < 0) n = -n;
    float4 texel = float4(1, 1, 1, 1);
    if (params.x > 0.5) texel = tex0.Sample(samp0, i.uv);
    // params.z: 1 cut out under half, 2 blended (color.a the opacity: the markers)
    if (params.z > 0.5 && params.z < 1.5) clip(texel.a - 0.5);
    // params.w: a mask (the wall-buy chalk), its shape white
    float3 base = params.w > 0.5 ? float3(0.93, 0.93, 0.9) : color.rgb * texel.rgb;
    float ndl = saturate(dot(n, -lightDir.xyz));
    float hemi = 0.5 + 0.5 * n.z;
    float3 lit;
    if (flags.x > 0.5) {
        float shadow = flags.y > 0.5 ? Shadow(i.wpos, n) : 1;
        float3 amb = ambient.rgb * lerp(0.55, 1.0, hemi);
        lit = base * (sunColor.rgb * ndl * shadow + amb);
        if (flags.z > 0.5) {
            float3 v = i.wpos - camPos.xyz;
            float d = length(v);
            float f = (1 - exp(-max(0, d - haze.x) * haze.y)) * haze.z;
            lit = lerp(lit, horizon.rgb, f);
        }
    }
    else {
        lit = base * (0.22 + 0.33 * hemi + 0.62 * ndl);
    }
    if (params.y > 0.5) lit = lerp(lit, float3(1.0, 0.62, 0.18), 0.38);
    return float4(lit, params.z > 1.5 ? texel.a * color.a : 1);
}

// the shadow map's pass: depth only
float4 DepthVS(MeshIn i) : SV_POSITION { return mul(mul(float4(i.pos, 1), world), lightViewProj); }

// the sky image read back at 2048 wide (SKY_W) from its mip nearest that
float4 StatsPS(float4 pos : SV_POSITION, float2 ndc : TEXCOORD0) : SV_TARGET {
    uint w, h, levels;
    skyTex.GetDimensions(0, w, h, levels);
    return skyTex.SampleLevel(skySamp, float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5), max(0, log2(w / 2048.0)));
}

// the sky behind everything: the view's ray through each pixel
struct SkyOut { float4 pos : SV_POSITION; float2 ndc : TEXCOORD0; };
SkyOut SkyVS(uint id : SV_VertexID) {
    SkyOut o;
    float2 p = float2((id << 1) & 2, id & 2);
    o.pos = float4(p * float2(2, -2) + float2(-1, 1), 1, 1);
    o.ndc = o.pos.xy;
    return o;
}
float4 SkyPS(SkyOut i) : SV_TARGET {
    float4 far = mul(float4(i.ndc, 1, 1), invViewProj);
    float3 d = normalize(far.xyz / far.w - camPos.xyz);
    float3 c = SkyColor(float3(d.xy, max(d.z, 0)));
    // the lowest 6-16 degrees into the horizon's haze (what a map hides there: zm_towers' horizon strip is pasted over
    // its photo from 8.6 degrees down, cutting its clouds flat), then under the horizon the ground's darker haze
    if (flags.w > 0.5) c = lerp(horizon.rgb, c, smoothstep(0.10, 0.28, asin(saturate(d.z))));
    if (d.z < 0) c *= lerp(1.0, 0.55, saturate(-d.z * 3));
    return float4(c, 1);
}

struct LineIn { float3 pos : POSITION; float4 col : COLOR; };
struct LineOut { float4 pos : SV_POSITION; float4 col : COLOR; };
LineOut LineVS(LineIn i) { LineOut o; o.pos = mul(float4(i.pos, 1), viewProj); o.col = i.col; return o; }
float4 LinePS(LineOut i) : SV_TARGET { return i.col; }
)" };

        struct FrameConstants {
            XMFLOAT4X4 viewProj;
            XMFLOAT4X4 lightViewProj;
            XMFLOAT4X4 invViewProj;
            XMFLOAT4 lightDir;
            XMFLOAT4 camPos;
            XMFLOAT4 sunColor;
            XMFLOAT4 ambient;
            XMFLOAT4 sky;
            XMFLOAT4 flags;
            XMFLOAT4 haze;
            XMFLOAT4 horizon;
        };
        struct ObjectConstants {
            XMFLOAT4X4 world;
            XMFLOAT4 color;
            XMFLOAT4 params; // x textured, y selected, z the alpha (scene::Alpha), w a mask
        };
        struct LineVertex {
            float pos[3];
            uint32_t color; // ABGR (R8G8B8A8_UNORM)
        };

        struct GpuPart {
            uint32_t start{};
            uint32_t count{};
            ID3D11ShaderResourceView* srv{};
            scene::Alpha alpha{};
            bool mask{};
        };
        struct GpuMesh {
            ID3D11Buffer* vb{};
            ID3D11Buffer* ib{};
            std::vector<GpuPart> parts{};
        };

        template<typename T>
        void Release(T*& p) {
            if (p) {
                p->Release();
                p = nullptr;
            }
        }

        ID3D11Device* device{};
        ID3D11DeviceContext* context{};
        constexpr UINT SHADOW_SIZE{ 4096 };
        constexpr UINT SKY_W{ 2048 }, SKY_H{ 512 }; // the sky image read back and cleaned at this size (StatsPS's 2048)

        struct Renderer {
            bool tried{};
            bool ready{};
            ID3D11VertexShader* meshVS{};
            ID3D11PixelShader* meshPS{};
            ID3D11InputLayout* meshLayout{};
            ID3D11VertexShader* lineVS{};
            ID3D11PixelShader* linePS{};
            ID3D11InputLayout* lineLayout{};
            ID3D11Buffer* frameCB{};
            ID3D11Buffer* objectCB{};
            ID3D11Buffer* lineVB{};
            size_t lineCapacity{};
            ID3D11SamplerState* sampler{};
            ID3D11RasterizerState* raster{};
            ID3D11DepthStencilState* depth{};
            ID3D11DepthStencilState* noDepth{}; // the gizmo, over everything
            ID3D11DepthStencilState* depthRead{}; // blended parts: tested, not written
            ID3D11RasterizerState* rasterCull{};  // the markers: their near faces only, one layer
            // the game look: the sun's shadow map, the sky
            ID3D11VertexShader* depthVS{};
            ID3D11VertexShader* skyVS{};
            ID3D11PixelShader* skyPS{};
            ID3D11PixelShader* statsPS{};
            ID3D11RasterizerState* rasterShadow{};
            ID3D11SamplerState* shadowSampler{};
            ID3D11SamplerState* skySampler{};
            ID3D11Texture2D* shadowTex{};
            ID3D11DepthStencilView* shadowDsv{};
            ID3D11ShaderResourceView* shadowSrv{};
            ID3D11ShaderResourceView* skySrv{};
            fs::path skyPath{};
            float skyAvg[3]{ 1, 1, 1 };   // what the sky shows above the horizon: the ambient's tint
            float skyHorizon[3]{ 0.62f, 0.68f, 0.74f };
            float skyExposure{ 1 };
            ID3D11ShaderResourceView* skyFix{}; // the image's filler painted over (rgb), where the image is sky (a)
            std::vector<uint8_t> skyShown{};    // the sky as drawn, unrolled at SKY_W x SKY_H (--render's dump)
            ID3D11BlendState* blend{};
            GpuMesh cube{};

            int width{};
            int height{};
            UINT samples{ 1 };
            ID3D11Texture2D* msColor{};
            ID3D11RenderTargetView* rtv{};
            ID3D11Texture2D* msDepth{};
            ID3D11DepthStencilView* dsv{};
            ID3D11Texture2D* resolved{};
            ID3D11ShaderResourceView* srv{};

            std::map<fs::path, ID3D11ShaderResourceView*> textures{};
            std::vector<GpuMesh> meshes{};
        } r{};

        // the scene shown and what it was built from
        scene::Scene world{};
        fs::path builtZone{};
        size_t builtGeneration{ (size_t)-1 };
        int selected{ -1 };

        struct Camera {
            XMFLOAT3 pos{ -600, -600, 450 };
            float yaw{ 45 };
            float pitch{ 28 };
            float speed{ 500 };
            float fov{ 70 };
        } cam{};

        bool showStatic{ true };
        bool showScript{ true };
        bool showEntities{ true };
        bool showGrid{ true };

        // the game look's switches (the side panel) and what the map's lighting says (read each frame: the Lighting
        // tab's edits show at once)
        struct Look {
            bool game{ true };
            bool shadows{ true };
            bool haze{ true };
            bool sky{ true };
            float ambient{ -1 }; // -1 the map's
            float sun{ 1 };
            float skyBright{ 1 };
            XMFLOAT3 lightDir{ 0.5436f, -0.4981f, -0.6756f };
            XMFLOAT3 sunColor{ 1, 1, 1 };
            XMFLOAT3 probe{ 1, 1, 1 };
            float sunPitch{ 137.5f };
            float sunYaw{ 137.5f };
            float sunIlluminance{};
            float mapAmbient{ 0.3f };
            fs::path skyImage{};
            float skyRotation{};
        } look{};

        // moving: the gizmo's world X / Y / Z arrows and its yaw ring about the selected object's origin; scaling (R, W
        // back): its X / Y / Z handles (a binding's scale numbers, or a static model's uniform scale)
        enum class Handle { NONE, X, Y, Z, RING };
        enum class GizmoMode { MOVE, SCALE };
        GizmoMode gizmoMode{ GizmoMode::MOVE };
        struct Gizmo {
            Handle hover{};
            Handle drag{};
            float startOrigin[3]{};
            float anchor[3]{};      // where the gizmo sits off the origin (GizmoAt) when the drag started
            float startAngles[3]{};
            float startScaleDelta[3]{};
            float startScale{ 1 };
            float startParam{}; // the axis's param, or the ring's angle, where the drag started
        } gizmo{};
        bool snapMove{ true };
        float snapSize{ 8 };
        bool snapRotate{ true };
        float snapAngle{ 15 };
        struct UndoStep {
            int index;
            float origin[3];
            float angles[3];
            float scaleDelta[3];
            float scale;
        };
        std::vector<UndoStep> undo{};

        void ApplyUndo(const UndoStep& step, Instance& inst) {
            std::copy(step.origin, step.origin + 3, inst.origin);
            std::copy(step.angles, step.angles + 3, inst.angles);
            std::copy(step.scaleDelta, step.scaleDelta + 3, inst.scaleDelta);
            inst.scale = step.scale;
        }

        // an object back as its files have it (its move, turn and scale undone)
        void Revert(Instance& inst) {
            std::copy(inst.savedOrigin, inst.savedOrigin + 3, inst.origin);
            std::copy(inst.savedAngles, inst.savedAngles + 3, inst.angles);
            std::fill(inst.followed, inst.followed + 3, 0.0f);
            std::fill(inst.scaleDelta, inst.scaleDelta + 3, 0.0f);
            inst.scale = inst.savedScale;
        }

        ID3DBlob* Compile(const char* entry, const char* target) {
            ID3DBlob* code{};
            ID3DBlob* errors{};
            HRESULT hr{ D3DCompile(SHADERS, strlen(SHADERS), "map-editor", nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors) };
            if (FAILED(hr)) {
                Logf(LogLevel::ERR, "3d view: shader %s: %s", entry, errors ? (const char*)errors->GetBufferPointer() : "can't compile");
            }
            Release(errors);
            return code;
        }

        bool MakeBuffer(UINT bind, const void* data, size_t size, ID3D11Buffer** out, bool dynamic = false) {
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = (UINT)size;
            bd.BindFlags = bind;
            bd.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
            bd.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
            D3D11_SUBRESOURCE_DATA sd{ data };
            return SUCCEEDED(device->CreateBuffer(&bd, data ? &sd : nullptr, out));
        }

        // a png / jpg / tga of the map's, its mips made on the GPU
        ID3D11ShaderResourceView* LoadPicture(const fs::path& path) {
            std::vector<uint8_t> rgba{};
            int w{}, h{};
            if (!LoadImagePixels(path, rgba, w, h)) return nullptr;
            D3D11_TEXTURE2D_DESC td{};
            td.Width = w;
            td.Height = h;
            td.MipLevels = 0;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
            ID3D11Texture2D* tex{};
            ID3D11ShaderResourceView* srv{};
            if (SUCCEEDED(device->CreateTexture2D(&td, nullptr, &tex))) {
                context->UpdateSubresource(tex, 0, nullptr, rgba.data(), w * 4, 0);
                device->CreateShaderResourceView(tex, nullptr, &srv);
                if (srv) context->GenerateMips(srv);
                tex->Release();
            }
            return srv;
        }

        /*
         * A dumped stock image (acts' lossless GfxImage json): the header's format (DXGI) at 0x64, width / height at
         * 0x68 / 0x6A, levels at 0x7A, and its pixels: the parts' data, the largest part first (it holds mip 0, each
         * part one more mip, the first the small ones), or "pixels" when resident. sRGB formats are read as their UNORM
         * ones, as the pngs are: the view shows the stored values.
         */
        ID3D11ShaderResourceView* LoadDumpedImage(const fs::path& path) {
            std::ifstream is{ path };
            json x{ json::parse(is, nullptr, false) };
            if (!x.is_object() || !x.contains("header") || !x["header"].is_string()) return nullptr;
            auto unhex = [](const json& j) {
                std::vector<uint8_t> out{};
                if (!j.is_string()) return out;
                const std::string& s{ j.get_ref<const std::string&>() };
                out.resize(s.size() / 2);
                auto nib = [](char c) -> uint8_t { return (uint8_t)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10); };
                for (size_t i = 0; i < out.size(); i++) out[i] = (uint8_t)(nib(s[i * 2]) << 4 | nib(s[i * 2 + 1]));
                return out;
            };
            std::vector<uint8_t> header{ unhex(x["header"]) };
            if (header.size() < 0x80) return nullptr;
            uint32_t format{};
            uint16_t w{}, h{};
            memcpy(&format, header.data() + 0x64, 4);
            memcpy(&w, header.data() + 0x68, 2);
            memcpy(&h, header.data() + 0x6A, 2);
            int levels{ std::max<int>(1, header[0x7A]) };
            std::vector<uint8_t> pixels{};
            if (x.contains("parts") && x["parts"].is_array() && !x["parts"].empty()) {
                for (size_t i = x["parts"].size(); i-- > 0;) {
                    std::vector<uint8_t> part{ unhex(x["parts"][i]["data"]) };
                    pixels.insert(pixels.end(), part.begin(), part.end());
                }
            }
            else if (x.contains("pixels")) {
                pixels = unhex(x["pixels"]);
            }
            if (!w || !h || pixels.empty()) return nullptr;

            // the format's block bytes (bc) or pixel bytes, sRGB read as UNORM
            struct Fmt { uint32_t in; DXGI_FORMAT out; uint32_t bytes; bool bc; };
            static const Fmt FORMATS[]{
                { 71, DXGI_FORMAT_BC1_UNORM, 8, true }, { 72, DXGI_FORMAT_BC1_UNORM, 8, true },
                { 74, DXGI_FORMAT_BC2_UNORM, 16, true }, { 75, DXGI_FORMAT_BC2_UNORM, 16, true },
                { 77, DXGI_FORMAT_BC3_UNORM, 16, true }, { 78, DXGI_FORMAT_BC3_UNORM, 16, true },
                { 80, DXGI_FORMAT_BC4_UNORM, 8, true }, { 81, DXGI_FORMAT_BC4_SNORM, 8, true },
                { 83, DXGI_FORMAT_BC5_UNORM, 16, true }, { 84, DXGI_FORMAT_BC5_SNORM, 16, true },
                { 95, DXGI_FORMAT_BC6H_UF16, 16, true }, { 96, DXGI_FORMAT_BC6H_SF16, 16, true },
                { 98, DXGI_FORMAT_BC7_UNORM, 16, true }, { 99, DXGI_FORMAT_BC7_UNORM, 16, true },
                { 28, DXGI_FORMAT_R8G8B8A8_UNORM, 4, false }, { 29, DXGI_FORMAT_R8G8B8A8_UNORM, 4, false },
                { 87, DXGI_FORMAT_B8G8R8A8_UNORM, 4, false }, { 91, DXGI_FORMAT_B8G8R8A8_UNORM, 4, false },
                { 61, DXGI_FORMAT_R8_UNORM, 1, false }, { 49, DXGI_FORMAT_R8G8_UNORM, 2, false },
                { 10, DXGI_FORMAT_R16G16B16A16_FLOAT, 8, false }, { 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 16, false },
            };
            const Fmt* fmt{};
            for (const Fmt& f : FORMATS) if (f.in == format) fmt = &f;
            if (!fmt) {
                Logf(LogLevel::WARN, "3d view: %s: image format %u not shown", ToUtf8(path.filename()).c_str(), format);
                return nullptr;
            }
            D3D11_SUBRESOURCE_DATA subs[16]{};
            size_t offset{};
            int made{};
            for (int l = 0; l < levels && l < 16; l++) {
                uint32_t lw{ std::max(1u, (uint32_t)w >> l) };
                uint32_t lh{ std::max(1u, (uint32_t)h >> l) };
                uint32_t pitch{ fmt->bc ? std::max(1u, (lw + 3) / 4) * fmt->bytes : lw * fmt->bytes };
                uint32_t rows{ fmt->bc ? std::max(1u, (lh + 3) / 4) : lh };
                size_t size{ (size_t)pitch * rows };
                if (offset + size > pixels.size()) break;
                subs[l] = { pixels.data() + offset, pitch, (UINT)size };
                offset += size;
                made++;
            }
            if (!made) return nullptr;
            D3D11_TEXTURE2D_DESC td{};
            td.Width = w;
            td.Height = h;
            td.MipLevels = made;
            td.ArraySize = 1;
            td.Format = fmt->out;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_IMMUTABLE;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            ID3D11Texture2D* tex{};
            ID3D11ShaderResourceView* srv{};
            if (SUCCEEDED(device->CreateTexture2D(&td, subs, &tex))) {
                device->CreateShaderResourceView(tex, nullptr, &srv);
                tex->Release();
            }
            return srv;
        }

        GpuMesh Upload(const scene::Mesh& mesh) {
            GpuMesh g{};
            if (mesh.vertices.empty() || mesh.indices.empty()) return g;
            MakeBuffer(D3D11_BIND_VERTEX_BUFFER, mesh.vertices.data(), mesh.vertices.size() * sizeof(scene::Vertex), &g.vb);
            MakeBuffer(D3D11_BIND_INDEX_BUFFER, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t), &g.ib);
            for (const scene::Part& p : mesh.parts) {
                GpuPart gp{ p.start, p.count, nullptr, p.alpha, p.mask };
                if (!p.texture.empty()) {
                    auto it{ r.textures.find(p.texture) };
                    if (it == r.textures.end()) {
                        ID3D11ShaderResourceView* srv{ p.texture.extension() == ".json" ? LoadDumpedImage(p.texture) : LoadPicture(p.texture) };
                        if (!srv) Logf(LogLevel::WARN, "3d view: can't read the texture %s", ToUtf8(p.texture).c_str());
                        it = r.textures.emplace(p.texture, srv).first;
                    }
                    gp.srv = it->second;
                }
                g.parts.push_back(gp);
            }
            return g;
        }

        void Free(GpuMesh& g) {
            Release(g.vb);
            Release(g.ib);
            g.parts.clear();
        }

        // a unit cube from 0 to 1 on each axis, its own normals a face
        scene::Mesh CubeMesh() {
            scene::Mesh m{};
            const float n[6][3]{ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
            for (int f = 0; f < 6; f++) {
                int axis{ f / 2 };
                float side{ n[f][axis] > 0 ? 1.0f : 0.0f };
                int u{ (axis + 1) % 3 };
                int v{ (axis + 2) % 3 };
                uint32_t base{ (uint32_t)m.vertices.size() };
                for (int k = 0; k < 4; k++) {
                    scene::Vertex vert{};
                    vert.pos[axis] = side;
                    vert.pos[u] = (k == 1 || k == 2) ? 1.0f : 0.0f;
                    vert.pos[v] = (k >= 2) ? 1.0f : 0.0f;
                    std::copy(n[f], n[f] + 3, vert.normal);
                    m.vertices.push_back(vert);
                }
                m.indices.insert(m.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
            }
            m.parts.push_back({ 0, (uint32_t)m.indices.size(), {} });
            return m;
        }

        bool Init() {
            r.tried = true;
            if (!device || !context) return false;
            ID3DBlob* mvs{ Compile("MeshVS", "vs_4_0") };
            ID3DBlob* mps{ Compile("MeshPS", "ps_4_0") };
            ID3DBlob* lvs{ Compile("LineVS", "vs_4_0") };
            ID3DBlob* lps{ Compile("LinePS", "ps_4_0") };
            bool ok{ mvs && mps && lvs && lps };
            if (ok) {
                device->CreateVertexShader(mvs->GetBufferPointer(), mvs->GetBufferSize(), nullptr, &r.meshVS);
                device->CreatePixelShader(mps->GetBufferPointer(), mps->GetBufferSize(), nullptr, &r.meshPS);
                device->CreateVertexShader(lvs->GetBufferPointer(), lvs->GetBufferSize(), nullptr, &r.lineVS);
                device->CreatePixelShader(lps->GetBufferPointer(), lps->GetBufferSize(), nullptr, &r.linePS);
                const D3D11_INPUT_ELEMENT_DESC meshIn[]{
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                };
                const D3D11_INPUT_ELEMENT_DESC lineIn[]{
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                };
                device->CreateInputLayout(meshIn, 3, mvs->GetBufferPointer(), mvs->GetBufferSize(), &r.meshLayout);
                device->CreateInputLayout(lineIn, 2, lvs->GetBufferPointer(), lvs->GetBufferSize(), &r.lineLayout);
            }
            Release(mvs);
            Release(mps);
            Release(lvs);
            Release(lps);
            if (!ok || !r.meshVS || !r.meshPS || !r.lineVS || !r.linePS || !r.meshLayout || !r.lineLayout) {
                Log(LogLevel::ERR, "3d view: can't make its shaders");
                return false;
            }
            MakeBuffer(D3D11_BIND_CONSTANT_BUFFER, nullptr, sizeof(FrameConstants), &r.frameCB);
            MakeBuffer(D3D11_BIND_CONSTANT_BUFFER, nullptr, sizeof(ObjectConstants), &r.objectCB);

            D3D11_SAMPLER_DESC sd{};
            sd.Filter = D3D11_FILTER_ANISOTROPIC;
            sd.MaxAnisotropy = 8;
            sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            sd.MaxLOD = D3D11_FLOAT32_MAX;
            device->CreateSamplerState(&sd, &r.sampler);

            D3D11_RASTERIZER_DESC rd{};
            rd.FillMode = D3D11_FILL_SOLID;
            rd.CullMode = D3D11_CULL_NONE;
            rd.DepthClipEnable = TRUE;
            rd.MultisampleEnable = TRUE;
            rd.AntialiasedLineEnable = TRUE;
            device->CreateRasterizerState(&rd, &r.raster);
            rd.CullMode = D3D11_CULL_BACK;
            device->CreateRasterizerState(&rd, &r.rasterCull);

            D3D11_DEPTH_STENCIL_DESC dd{};
            dd.DepthEnable = TRUE;
            dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
            device->CreateDepthStencilState(&dd, &r.depth);
            dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            device->CreateDepthStencilState(&dd, &r.depthRead);
            dd.DepthEnable = FALSE;
            device->CreateDepthStencilState(&dd, &r.noDepth);

            D3D11_BLEND_DESC bd{};
            bd.RenderTarget[0].BlendEnable = TRUE;
            bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            device->CreateBlendState(&bd, &r.blend);

            UINT quality{};
            if (SUCCEEDED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality)) && quality > 0) {
                r.samples = 4;
            }
            r.cube = Upload(CubeMesh());

            // the game look's parts: without them the view still draws, plainly
            ID3DBlob* dvs{ Compile("DepthVS", "vs_4_0") };
            ID3DBlob* svs{ Compile("SkyVS", "vs_4_0") };
            ID3DBlob* sps{ Compile("SkyPS", "ps_4_0") };
            ID3DBlob* tps{ Compile("StatsPS", "ps_4_0") };
            if (dvs) device->CreateVertexShader(dvs->GetBufferPointer(), dvs->GetBufferSize(), nullptr, &r.depthVS);
            if (svs) device->CreateVertexShader(svs->GetBufferPointer(), svs->GetBufferSize(), nullptr, &r.skyVS);
            if (sps) device->CreatePixelShader(sps->GetBufferPointer(), sps->GetBufferSize(), nullptr, &r.skyPS);
            if (tps) device->CreatePixelShader(tps->GetBufferPointer(), tps->GetBufferSize(), nullptr, &r.statsPS);
            Release(dvs);
            Release(svs);
            Release(sps);
            Release(tps);

            rd.CullMode = D3D11_CULL_NONE;
            rd.MultisampleEnable = FALSE;
            rd.DepthBias = 0;
            rd.SlopeScaledDepthBias = 2.0f;
            rd.DepthBiasClamp = 0.01f;
            device->CreateRasterizerState(&rd, &r.rasterShadow);

            D3D11_SAMPLER_DESC cmp{};
            cmp.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
            cmp.AddressU = cmp.AddressV = cmp.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
            cmp.BorderColor[0] = cmp.BorderColor[1] = cmp.BorderColor[2] = cmp.BorderColor[3] = 1;
            cmp.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
            cmp.MaxLOD = D3D11_FLOAT32_MAX;
            device->CreateSamplerState(&cmp, &r.shadowSampler);
            D3D11_SAMPLER_DESC skd{};
            skd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            skd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            skd.AddressV = skd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            skd.MaxLOD = D3D11_FLOAT32_MAX;
            device->CreateSamplerState(&skd, &r.skySampler);

            D3D11_TEXTURE2D_DESC st{};
            st.Width = st.Height = SHADOW_SIZE;
            st.MipLevels = 1;
            st.ArraySize = 1;
            st.Format = DXGI_FORMAT_R32_TYPELESS;
            st.SampleDesc.Count = 1;
            st.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            if (SUCCEEDED(device->CreateTexture2D(&st, nullptr, &r.shadowTex))) {
                D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
                dv.Format = DXGI_FORMAT_D32_FLOAT;
                dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                device->CreateDepthStencilView(r.shadowTex, &dv, &r.shadowDsv);
                D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
                sv.Format = DXGI_FORMAT_R32_FLOAT;
                sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                sv.Texture2D.MipLevels = 1;
                device->CreateShaderResourceView(r.shadowTex, &sv, &r.shadowSrv);
            }
            r.ready = true;
            return true;
        }

        void FreeTargets() {
            Release(r.rtv);
            Release(r.msColor);
            Release(r.dsv);
            Release(r.msDepth);
            Release(r.srv);
            Release(r.resolved);
            r.width = r.height = 0;
        }

        bool MakeTargets(int w, int h) {
            if (w == r.width && h == r.height && r.srv) return true;
            FreeTargets();
            D3D11_TEXTURE2D_DESC td{};
            td.Width = w;
            td.Height = h;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = r.samples;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (FAILED(device->CreateTexture2D(&td, nullptr, &r.msColor))) return false;
            device->CreateRenderTargetView(r.msColor, nullptr, &r.rtv);
            td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            if (FAILED(device->CreateTexture2D(&td, nullptr, &r.msDepth))) return false;
            device->CreateDepthStencilView(r.msDepth, nullptr, &r.dsv);
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(device->CreateTexture2D(&td, nullptr, &r.resolved))) return false;
            device->CreateShaderResourceView(r.resolved, nullptr, &r.srv);
            r.width = w;
            r.height = h;
            return r.rtv && r.dsv && r.srv;
        }

        /*
         * Loading the scene in steps, a loading screen drawn between them (the 3D tab opening, a reload): the renderer
         * started, the map read (Scene::Build), its meshes and their textures uploaded a few at a time, the sky.
         * RebuildScene runs them all at once (the Rebuild button, --render).
         */
        /*
         * The mouse grabbed while looking (right button) or panning (middle): the OS cursor put back on the view's
         * center each frame and the motion read from there (ImGui's delta stops when the cursor hits the screen's
         * edge), then back where the button went down.
         */
        struct MouseGrab {
            bool on{};
            POINT start{};  // where the button went down (screen)
            POINT anchor{}; // the view's center (screen)
        } grab{};

        // this frame's motion in pixels (0 while not grabbing); center in ImGui's (the window's client) coordinates
        ImVec2 GrabMotion(bool want, ImVec2 center) {
            if (!want) {
                if (grab.on) SetCursorPos(grab.start.x, grab.start.y);
                grab.on = false;
                return ImVec2(0, 0);
            }
            if (!grab.on) {
                HWND hwnd{ (HWND)ImGui::GetMainViewport()->PlatformHandleRaw };
                if (!hwnd) hwnd = GetActiveWindow();
                GetCursorPos(&grab.start);
                grab.anchor = { (LONG)center.x, (LONG)center.y };
                if (hwnd) ClientToScreen(hwnd, &grab.anchor);
                SetCursorPos(grab.anchor.x, grab.anchor.y);
                grab.on = true;
                return ImVec2(0, 0);
            }
            POINT p{};
            GetCursorPos(&p);
            SetCursorPos(grab.anchor.x, grab.anchor.y);
            return ImVec2((float)(p.x - grab.anchor.x), (float)(p.y - grab.anchor.y));
        }

        void UpdateLook(App& app);
        void LookAt(const float target[3], float distance);
        enum class LoadStep { NONE, START, BUILD, UPLOAD, SKY };
        struct Loading {
            LoadStep step{};
            size_t next{};       // the next mesh to upload
            scene::Selection pick{}; // the selection, kept by its label (an entity's index changes when the tools rewrite the list), or a copy's
            bool firstLook{};    // the camera put on the origin once loaded
            bool shown{};        // the loading screen drawn once before the first step
            bool background{};   // a reload: built aside while the old scene stays drawn, swapped in at the end
            std::chrono::steady_clock::time_point started{}, stepStarted{};
        } loading{};
        // a background reload's scene and meshes, until they're swapped in
        scene::Scene pendingWorld{};
        std::vector<GpuMesh> pendingMeshes{};
        scene::Selection copyPick{}; // Ctrl+D's copy, selected once the map reloads

        void StartLoading(App& app, bool background = false) {
            if (size_t moved{ world.MovedCount() }) {
                Logf(LogLevel::WARN, "3d view: the map reloaded, %zu unsaved move(s) dropped", moved);
            }
            if (loading.step == LoadStep::NONE) {
                loading.pick = {};
                if (!copyPick.label.empty() || copyPick.sourceIndex >= 0) loading.pick = copyPick;
                else if (selected >= 0 && selected < (int)world.instances.size()) loading.pick.label = world.instances[selected].label;
                copyPick = {};
            }
            loading.firstLook = loading.firstLook || builtGeneration == (size_t)-1;
            loading.step = LoadStep::START;
            loading.next = 0;
            loading.shown = false;
            loading.background = background;
            loading.started = loading.stepStarted = std::chrono::steady_clock::now();
            for (GpuMesh& g : pendingMeshes) Free(g);
            pendingMeshes.clear();
            if (background) return;
            selected = -1;
            gizmo = {};
            undo.clear();
            for (GpuMesh& g : r.meshes) Free(g);
            r.meshes.clear();
        }

        // the next step, uploads until the budget's spent (seconds)
        void LoadingStep(App& app, float budget) {
            auto now = [] { return std::chrono::steady_clock::now(); };
            auto since = [&](std::chrono::steady_clock::time_point t) { return std::chrono::duration<float>(now() - t).count(); };
            scene::Scene& target{ loading.background ? pendingWorld : world };
            std::vector<GpuMesh>& meshes{ loading.background ? pendingMeshes : r.meshes };
            switch (loading.step) {
            case LoadStep::START:
                if (!r.tried) Init();
                loading.step = LoadStep::BUILD;
                break;
            case LoadStep::BUILD:
                loading.stepStarted = now();
                target.Build(app.project);
                builtZone = app.project.zone.path;
                builtGeneration = app.project.generation;
                Logf(LogLevel::INFO, "3d view: built in %.2fs", since(loading.stepStarted));
                loading.step = LoadStep::UPLOAD;
                loading.stepStarted = now();
                break;
            case LoadStep::UPLOAD: {
                auto t0{ now() };
                while (loading.next < target.meshes.size() && since(t0) < budget) meshes.push_back(Upload(target.meshes[loading.next++]));
                if (loading.next < target.meshes.size()) break;
                Logf(LogLevel::INFO, "3d view: %zu meshes uploaded in %.2fs", target.meshes.size(), since(loading.stepStarted));
                if (loading.background) {
                    // swapped in: the old scene's meshes go
                    for (GpuMesh& g : r.meshes) Free(g);
                    r.meshes = std::move(pendingMeshes);
                    pendingMeshes.clear();
                    world = std::move(pendingWorld);
                    pendingWorld = {};
                    selected = -1;
                    gizmo = {};
                    undo.clear();
                }
                for (size_t i = 0; i < world.instances.size(); i++) {
                    const Instance& inst{ world.instances[i] };
                    const scene::Selection& p{ loading.pick };
                    bool hit{ p.sourceIndex >= 0 ? inst.board == 0 && inst.kind != Kind::SPAWNED && inst.sourceIndex == p.sourceIndex && inst.source == p.source
                        : !p.label.empty() && inst.label == p.label };
                    if (hit) {
                        selected = (int)i;
                        break;
                    }
                }
                loading.step = LoadStep::SKY;
                break;
            }
            case LoadStep::SKY:
                UpdateLook(app);
                loading.step = LoadStep::NONE;
                if (loading.firstLook) {
                    const float target[3]{ 0, 0, 0 };
                    cam.yaw = 45;
                    cam.pitch = 28;
                    LookAt(target, 900);
                    loading.firstLook = false;
                }
                Logf(LogLevel::INFO, "3d view: %s in %.2fs", loading.background ? "reloaded" : "loaded", since(loading.started));
                break;
            default:
                break;
            }
        }

        void RebuildScene(App& app) {
            StartLoading(app);
            while (loading.step != LoadStep::NONE) LoadingStep(app, FLT_MAX);
        }

        // the loading screen: what's being done, how far along
        void DrawLoading() {
            const char* what{ "Starting the renderer..." };
            float progress{ 0.02f };
            char buff[96]{};
            switch (loading.step) {
            case LoadStep::BUILD:
                what = "Reading the map: its gfx_map, entities and models...";
                progress = 0.08f;
                break;
            case LoadStep::UPLOAD:
                snprintf(buff, sizeof(buff), "Loading models and textures (%zu of %zu)...", loading.next, world.meshes.size());
                what = buff;
                progress = 0.15f + 0.75f * (world.meshes.empty() ? 1.0f : (float)loading.next / world.meshes.size());
                break;
            case LoadStep::SKY:
                what = "Preparing the sky and lighting...";
                progress = 0.95f;
                break;
            default:
                break;
            }
            ImVec2 avail{ ImGui::GetContentRegionAvail() };
            ImVec2 origin{ ImGui::GetCursorScreenPos() };
            ImDrawList* dl{ ImGui::GetWindowDrawList() };
            dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(14, 15, 17, 255));
            float width{ std::min(460.0f, avail.x - 40) };
            ImVec2 center{ origin.x + avail.x * 0.5f, origin.y + avail.y * 0.45f };
            // a spinner: an arc going round
            float t{ (float)ImGui::GetTime() };
            float radius{ 18 };
            ImVec2 sc{ center.x, center.y - 58 };
            dl->PathClear();
            for (int i = 0; i <= 24; i++) {
                float a{ t * 5.0f + i * (XM_PI * 1.4f / 24) };
                dl->PathLineTo(ImVec2(sc.x + std::cos(a) * radius, sc.y + std::sin(a) * radius));
            }
            dl->PathStroke(ImGui::GetColorU32(ImGuiCol_CheckMark), 0, 3.5f);
            const char* title{ "Loading the 3D view" };
            ImVec2 ts{ ImGui::CalcTextSize(title) };
            dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - 22), ImGui::GetColorU32(ImGuiCol_Text), title);
            ImGui::SetCursorScreenPos(ImVec2(center.x - width * 0.5f, center.y + 4));
            // the theme's accent, not its (red) plot color
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
            ImGui::ProgressBar(progress, ImVec2(width, 0), "");
            ImGui::PopStyleColor();
            ImVec2 ws{ ImGui::CalcTextSize(what) };
            dl->AddText(ImVec2(center.x - ws.x * 0.5f, center.y + 32), ImGui::GetColorU32(ImGuiCol_TextDisabled), what);
            ImGui::Dummy(ImVec2(0, 0));
        }

        bool Visible(const Instance& inst) {
            if (inst.board > 0) return showEntities; // with its barrier
            switch (inst.kind) {
            case Kind::STATIC_MODEL: return showStatic;
            case Kind::SCRIPT_MODEL:
            case Kind::SPAWNED: return showScript;
            default: return showEntities;
            }
        }

        XMMATRIX WorldOf(const Instance& inst) {
            if (inst.useRot) {
                return XMMatrixScaling(inst.scale, inst.scale, inst.scale) *
                    XMMatrixRotationQuaternion(XMVectorSet(inst.rot[0], inst.rot[1], inst.rot[2], inst.rot[3])) *
                    XMMatrixTranslation(inst.origin[0], inst.origin[1], inst.origin[2]);
            }
            // a binding's scale handles, the live preview of the new size: its box grown by the drags, on x / y about its
            // middle on both sides, up from its base (the room's walls, a window's sill)
            if (const scene::Binding* b{ world.BindingOf(inst) }; b && inst.kind == Kind::STATIC_MODEL) {
                if (const scene::Target* g{ b->Group() }; g && g->Scales()) {
                    float ext[3]{}, s[3]{};
                    for (int k = 0; k < 3; k++) ext[k] = std::max(1.0f, inst.maxs[k] - inst.mins[k]);
                    float dy{ g->scaleXY ? inst.scaleDelta[0] : inst.scaleDelta[1] };
                    s[0] = std::max(0.05f, (ext[0] + 2 * inst.scaleDelta[0]) / ext[0]);
                    s[1] = std::max(0.05f, (ext[1] + 2 * dy) / ext[1]);
                    s[2] = std::max(0.05f, (ext[2] + inst.scaleDelta[2]) / ext[2]);
                    XMVECTOR pivot{ XMVectorSet((inst.mins[0] + inst.maxs[0]) * 0.5f, (inst.mins[1] + inst.maxs[1]) * 0.5f, inst.mins[2], 0) };
                    return XMMatrixTranslationFromVector(-pivot) * XMMatrixScaling(s[0], s[1], s[2]) * XMMatrixTranslationFromVector(pivot) *
                        XMMatrixScaling(inst.scale, inst.scale, inst.scale) *
                        XMMatrixRotationZ(XMConvertToRadians(inst.angles[1])) *
                        XMMatrixTranslation(inst.origin[0], inst.origin[1], inst.origin[2]);
                }
            }
            return XMMatrixScaling(inst.scale, inst.scale, inst.scale) *
                XMMatrixRotationX(XMConvertToRadians(inst.angles[2])) *
                XMMatrixRotationY(XMConvertToRadians(inst.angles[0])) *
                XMMatrixRotationZ(XMConvertToRadians(inst.angles[1])) *
                XMMatrixTranslation(inst.origin[0], inst.origin[1], inst.origin[2]);
        }

        // a marker's cube in model space: the unit cube on its box
        XMMATRIX BoxOf(const Instance& inst) {
            return XMMatrixScaling(inst.maxs[0] - inst.mins[0], inst.maxs[1] - inst.mins[1], inst.maxs[2] - inst.mins[2]) *
                XMMatrixTranslation(inst.mins[0], inst.mins[1], inst.mins[2]);
        }

        XMVECTOR Forward() {
            float p{ XMConvertToRadians(cam.pitch) };
            float y{ XMConvertToRadians(cam.yaw) };
            return XMVectorSet(std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), -std::sin(p), 0);
        }

        XMVECTOR Right() {
            float y{ XMConvertToRadians(cam.yaw) };
            return XMVectorSet(std::sin(y), -std::cos(y), 0, 0);
        }

        XMMATRIX ViewProj(float aspect) {
            XMVECTOR pos{ XMLoadFloat3(&cam.pos) };
            XMMATRIX view{ XMMatrixLookToRH(pos, Forward(), XMVectorSet(0, 0, 1, 0)) };
            XMMATRIX proj{ XMMatrixPerspectiveFovRH(XMConvertToRadians(cam.fov), aspect, 2.0f, 60000.0f) };
            return view * proj;
        }

        void LookAt(const float target[3], float distance) {
            XMVECTOR t{ XMVectorSet(target[0], target[1], target[2], 0) };
            XMStoreFloat3(&cam.pos, t - Forward() * distance);
        }

        // the view on an instance: its box's center, far enough to see it whole
        void Frame(const Instance& inst) {
            float c[3]{};
            float radius{};
            for (int k = 0; k < 3; k++) {
                c[k] = (inst.mins[k] + inst.maxs[k]) * 0.5f;
                radius = std::max(radius, (inst.maxs[k] - inst.mins[k]) * inst.scale);
            }
            XMVECTOR center{ XMVector3TransformCoord(XMVectorSet(c[0], c[1], c[2], 1), WorldOf(inst)) };
            float target[3]{ XMVectorGetX(center), XMVectorGetY(center), XMVectorGetZ(center) };
            LookAt(target, std::max(64.0f, radius * 1.2f));
        }

        void FrameAll() {
            float c[3]{};
            float radius{};
            for (int k = 0; k < 3; k++) {
                c[k] = (world.mins[k] + world.maxs[k]) * 0.5f;
                radius = std::max(radius, world.maxs[k] - world.mins[k]);
            }
            LookAt(c, std::max(256.0f, radius * 0.8f));
        }

        uint32_t Rgba(float r_, float g, float b, float a) {
            auto c = [](float v) { return (uint32_t)std::clamp(v * 255.0f, 0.0f, 255.0f); };
            return c(r_) | (c(g) << 8) | (c(b) << 16) | (c(a) << 24);
        }

        void Line(std::vector<LineVertex>& out, XMVECTOR a, XMVECTOR b, uint32_t color) {
            LineVertex va{ { XMVectorGetX(a), XMVectorGetY(a), XMVectorGetZ(a) }, color };
            LineVertex vb{ { XMVectorGetX(b), XMVectorGetY(b), XMVectorGetZ(b) }, color };
            out.push_back(va);
            out.push_back(vb);
        }

        // an instance's box, its 12 edges in world space
        void BoxLines(std::vector<LineVertex>& out, const Instance& inst, uint32_t color) {
            XMMATRIX w{ WorldOf(inst) };
            XMVECTOR corner[8];
            for (int i = 0; i < 8; i++) {
                corner[i] = XMVector3TransformCoord(XMVectorSet(i & 1 ? inst.maxs[0] : inst.mins[0], i & 2 ? inst.maxs[1] : inst.mins[1],
                    i & 4 ? inst.maxs[2] : inst.mins[2], 1), w);
            }
            const int edges[12][2]{ { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
            for (const auto& e : edges) Line(out, corner[e[0]], corner[e[1]], color);
        }

        Instance* SelectedInstance() {
            return selected >= 0 && selected < (int)world.instances.size() ? &world.instances[selected] : nullptr;
        }

        // a board's barrier (the entity it's spawned from)
        const Instance* BarrierOf(const Instance& board) {
            for (const Instance& b : world.instances) {
                if (b.board == 0 && b.kind != Kind::SPAWNED && b.source == board.source && b.sourceIndex == board.sourceIndex) return &b;
            }
            return nullptr;
        }

        // what a click selects: a board's barrier (they move and turn as one), else the object
        int OwnerIndex(int i) {
            if (i < 0 || i >= (int)world.instances.size() || world.instances[i].board <= 0) return i;
            const Instance* b{ BarrierOf(world.instances[i]) };
            return b ? (int)(b - world.instances.data()) : i;
        }

        // a board of the selected barrier: drawn selected with it
        bool BoardOfSelected(const Instance& inst) {
            return inst.board > 0 && selected >= 0 && selected < (int)world.instances.size() && BarrierOf(inst) == &world.instances[selected];
        }

        // a barrier drawn as its boards: its marker box neither drawn nor picked (it hid the window's sill), its icon kept
        bool HasBoards(const Instance& inst) {
            if (inst.mesh >= 0 || inst.board > 0) return false;
            for (const Instance& b : world.instances) {
                if (b.board > 0 && b.source == inst.source && b.sourceIndex == inst.sourceIndex) return true;
            }
            return false;
        }

        // the group an object moves with (its binding's relative target: a window, a wall-buy, the room), a board its barrier's
        const scene::Target* GroupOf(const Instance& inst) {
            const Instance* owner{ inst.board > 0 ? BarrierOf(inst) : &inst };
            const scene::Binding* b{ owner ? world.BindingOf(*owner) : nullptr };
            return b ? b->Group() : nullptr;
        }

        std::string GroupKey(const scene::Target* g) {
            return g ? ToUtf8(g->file) + "|" + g->path.dump() : std::string{};
        }

        // what the selected object's binding lets it do
        struct Allowed {
            bool move{};
            bool z{};
            bool yaw{};
            bool full{}; // pitch and roll too
            bool x{ true };
            bool y{ true };
            bool sx{}, sy{}, sz{}; // the scale mode's handles
            bool uniform{};        // they scale the model as a whole (a static model's scale)
            bool Scales() const { return sx || sy || sz; }
            bool Editable() const { return move || Scales(); }
        };
        Allowed AllowedFor(const Instance& inst) {
            // a board moves its barrier's group
            if (inst.board > 0) {
                const Instance* b{ BarrierOf(inst) };
                return b ? AllowedFor(*b) : Allowed{};
            }
            if (!inst.locked.empty()) return {};
            const scene::Binding* b{ world.BindingOf(inst) };
            if (!b) {
                // a static model saved into the gfx_map: its uniform scale too
                bool st{ inst.kind == Kind::STATIC_MODEL };
                return { true, true, true, true, true, true, st, st, st, st };
            }
            if (!b->locked.empty()) return {};
            // a group: along its wall and up / down (anywhere with its own origin: a stone), turned alone by its yaw
            // number; its scale numbers' handles
            if (const scene::Target* g{ b->Group() }) {
                bool along{ !g->along.empty() }, across{ !g->across.empty() }, free{ !g->origin.empty() };
                bool x{ free || (along && g->axis == 0) || (across && g->AcrossAxis() == 0) };
                bool y{ free || (along && g->axis == 1) || (across && g->AcrossAxis() == 1) }, z{ !g->zKey.empty() || g->origin == "xyz" };
                bool yaw{ !g->yawKey.empty() || !g->yaw.empty() };
                return { x || y || z || yaw, z, yaw, false, x, y, !g->scaleKey[0].empty(), !g->scaleKey[1].empty() && !g->scaleXY, !g->scaleKey[2].empty() };
            }
            return { true, b->MovesZ(), b->Rotates(), b->FullAngles() };
        }

        /*
         * The selected object's group moves with it: every object bound to the same object of the map's json (the
         * window's barrier, its structs, risers and sill; a wall-buy's trigger, model and chalk) by the selected one's
         * move, on each axis only where its binding writes the same number (the window's center moves them all, the
         * barrier's barrierZ only it and its boards, the sill's height only the sill); boards answer as their barrier.
         */
        void SyncMembers();
        void SyncGroup() {
            SyncMembers();
            // the boards on their barriers as they are now (moved, turned)
            world.PlaceBoards();
        }
        void SyncMembers() {
            const Instance* sel{ selected >= 0 && selected < (int)world.instances.size() ? &world.instances[selected] : nullptr };
            const scene::Target* g{ sel ? GroupOf(*sel) : nullptr };
            if (!g) return;
            std::string key{ GroupKey(g) };
            float d[3]{};
            for (int k = 0; k < 3; k++) d[k] = sel->origin[k] - sel->savedOrigin[k] - sel->followed[k];
            for (size_t i = 0; i < world.instances.size(); i++) {
                Instance& m{ world.instances[i] };
                if ((int)i == selected) continue;
                const scene::Target* mg{ GroupOf(m) };
                if (!mg || GroupKey(mg) != key) continue;
                // shared: the same number on that axis (moves with it); followed: a number it follows without writing
                // it (the sill on the window's center: its own shift kept, the followed part recorded apart)
                bool shared[3]{}, followed[3]{};
                auto same = [](const std::string& a, const std::string& b) { return !a.empty() && a == b; };
                auto follows = [&](const std::string& leaderKey) {
                    return !leaderKey.empty() && std::find(mg->follow.begin(), mg->follow.end(), leaderKey) != mg->follow.end();
                };
                if (g->axis >= 0 && mg->axis == g->axis) {
                    shared[g->axis] = same(mg->along, g->along);
                    followed[g->axis] = !shared[g->axis] && follows(g->along);
                    shared[g->AcrossAxis()] = same(mg->across, g->across);
                    followed[g->AcrossAxis()] = !shared[g->AcrossAxis()] && follows(g->across);
                }
                shared[2] = same(mg->zKey, g->zKey);
                followed[2] = !shared[2] && follows(g->zKey);
                // the axes it doesn't share or follow keep its own change (the sill's height when the barrier is selected)
                for (int k = 0; k < 3; k++) {
                    if (shared[k]) {
                        m.origin[k] = m.savedOrigin[k] + d[k];
                        m.followed[k] = 0;
                    }
                    else if (followed[k]) {
                        float own{ m.origin[k] - m.savedOrigin[k] - m.followed[k] };
                        m.followed[k] = d[k];
                        m.origin[k] = m.savedOrigin[k] + own + d[k];
                    }
                }
            }
        }

        // where the gizmo sits: the object's origin, or its box's middle when the origin is off the box (a static model
        // built in world space: the window sills' origin is the room's middle)
        XMVECTOR GizmoAt(const Instance& inst) {
            bool inside{ true };
            for (int k = 0; k < 3; k++) inside = inside && inst.mins[k] - 1 <= 0 && 0 <= inst.maxs[k] + 1;
            if (inside || inst.mesh < 0) return XMVectorSet(inst.origin[0], inst.origin[1], inst.origin[2], 1);
            XMVECTOR c{ XMVectorSet((inst.mins[0] + inst.maxs[0]) * 0.5f, (inst.mins[1] + inst.maxs[1]) * 0.5f, (inst.mins[2] + inst.maxs[2]) * 0.5f, 1) };
            return XMVector3TransformCoord(c, WorldOf(inst));
        }

        // the gizmo's arrow length: the same size on screen at any distance
        float GizmoSize(const Instance& inst) {
            XMVECTOR o{ XMVectorSetW(GizmoAt(inst), 0) };
            float d{ XMVectorGetX(XMVector3Length(o - XMLoadFloat3(&cam.pos))) };
            return std::max(2.0f, d * std::tan(XMConvertToRadians(cam.fov) * 0.5f) * 0.28f);
        }

        XMVECTOR AxisOf(Handle h) {
            return h == Handle::X ? XMVectorSet(1, 0, 0, 0) : h == Handle::Y ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(0, 0, 1, 0);
        }

        // a handle's direction: the world's axes, a scale handle's x / y the object's own (a turned stone grows along itself)
        XMVECTOR HandleAxis(const Instance& inst, Handle h) {
            if (gizmoMode != GizmoMode::SCALE || h == Handle::Z || AllowedFor(inst).uniform) return AxisOf(h);
            return XMVector3TransformNormal(AxisOf(h), XMMatrixRotationZ(XMConvertToRadians(inst.angles[1])));
        }

        uint32_t HandleColor(Handle h) {
            if (gizmo.drag == h || (gizmo.drag == Handle::NONE && gizmo.hover == h)) return Rgba(1, 0.9f, 0.2f, 1);
            switch (h) {
            case Handle::X: return Rgba(1, 0.3f, 0.3f, 1);
            case Handle::Y: return Rgba(0.35f, 1, 0.35f, 1);
            case Handle::Z: return Rgba(0.35f, 0.55f, 1, 1);
            default: return Rgba(0.5f, 0.9f, 1, 1);
            }
        }

        constexpr int RING_SEGMENTS{ 64 };
        XMVECTOR RingPoint(XMVECTOR o, float radius, int i) {
            float a{ XM_2PI * i / RING_SEGMENTS };
            return o + XMVectorSet(std::cos(a) * radius, std::sin(a) * radius, 0, 0);
        }

        // a handle shown in the gizmo's mode: the move arrows the binding writes, or its scale handles; the ring moving
        bool HandleOn(const Allowed& a, Handle h) {
            if (gizmoMode == GizmoMode::SCALE) return h == Handle::X ? a.sx : h == Handle::Y ? a.sy : h == Handle::Z ? a.sz : false;
            if (!a.move) return false;
            return h == Handle::X ? a.x : h == Handle::Y ? a.y : h == Handle::Z ? a.z : h == Handle::RING ? a.yaw : false;
        }

        bool GizmoOn(const Allowed& a) {
            return HandleOn(a, Handle::X) || HandleOn(a, Handle::Y) || HandleOn(a, Handle::Z) || HandleOn(a, Handle::RING);
        }

        void GizmoLines(const Instance& inst, std::vector<LineVertex>& out) {
            Allowed a{ AllowedFor(inst) };
            if (!GizmoOn(a)) return;
            float s{ GizmoSize(inst) };
            XMVECTOR o{ GizmoAt(inst) };
            for (Handle h : { Handle::X, Handle::Y, Handle::Z }) {
                if (HandleOn(a, h)) Line(out, o, o + HandleAxis(inst, h) * s, HandleColor(h));
            }
            if (HandleOn(a, Handle::RING)) {
                for (int i = 0; i < RING_SEGMENTS; i++) Line(out, RingPoint(o, s * 0.75f, i), RingPoint(o, s * 0.75f, i + 1), HandleColor(Handle::RING));
            }
        }

        bool ToScreen(const XMMATRIX& vp, XMVECTOR p, ImVec2 origin, float w, float h, ImVec2& out) {
            XMVECTOR c{ XMVector4Transform(XMVectorSetW(p, 1), vp) };
            float cw{ XMVectorGetW(c) };
            if (cw <= 0.01f) return false;
            out = ImVec2(origin.x + (XMVectorGetX(c) / cw * 0.5f + 0.5f) * w, origin.y + (0.5f - XMVectorGetY(c) / cw * 0.5f) * h);
            return true;
        }

        float SegmentDistance(ImVec2 p, ImVec2 a, ImVec2 b) {
            float dx{ b.x - a.x }, dy{ b.y - a.y };
            float len{ dx * dx + dy * dy };
            float t{ len > 0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len, 0.0f, 1.0f) : 0.0f };
            float ex{ a.x + dx * t - p.x }, ey{ a.y + dy * t - p.y };
            return std::sqrt(ex * ex + ey * ey);
        }

        // the handle under the mouse (within 9 pixels), arrows before the ring
        Handle HitGizmo(const Instance& inst, const XMMATRIX& vp, ImVec2 origin, float w, float h, ImVec2 mouse) {
            Allowed a{ AllowedFor(inst) };
            if (!GizmoOn(a)) return Handle::NONE;
            float s{ GizmoSize(inst) };
            XMVECTOR o{ GizmoAt(inst) };
            ImVec2 so{};
            if (!ToScreen(vp, o, origin, w, h, so)) return Handle::NONE;
            Handle best{ Handle::NONE };
            float bestD{ 9 };
            for (Handle hd : { Handle::X, Handle::Y, Handle::Z }) {
                if (!HandleOn(a, hd)) continue;
                ImVec2 tip{};
                if (!ToScreen(vp, o + HandleAxis(inst, hd) * s, origin, w, h, tip)) continue;
                float d{ SegmentDistance(mouse, so, tip) };
                if (d < bestD) {
                    bestD = d;
                    best = hd;
                }
            }
            if (best != Handle::NONE || !HandleOn(a, Handle::RING)) return best;
            for (int i = 0; i < RING_SEGMENTS; i++) {
                ImVec2 p0{}, p1{};
                if (!ToScreen(vp, RingPoint(o, s * 0.75f, i), origin, w, h, p0) || !ToScreen(vp, RingPoint(o, s * 0.75f, i + 1), origin, w, h, p1)) continue;
                if (SegmentDistance(mouse, p0, p1) < 9) return Handle::RING;
            }
            return Handle::NONE;
        }

        // where the mouse ray passes the axis through the drag's start: its param along the axis
        bool AxisParam(XMVECTOR ro, XMVECTOR rd, XMVECTOR axis, float& param) {
            XMVECTOR start{ XMVectorSet(gizmo.startOrigin[0] + gizmo.anchor[0], gizmo.startOrigin[1] + gizmo.anchor[1], gizmo.startOrigin[2] + gizmo.anchor[2], 0) };
            XMVECTOR w0{ ro - start };
            float a{ XMVectorGetX(XMVector3Dot(rd, rd)) };
            float b{ XMVectorGetX(XMVector3Dot(rd, axis)) };
            float d{ XMVectorGetX(XMVector3Dot(rd, w0)) };
            float e{ XMVectorGetX(XMVector3Dot(axis, w0)) };
            float denom{ a - b * b };
            if (std::abs(denom) < 1e-9f * a) return false;
            param = (a * e - b * d) / denom;
            return true;
        }

        // the mouse ray on the start's horizontal plane: the angle about the start, in degrees
        bool RingAngle(XMVECTOR ro, XMVECTOR rd, float& angle) {
            float dz{ XMVectorGetZ(rd) };
            if (std::abs(dz) < 1e-9f) return false;
            float t{ (gizmo.startOrigin[2] + gizmo.anchor[2] - XMVectorGetZ(ro)) / dz };
            XMVECTOR p{ ro + rd * t };
            angle = XMConvertToDegrees(std::atan2(XMVectorGetY(p) - gizmo.startOrigin[1] - gizmo.anchor[1], XMVectorGetX(p) - gizmo.startOrigin[0] - gizmo.anchor[0]));
            return true;
        }

        void PushUndo(int index) {
            const Instance& inst{ world.instances[index] };
            UndoStep step{ index };
            std::copy(inst.origin, inst.origin + 3, step.origin);
            std::copy(inst.angles, inst.angles + 3, step.angles);
            std::copy(inst.scaleDelta, inst.scaleDelta + 3, step.scaleDelta);
            step.scale = inst.scale;
            undo.push_back(step);
            if (undo.size() > 200) undo.erase(undo.begin());
        }

        float Wrap360(float a) {
            a = std::fmod(a, 360.0f);
            return a < 0 ? a + 360.0f : a;
        }

        // a turn, -180 to 180
        float Turn(float a) {
            a = Wrap360(a);
            return a > 180 ? a - 360 : a;
        }

        void SetObject(const XMMATRIX& w, const float color[3], bool textured, bool isSelected, scene::Alpha alpha = scene::Alpha::SOLID,
            bool mask = false, float opacity = 1) {
            ObjectConstants oc{};
            XMStoreFloat4x4(&oc.world, w);
            oc.color = XMFLOAT4(color[0], color[1], color[2], opacity);
            oc.params = XMFLOAT4(textured ? 1.0f : 0.0f, isSelected ? 1.0f : 0.0f, (float)alpha, textured && mask ? 1.0f : 0.0f);
            context->UpdateSubresource(r.objectCB, 0, nullptr, &oc, 0, 0);
        }

        // a marker's box, see-through (its icon says what it is)
        void DrawMarker(const XMMATRIX& w, const float color[3], bool isSelected) {
            if (!r.cube.vb || !r.cube.ib) return;
            UINT stride{ sizeof(scene::Vertex) };
            UINT offset{};
            context->IASetVertexBuffers(0, 1, &r.cube.vb, &stride, &offset);
            context->IASetIndexBuffer(r.cube.ib, DXGI_FORMAT_R32_UINT, 0);
            SetObject(w, color, false, isSelected, scene::Alpha::BLEND, false, isSelected ? 0.75f : 0.4f);
            ID3D11ShaderResourceView* none{};
            context->PSSetShaderResources(0, 1, &none);
            context->RSSetState(r.rasterCull);
            context->DrawIndexed(r.cube.parts[0].count, 0, 0);
            context->RSSetState(r.raster);
        }

        // the mesh's parts of a pass: false the solid and cut out ones, true the blended ones (a blended part without its
        // texture isn't drawn: a decal's plate)
        void DrawMesh(const GpuMesh& g, const XMMATRIX& w, const float color[3], bool isSelected, bool blendPass = false) {
            if (!g.vb || !g.ib) return;
            UINT stride{ sizeof(scene::Vertex) };
            UINT offset{};
            context->IASetVertexBuffers(0, 1, &g.vb, &stride, &offset);
            context->IASetIndexBuffer(g.ib, DXGI_FORMAT_R32_UINT, 0);
            for (const GpuPart& p : g.parts) {
                bool blended{ p.alpha == scene::Alpha::BLEND };
                if (blended != blendPass || (blended && !p.srv)) continue;
                // textured parts take the texture's colors whole
                const float white[3]{ 1, 1, 1 };
                SetObject(w, p.srv ? white : color, p.srv != nullptr, isSelected, p.alpha, p.mask);
                context->PSSetShaderResources(0, 1, &p.srv);
                context->DrawIndexed(p.count, p.start, 0);
            }
        }

        void DrawLines(const std::vector<LineVertex>& lines) {
            if (lines.empty()) return;
            if (lines.size() > r.lineCapacity) {
                Release(r.lineVB);
                r.lineCapacity = lines.size() * 2;
                MakeBuffer(D3D11_BIND_VERTEX_BUFFER, nullptr, r.lineCapacity * sizeof(LineVertex), &r.lineVB, true);
            }
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (!r.lineVB || FAILED(context->Map(r.lineVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
            memcpy(mapped.pData, lines.data(), lines.size() * sizeof(LineVertex));
            context->Unmap(r.lineVB, 0);
            UINT stride{ sizeof(LineVertex) };
            UINT offset{};
            context->IASetVertexBuffers(0, 1, &r.lineVB, &stride, &offset);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            context->IASetInputLayout(r.lineLayout);
            context->VSSetShader(r.lineVS, nullptr, 0);
            context->PSSetShader(r.linePS, nullptr, 0);
            context->Draw((UINT)lines.size(), 0);
        }

        /*
         * The game's sky images aren't all sky: zm_towers' has a flat grey cap over its top 40%, flat bright blocks
         * (u 0-0.25, 0.94-1, stair-stepped at their feet) where its arena hides the horizon, and land and trees in its
         * lowest rows. Flat filler is found as exact colors repeated over 0.5% of the image (real sky never repeats one
         * value that often), in the lowest quarter reaching down under the blocks (the strip pasted under their steps,
         * cut flat); land as texels under a fifth of the sky's median in the lowest 15% (trees and buildings
         * read near black, the darkest clouds about half the median). Both grown for their blended / smeared edges.
         * Each row's filler is painted with the sky beside it mirrored in from both sides, crossfaded, the runs taken
         * from the filler merged over 32 rows up and down so stair steps mirror alike (no blocks); rows without sky
         * take the nearest sky row fading to its mean. The image shows where it's sky (alpha: kept off the filler by
         * the blur's reach, then blurred 3 texels across and 8 down, so the filler never bleeds in). The exposure, the
         * ambient's tint and the haze's color are the sky texels' medians.
         */
        void CleanSky(const std::vector<float>& px) {
            const int W{ (int)SKY_W }, H{ (int)SKY_H };
            const size_t n{ (size_t)W * H };
            auto key = [&](size_t i) {
                uint32_t c[3];
                memcpy(c, &px[i * 4], sizeof(c));
                return ((uint64_t)c[0] * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)c[1] * 0xC2B2AE3D27D4EB4Full) ^ c[2];
            };
            std::unordered_map<uint64_t, std::pair<size_t, size_t>> counts{}; // count, first texel
            for (size_t i = 0; i < n; i++) {
                auto& c{ counts.try_emplace(key(i), 0, i).first->second };
                c.first++;
            }
            std::vector<uint8_t> filler(n);
            for (size_t i = 0; i < n; i++) filler[i] = counts[key(i)].first * 200 >= n;
            for (const auto& [k, c] : counts) {
                if (c.first * 200 < n) continue;
                const float* p{ &px[c.second * 4] };
                Logf(LogLevel::INFO, "3d view: sky filler %.3g %.3g %.3g over %.1f%%", p[0], p[1], p[2], c.first * 100.0 / n);
            }
            // under the blocks' feet (the lowest quarter): the art pastes a horizon strip there, cut flat at each step
            for (int x = 0; x < W; x++) {
                bool under{};
                for (int y = H * 3 / 4; y < H; y++) {
                    size_t t{ (size_t)y * W + x };
                    under = under || filler[t];
                    filler[t] = under;
                }
            }
            // the land and trees along the horizon (their own mask: mirrored by row, not merged over rows as the blocks)
            auto lum = [&](size_t i) { return px[i * 4] * 0.3f + px[i * 4 + 1] * 0.59f + px[i * 4 + 2] * 0.11f; };
            std::vector<float> lums{};
            for (size_t i = 0; i < n; i++) {
                if (!filler[i]) lums.push_back(lum(i));
            }
            std::vector<uint8_t> land(n);
            if (!lums.empty()) {
                std::nth_element(lums.begin(), lums.begin() + lums.size() / 2, lums.end());
                float dark{ lums[lums.size() / 2] * 0.2f };
                size_t count{};
                for (size_t i = (size_t)(H * 85 / 100) * W; i < n; i++) {
                    if (filler[i] || lum(i) >= dark) continue;
                    land[i] = 1;
                    count++;
                }
                Logf(LogLevel::INFO, "3d view: sky land / trees %.1f%%", count * 100.0 / n);
            }
            // a mask grown by radii across and down (the rows wrap)
            auto grow = [&](std::vector<uint8_t>& m, int rx, int ry) {
                std::vector<uint8_t> across(n);
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        if (!m[(size_t)y * W + x]) continue;
                        for (int dx = -rx; dx <= rx; dx++) across[(size_t)y * W + (x + dx + W) % W] = 1;
                    }
                }
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        if (!across[(size_t)y * W + x]) continue;
                        for (int yy = std::max(0, y - ry); yy <= std::min(H - 1, y + ry); yy++) m[(size_t)yy * W + x] = 1;
                    }
                }
            };
            // across by 10: the art smears its last texels sideways before the filler (flat per row, blocky mirrored)
            grow(filler, 10, 2);
            grow(land, 2, 2);
            // the runs: the blocks merged over 32 rows up and down, and the land
            const int V{ 32 };
            std::vector<uint8_t> runs(n);
            {
                std::vector<int> pre((size_t)(H + 1) * W);
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) pre[(size_t)(y + 1) * W + x] = pre[(size_t)y * W + x] + filler[(size_t)y * W + x];
                }
                for (int y = 0; y < H; y++) {
                    int lo{ std::max(0, y - V) }, hi{ std::min(H, y + V + 1) };
                    for (int x = 0; x < W; x++) runs[(size_t)y * W + x] = pre[(size_t)hi * W + x] > pre[(size_t)lo * W + x];
                }
            }
            for (size_t i = 0; i < n; i++) {
                filler[i] |= land[i];
                runs[i] |= land[i];
            }

            std::vector<float> fix(n * 4);
            std::vector<std::array<float, 3>> rowMean(H);
            std::vector<uint8_t> rowSky(H);
            std::vector<float> all[3]{}, hor[3]{};
            for (int y = 0; y < H; y++) {
                const uint8_t* f{ &filler[(size_t)y * W] };
                const uint8_t* m{ &runs[(size_t)y * W] };
                const float* p{ &px[(size_t)y * W * 4] };
                float* o{ &fix[(size_t)y * W * 4] };
                double sum[3]{};
                int count{};
                int open{}; // texels outside the runs
                bool band{ y >= H * 3 / 4 && y < H * 19 / 20 };
                for (int x = 0; x < W; x++) {
                    open += !m[x];
                    if (f[x]) continue;
                    for (int k = 0; k < 3; k++) {
                        sum[k] += p[x * 4 + k];
                        o[x * 4 + k] = p[x * 4 + k];
                        all[k].push_back(p[x * 4 + k]);
                        if (band) hor[k].push_back(p[x * 4 + k]);
                    }
                    count++;
                }
                // a sliver of sky would be mirrored over the whole row: such rows are filled as the rows without sky
                if (count < W / 50 || !open) continue;
                rowSky[y] = 1;
                for (int k = 0; k < 3; k++) rowMean[y][k] = (float)(sum[k] / count);
                if (open == W) continue;
                auto at = [&](int x, int k) {
                    x = (x % W + W) % W;
                    return f[x] ? rowMean[y][k] : p[x * 4 + k];
                };
                // the runs, from a texel outside them (the row wraps)
                int start{};
                while (m[start]) start++;
                for (int i = 1; i < W; i++) {
                    int x0{ (start + i) % W };
                    if (!m[x0]) continue;
                    int len{};
                    while (m[(x0 + len) % W]) len++;
                    for (int j = 0; j < len; j++) {
                        float t{ (j + 0.5f) / len };
                        t = t * t * (3 - 2 * t);
                        float* d{ &o[((x0 + j) % W) * 4] };
                        for (int k = 0; k < 3; k++) d[k] = at(x0 - 1 - j, k) * (1 - t) + at(x0 + len + (len - 1 - j), k) * t;
                    }
                    i += len - 1;
                }
            }
            // the rows without sky: the nearest sky row, fading to its mean over 32 rows
            int skyRows{};
            for (int y = 0; y < H; y++) skyRows += rowSky[y];
            if (!skyRows) {
                Log(LogLevel::WARN, "3d view: the sky image is all filler, drawn as it is");
                return;
            }
            for (int y = 0; y < H; y++) {
                if (rowSky[y]) continue;
                int nearest{ -1 };
                for (int dist = 1; nearest < 0; dist++) {
                    if (y - dist >= 0 && rowSky[y - dist]) nearest = y - dist;
                    else if (y + dist < H && rowSky[y + dist]) nearest = y + dist;
                }
                float t{ std::min(1.0f, std::abs(y - nearest) / 32.0f) };
                const float* s{ &fix[(size_t)nearest * W * 4] };
                float* o{ &fix[(size_t)y * W * 4] };
                for (int x = 0; x < W; x++) {
                    for (int k = 0; k < 3; k++) o[x * 4 + k] = s[x * 4 + k] * (1 - t) + rowMean[nearest][k] * t;
                }
            }
            // where the image shows: never over the filler or its grown edge (the blur's reach kept off it too)
            const int R{ 3 }, RV{ 8 };
            std::vector<uint8_t> hidden(filler);
            grow(hidden, 2 + R, 2 + RV);
            std::vector<float> a(n), tmp(n);
            for (size_t i = 0; i < n; i++) a[i] = hidden[i] ? 0.0f : 1.0f;
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    float s{};
                    for (int d = -R; d <= R; d++) s += a[(size_t)y * W + (x + d + W) % W];
                    tmp[(size_t)y * W + x] = s / (2 * R + 1);
                }
            }
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    float s{};
                    for (int d = -RV; d <= RV; d++) s += tmp[(size_t)std::clamp(y + d, 0, H - 1) * W + x];
                    fix[((size_t)y * W + x) * 4 + 3] = s / (2 * RV + 1);
                }
            }

            auto median = [](std::vector<float>& v) {
                if (v.empty()) return 1.0f;
                std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
                return v[v.size() / 2];
            };
            for (int k = 0; k < 3; k++) {
                r.skyAvg[k] = median(all[k]);
                r.skyHorizon[k] = hor[k].empty() ? r.skyAvg[k] : median(hor[k]);
            }
            float mid{ r.skyAvg[0] * 0.3f + r.skyAvg[1] * 0.59f + r.skyAvg[2] * 0.11f };
            r.skyExposure = mid > 1e-8f ? 0.6f / mid : 1;
            Logf(LogLevel::INFO, "3d view: sky %.1f%% sky, %d of %d rows; median %.3g %.3g %.3g (horizon %.3g %.3g %.3g), exposure %.3g",
                all[0].size() * 100.0 / n, skyRows, H, r.skyAvg[0], r.skyAvg[1], r.skyAvg[2], r.skyHorizon[0], r.skyHorizon[1],
                r.skyHorizon[2], r.skyExposure);

            // the sky as drawn (the image over the fix, exposed and tone mapped as SkyColor, into SkyPS's horizon haze)
            r.skyShown.resize(n * 4);
            auto tone = [](float c) { return std::clamp(c / (1 + c) * 1.6f, 0.0f, 1.0f); };
            for (size_t i = 0; i < n; i++) {
                float elev{ (1 - ((float)(i / W) + 0.5f) / H) * XM_PIDIV2 };
                float w{ std::clamp((elev - 0.10f) / (0.28f - 0.10f), 0.0f, 1.0f) };
                w = w * w * (3 - 2 * w);
                for (int k = 0; k < 3; k++) {
                    float c{ tone((fix[i * 4 + k] * (1 - fix[i * 4 + 3]) + px[i * 4 + k] * fix[i * 4 + 3]) * r.skyExposure) };
                    c = tone(r.skyHorizon[k] * r.skyExposure) * (1 - w) + c * w;
                    r.skyShown[i * 4 + k] = (uint8_t)(c * 255 + 0.5f);
                }
                r.skyShown[i * 4 + 3] = 255;
            }

            std::vector<uint16_t> half(n * 4);
            for (size_t i = 0; i < n * 4; i++) half[i] = PackedVector::XMConvertFloatToHalf(fix[i]);
            D3D11_TEXTURE2D_DESC td{};
            td.Width = W;
            td.Height = H;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_IMMUTABLE;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA sd{ half.data(), (UINT)W * 8, 0 };
            ID3D11Texture2D* tex{};
            if (SUCCEEDED(device->CreateTexture2D(&td, &sd, &tex))) {
                device->CreateShaderResourceView(tex, nullptr, &r.skyFix);
                tex->Release();
            }
        }

        // the sky image: its texture, read back at SKY_W x SKY_H for CleanSky
        void LoadSky(const fs::path& path) {
            Release(r.skySrv);
            Release(r.skyFix);
            r.skyShown.clear();
            r.skyPath = path;
            r.skyExposure = 1;
            std::fill(r.skyAvg, r.skyAvg + 3, 1.0f);
            if (path.empty() || !r.skyVS || !r.statsPS) return;
            r.skySrv = LoadDumpedImage(path);
            if (!r.skySrv) {
                Logf(LogLevel::WARN, "3d view: can't read the sky image %s", ToUtf8(path).c_str());
                return;
            }
            D3D11_TEXTURE2D_DESC td{};
            td.Width = SKY_W;
            td.Height = SKY_H;
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            td.SampleDesc.Count = 1;
            td.BindFlags = D3D11_BIND_RENDER_TARGET;
            ID3D11Texture2D* rt{};
            ID3D11RenderTargetView* rtv{};
            ID3D11Texture2D* staging{};
            device->CreateTexture2D(&td, nullptr, &rt);
            if (rt) device->CreateRenderTargetView(rt, nullptr, &rtv);
            td.Usage = D3D11_USAGE_STAGING;
            td.BindFlags = 0;
            td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            device->CreateTexture2D(&td, nullptr, &staging);
            std::vector<float> px{};
            if (rtv && staging) {
                context->OMSetRenderTargets(1, &rtv, nullptr);
                D3D11_VIEWPORT vp{ 0, 0, (float)SKY_W, (float)SKY_H, 0, 1 };
                context->RSSetViewports(1, &vp);
                context->RSSetState(r.raster);
                context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
                context->OMSetDepthStencilState(r.noDepth, 0);
                context->IASetInputLayout(nullptr);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context->VSSetShader(r.skyVS, nullptr, 0);
                context->PSSetShader(r.statsPS, nullptr, 0);
                context->PSSetShaderResources(1, 1, &r.skySrv);
                context->PSSetSamplers(2, 1, &r.skySampler);
                context->Draw(3, 0);
                context->CopyResource(staging, rt);
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
                    px.resize((size_t)SKY_W * SKY_H * 4);
                    for (UINT y = 0; y < SKY_H; y++) {
                        memcpy(&px[(size_t)y * SKY_W * 4], (const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch, (size_t)SKY_W * 16);
                    }
                    context->Unmap(staging, 0);
                }
            }
            Release(staging);
            Release(rtv);
            Release(rt);
            ID3D11ShaderResourceView* none{};
            context->PSSetShaderResources(1, 1, &none);
            auto t0{ std::chrono::steady_clock::now() };
            if (!px.empty()) CleanSky(px);
            Logf(LogLevel::INFO, "3d view: sky cleaned in %.2fs", std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count());
        }

        // the map's lighting into the look: the state's block 0 sun, the json's probe color, light grid and first sky
        void UpdateLook(App& app) {
            Lighting& l{ app.lighting };
            if (!l.loaded && app.project.IsOpen()) l.Load(app.project);
            if (LightingState* s{ l.State(0) }; s && s->data.size() >= 0x50 + 0x18) {
                const float* f{ reinterpret_cast<const float*>(s->data.data() + 0x50) };
                look.sunPitch = f[0];
                look.sunYaw = f[1];
                float m{ std::max({ f[2], f[3], f[4], 1e-4f }) };
                look.sunColor = XMFLOAT3(f[2] / m, f[3] / m, f[4] / m);
                look.sunIlluminance = f[5];
            }
            float p{ XMConvertToRadians(look.sunPitch) };
            float y{ XMConvertToRadians(look.sunYaw) };
            look.lightDir = XMFLOAT3(std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), -std::sin(p));
            auto num = [](const json& j, float def) { return j.is_number() ? j.get<float>() : def; };
            // the probes' tint (their brightness goes with the grid: the night's are tiny, its reflections dim)
            float probeLum{ 1 };
            if (l.root.contains("probeColor") && l.root["probeColor"].is_array() && l.root["probeColor"].size() >= 3) {
                XMFLOAT3 pc{ num(l.root["probeColor"][0], 1), num(l.root["probeColor"][1], 1), num(l.root["probeColor"][2], 1) };
                float m{ std::max({ pc.x, pc.y, pc.z, 1e-12f }) };
                look.probe = XMFLOAT3(pc.x / m, pc.y / m, pc.z / m);
                probeLum = 0.2126f * pc.x + 0.7152f * pc.y + 0.0722f * pc.z;
            }
            float grid{ 300 };
            if (l.root.contains("lightGrid") && l.root["lightGrid"].is_array() && l.root["lightGrid"].size() >= 3) {
                grid = (num(l.root["lightGrid"][0], 0) + num(l.root["lightGrid"][1], 0) + num(l.root["lightGrid"][2], 0)) / 3;
            }
            // the ambient (grid x probe) against the sun's light (the game exposes the two together), kept readable
            look.mapAmbient = look.sunIlluminance > 0 ? std::clamp(grid * probeLum * XM_PI / look.sunIlluminance * 4, 0.25f, 0.7f) : 0.3f;
            fs::path skyImage{};
            look.skyRotation = 0;
            if (l.root.contains("sky")) {
                const json& sky{ l.root["sky"].is_array() && !l.root["sky"].empty() ? l.root["sky"][0] : l.root["sky"] };
                if (sky.is_object()) {
                    if (sky.contains("image") && sky["image"].is_string()) skyImage = world.assets.Find(scene::AssetType::IMAGE, sky["image"].get<std::string>());
                    look.skyRotation = num(sky.value("rotation", json{}), 0);
                }
            }
            if (skyImage != r.skyPath) LoadSky(skyImage);
        }

        // the sun's view over the whole scene (its bounds' sphere), orthographic
        XMMATRIX LightViewProj() {
            XMVECTOR mn{ XMVectorSet(world.mins[0], world.mins[1], world.mins[2], 0) };
            XMVECTOR mx{ XMVectorSet(world.maxs[0], world.maxs[1], world.maxs[2], 0) };
            XMVECTOR c{ (mn + mx) * 0.5f };
            float radius{ std::max(256.0f, XMVectorGetX(XMVector3Length(mx - mn)) * 0.5f) };
            XMVECTOR l{ XMVector3Normalize(XMLoadFloat3(&look.lightDir)) };
            XMVECTOR up{ std::abs(XMVectorGetZ(l)) > 0.99f ? XMVectorSet(1, 0, 0, 0) : XMVectorSet(0, 0, 1, 0) };
            XMMATRIX view{ XMMatrixLookToRH(c - l * radius * 2, l, up) };
            return view * XMMatrixOrthographicRH(radius * 2, radius * 2, radius * 0.5f, radius * 3.5f);
        }

        // the shadow map: every solid part's depth from the sun
        void ShadowPass() {
            ID3D11ShaderResourceView* none{};
            context->PSSetShaderResources(2, 1, &none);
            context->OMSetRenderTargets(0, nullptr, r.shadowDsv);
            context->ClearDepthStencilView(r.shadowDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            D3D11_VIEWPORT vp{ 0, 0, (float)SHADOW_SIZE, (float)SHADOW_SIZE, 0, 1 };
            context->RSSetViewports(1, &vp);
            context->RSSetState(r.rasterShadow);
            context->OMSetDepthStencilState(r.depth, 0);
            context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
            context->IASetInputLayout(r.meshLayout);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->VSSetShader(r.depthVS, nullptr, 0);
            context->PSSetShader(nullptr, nullptr, 0);
            for (const Instance& inst : world.instances) {
                if (inst.mesh < 0 || inst.mesh >= (int)r.meshes.size() || !Visible(inst)) continue;
                const GpuMesh& g{ r.meshes[inst.mesh] };
                if (!g.vb || !g.ib) continue;
                UINT stride{ sizeof(scene::Vertex) };
                UINT offset{};
                context->IASetVertexBuffers(0, 1, &g.vb, &stride, &offset);
                context->IASetIndexBuffer(g.ib, DXGI_FORMAT_R32_UINT, 0);
                const float white[3]{ 1, 1, 1 };
                SetObject(WorldOf(inst), white, false, false);
                for (const GpuPart& p : g.parts) {
                    if (p.alpha != scene::Alpha::BLEND) context->DrawIndexed(p.count, p.start, 0);
                }
            }
        }

        void Render(int w, int h) {
            if (!MakeTargets(w, h)) return;
            bool game{ look.game };
            bool shadows{ game && look.shadows && r.shadowDsv && r.depthVS };
            bool sky{ game && look.sky && r.skyVS && r.skyPS };

            FrameConstants fc{};
            XMMATRIX viewProj{ ViewProj((float)w / h) };
            XMStoreFloat4x4(&fc.viewProj, viewProj);
            XMStoreFloat4x4(&fc.invViewProj, XMMatrixInverse(nullptr, viewProj));
            XMStoreFloat4x4(&fc.lightViewProj, LightViewProj());
            XMVECTOR light{ game ? XMVector3Normalize(XMLoadFloat3(&look.lightDir)) : XMVector3Normalize(XMVectorSet(-0.45f, -0.35f, -0.82f, 0)) };
            XMStoreFloat4(&fc.lightDir, light);
            fc.camPos = XMFLOAT4(cam.pos.x, cam.pos.y, cam.pos.z, 1);
            fc.sunColor = XMFLOAT4(look.sunColor.x * look.sun, look.sunColor.y * look.sun, look.sunColor.z * look.sun, 0);
            // the ambient: the probe color, half tinted by the sky's average
            float m{ std::max({ r.skyAvg[0], r.skyAvg[1], r.skyAvg[2], 1e-6f }) };
            float tint[3]{};
            for (int k = 0; k < 3; k++) tint[k] = r.skySrv ? 0.5f + 0.5f * r.skyAvg[k] / m : 1.0f;
            float amb{ look.ambient >= 0 ? look.ambient : look.mapAmbient };
            fc.ambient = XMFLOAT4(look.probe.x * tint[0] * amb, look.probe.y * tint[1] * amb, look.probe.z * tint[2] * amb, 0);
            // the sky image without its model: a lat-long, turned by its rotation
            fc.sky = XMFLOAT4(-look.skyRotation / 360.0f, -1, r.skyExposure * look.skyBright, 1.0f / SHADOW_SIZE);
            // the haze's color: the sky's at the horizon as drawn (exposed and tone mapped as the sky)
            float hz[3]{};
            float hm{};
            for (int k = 0; k < 3; k++) {
                float c{ r.skyHorizon[k] * r.skyExposure * look.skyBright };
                hz[k] = r.skySrv ? std::min(1.0f, c / (1 + c) * 1.6f) : r.skyHorizon[k];
                hm = std::max(hm, hz[k]);
            }
            fc.horizon = XMFLOAT4(hz[0], hz[1], hz[2], 0);
            fc.flags = XMFLOAT4(game ? 1.0f : 0.0f, shadows ? 1.0f : 0.0f, game && look.haze ? 1.0f : 0.0f, r.skySrv ? 1.0f : 0.0f);
            fc.haze = XMFLOAT4(800, 0.00025f, 0.7f, r.skySrv && r.skyFix ? 1.0f : 0.0f);
            context->UpdateSubresource(r.frameCB, 0, nullptr, &fc, 0, 0);

            ID3D11Buffer* cbs[]{ r.frameCB, r.objectCB };
            context->VSSetConstantBuffers(0, 2, cbs);
            context->PSSetConstantBuffers(0, 2, cbs);
            ID3D11SamplerState* samplers[]{ r.sampler, r.shadowSampler, r.skySampler };
            context->PSSetSamplers(0, 3, samplers);
            context->GSSetShader(nullptr, nullptr, 0);
            if (shadows) ShadowPass();

            const float clear[4]{ 0.17f, 0.19f, 0.22f, 1 };
            context->OMSetRenderTargets(1, &r.rtv, r.dsv);
            context->ClearRenderTargetView(r.rtv, clear);
            context->ClearDepthStencilView(r.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            D3D11_VIEWPORT vp{ 0, 0, (float)w, (float)h, 0, 1 };
            context->RSSetViewports(1, &vp);
            context->RSSetState(r.raster);
            context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
            context->PSSetShaderResources(1, 1, &r.skySrv);
            if (sky) {
                context->OMSetDepthStencilState(r.noDepth, 0);
                context->IASetInputLayout(nullptr);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context->VSSetShader(r.skyVS, nullptr, 0);
                context->PSSetShader(r.skyPS, nullptr, 0);
                context->PSSetShaderResources(3, 1, &r.skyFix);
                context->Draw(3, 0);
            }
            if (shadows) context->PSSetShaderResources(2, 1, &r.shadowSrv);
            context->OMSetDepthStencilState(r.depth, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->IASetInputLayout(r.meshLayout);
            context->VSSetShader(r.meshVS, nullptr, 0);
            context->PSSetShader(r.meshPS, nullptr, 0);

            // the solid and cut out parts, then the blended ones over them
            for (int pass = 0; pass < 2; pass++) {
                if (pass == 1) {
                    context->OMSetBlendState(r.blend, nullptr, 0xffffffff);
                    context->OMSetDepthStencilState(r.depthRead, 0);
                }
                for (size_t i = 0; i < world.instances.size(); i++) {
                    const Instance& inst{ world.instances[i] };
                    if (!Visible(inst)) continue;
                    XMMATRIX m{ WorldOf(inst) };
                    bool sel{ (int)i == selected || BoardOfSelected(inst) };
                    if (inst.mesh >= 0 && inst.mesh < (int)r.meshes.size()) {
                        DrawMesh(r.meshes[inst.mesh], m, inst.color, sel, pass == 1);
                    }
                    else if (pass == 1 && !HasBoards(inst)) {
                        DrawMarker(BoxOf(inst) * m, inst.color, sel);
                    }
                }
            }
            context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
            context->OMSetDepthStencilState(r.depth, 0);

            // the lines: the grid, the axes, the selection's box
            std::vector<LineVertex> lines{};
            if (showGrid) {
                const float extent{ 4096 };
                for (float v = -extent; v <= extent; v += 128) {
                    bool major{ std::fmod(std::abs(v), 1024.0f) < 0.5f };
                    uint32_t c{ major ? Rgba(0.55f, 0.58f, 0.62f, 1) : Rgba(0.32f, 0.34f, 0.37f, 1) };
                    Line(lines, XMVectorSet(v, -extent, 0.4f, 1), XMVectorSet(v, extent, 0.4f, 1), c);
                    Line(lines, XMVectorSet(-extent, v, 0.4f, 1), XMVectorSet(extent, v, 0.4f, 1), c);
                }
            }
            Line(lines, XMVectorSet(0, 0, 0.6f, 1), XMVectorSet(96, 0, 0.6f, 1), Rgba(1, 0.3f, 0.3f, 1));
            Line(lines, XMVectorSet(0, 0, 0.6f, 1), XMVectorSet(0, 96, 0.6f, 1), Rgba(0.3f, 1, 0.3f, 1));
            Line(lines, XMVectorSet(0, 0, 0.6f, 1), XMVectorSet(0, 0, 96, 1), Rgba(0.35f, 0.5f, 1, 1));
            if (selected >= 0 && selected < (int)world.instances.size()) {
                // a barrier's: its boards' boxes (its marker isn't drawn)
                const Instance& s{ world.instances[selected] };
                if (HasBoards(s)) {
                    for (const Instance& b : world.instances) if (BoardOfSelected(b)) BoxLines(lines, b, Rgba(1, 0.85f, 0.2f, 1));
                }
                else BoxLines(lines, s, Rgba(1, 0.85f, 0.2f, 1));
            }
            DrawLines(lines);

            // the gizmo over everything, its arrows' tips as cubes
            if (const Instance* inst{ SelectedInstance() }; inst && GizmoOn(AllowedFor(*inst))) {
                std::vector<LineVertex> g{};
                GizmoLines(*inst, g);
                context->OMSetDepthStencilState(r.noDepth, 0);
                DrawLines(g);
                context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                context->IASetInputLayout(r.meshLayout);
                context->VSSetShader(r.meshVS, nullptr, 0);
                context->PSSetShader(r.meshPS, nullptr, 0);
                float s{ GizmoSize(*inst) };
                // the scale handles' tips bigger than the arrows'
                float c{ s * (gizmoMode == GizmoMode::SCALE ? 0.13f : 0.07f) };
                Allowed al{ AllowedFor(*inst) };
                for (Handle h : { Handle::X, Handle::Y, Handle::Z }) {
                    if (!HandleOn(al, h)) continue;
                    XMVECTOR tip{ GizmoAt(*inst) + HandleAxis(*inst, h) * s };
                    uint32_t col{ HandleColor(h) };
                    const float rgb[3]{ (col & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f, ((col >> 16) & 0xFF) / 255.0f };
                    XMMATRIX m{ XMMatrixTranslation(-0.5f, -0.5f, -0.5f) * XMMatrixScaling(c, c, c) * XMMatrixTranslationFromVector(tip) };
                    DrawMesh(r.cube, m, rgb, false);
                }
                context->OMSetDepthStencilState(r.depth, 0);
            }

            ID3D11RenderTargetView* none{};
            context->OMSetRenderTargets(1, &none, nullptr);
            // the shadow map unbound: the next frame's pass writes it
            ID3D11ShaderResourceView* noSrv{};
            context->PSSetShaderResources(2, 1, &noSrv);
            context->ResolveSubresource(r.resolved, 0, r.msColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        }

        // the ray through a point of the view, from the near plane, its t of 1 at the far plane
        void Ray(float nx, float ny, float aspect, XMVECTOR& origin, XMVECTOR& dir) {
            XMMATRIX inv{ XMMatrixInverse(nullptr, ViewProj(aspect)) };
            XMVECTOR a{ XMVector3TransformCoord(XMVectorSet(nx, ny, 0, 1), inv) };
            XMVECTOR b{ XMVector3TransformCoord(XMVectorSet(nx, ny, 1, 1), inv) };
            origin = a;
            dir = b - a;
        }

        bool RayBox(const float o[3], const float d[3], const float mins[3], const float maxs[3], float& tmin) {
            float t0{ 0 };
            float t1{ 1 };
            for (int k = 0; k < 3; k++) {
                if (std::abs(d[k]) < 1e-12f) {
                    if (o[k] < mins[k] || o[k] > maxs[k]) return false;
                    continue;
                }
                float a{ (mins[k] - o[k]) / d[k] };
                float b{ (maxs[k] - o[k]) / d[k] };
                if (a > b) std::swap(a, b);
                t0 = std::max(t0, a);
                t1 = std::min(t1, b);
                if (t0 > t1) return false;
            }
            tmin = t0;
            return true;
        }

        bool RayTriangle(const float o[3], const float d[3], const float* a, const float* b, const float* c, float& t) {
            float e1[3]{ b[0] - a[0], b[1] - a[1], b[2] - a[2] };
            float e2[3]{ c[0] - a[0], c[1] - a[1], c[2] - a[2] };
            float p[3]{ d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
            float det{ e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2] };
            if (std::abs(det) < 1e-12f) return false;
            float inv{ 1.0f / det };
            float s[3]{ o[0] - a[0], o[1] - a[1], o[2] - a[2] };
            float u{ (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv };
            if (u < 0 || u > 1) return false;
            float q[3]{ s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
            float v{ (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv };
            if (v < 0 || u + v > 1) return false;
            t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
            return t >= 0;
        }

        // the nearest visible instance under the ray: a mesh's triangles, a marker's box
        int Pick(XMVECTOR origin, XMVECTOR dir) {
            int best{ -1 };
            float bestT{ FLT_MAX };
            for (size_t i = 0; i < world.instances.size(); i++) {
                const Instance& inst{ world.instances[i] };
                if (!Visible(inst) || HasBoards(inst)) continue;
                XMMATRIX inv{ XMMatrixInverse(nullptr, WorldOf(inst)) };
                XMVECTOR lo{ XMVector3TransformCoord(origin, inv) };
                XMVECTOR ld{ XMVector3TransformNormal(dir, inv) };
                float o[3]{ XMVectorGetX(lo), XMVectorGetY(lo), XMVectorGetZ(lo) };
                float d[3]{ XMVectorGetX(ld), XMVectorGetY(ld), XMVectorGetZ(ld) };
                float t{};
                if (!RayBox(o, d, inst.mins, inst.maxs, t) || t >= bestT) continue;
                if (inst.mesh >= 0) {
                    const scene::Mesh& m{ world.meshes[inst.mesh] };
                    float hit{ FLT_MAX };
                    for (size_t k = 0; k + 2 < m.indices.size(); k += 3) {
                        float tt{};
                        if (RayTriangle(o, d, m.vertices[m.indices[k]].pos, m.vertices[m.indices[k + 1]].pos, m.vertices[m.indices[k + 2]].pos, tt) && tt < hit) {
                            hit = tt;
                        }
                    }
                    if (hit == FLT_MAX) continue;
                    t = hit;
                }
                if (t < bestT) {
                    bestT = t;
                    best = (int)i;
                }
            }
            return best;
        }

        // the markers' icons: what an object without a drawable mesh is, a badge over its box
        enum class Icon { CUBE, PLAYER, ZOMBIE, DOG, SPAWN, BARRIER, BOX, PAP, PERK, WALLBUY, ITEM, TRIGGER, STRUCT, SCENE, ENDING };
        struct IconInfo {
            Icon icon;
            const char* what;
        };

        std::string Field(const Instance& inst, const char* key) {
            auto it{ inst.data.find(key) };
            return it != inst.data.end() && it->is_string() ? it->get<std::string>() : std::string{};
        }

        bool Has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

        IconInfo IconOf(const Instance& inst) {
            std::string cls{ Field(inst, "classname") };
            std::string tn{ Field(inst, "targetname") };
            std::string nw{ Field(inst, "script_noteworthy") };
            std::string ss{ Field(inst, "script_string") };
            if (Has(cls, "scene") || Has(tn, "scene") || Has(nw, "scene")) return { Icon::SCENE, "scene" };
            if (nw == "initial_spawn") return { Icon::PLAYER, "player spawn" };
            if (Has(cls, "actor_spawner") && Has(cls, "dog")) return { Icon::DOG, "dog spawner" };
            if (Has(cls, "actor_spawner")) return { Icon::ZOMBIE, "zombie spawner" };
            if (nw == "dog_location") return { Icon::DOG, "dog spawn spot" };
            if (Has(nw, "riser") || Has(tn, "_spawns")) return { Icon::SPAWN, "zombie spawn spot" };
            if (Has(cls, "magicbox") || tn == "treasure_chest_use" || Has(tn, "start_chest")) return { Icon::BOX, "mystery box" };
            if (Has(cls, "packapunch") || Has(tn, "pack_a_punch")) return { Icon::PAP, "Pack-a-Punch" };
            if (Has(cls, "zbarrier") || Has(ss, "window") || Has(tn, "barrier") || tn == "exterior_goal") return { Icon::BARRIER, "window barrier" };
            if (Has(tn, "perk")) return { Icon::PERK, "perk" };
            if (tn == "weapon_upgrade" || Has(tn, "wallbuy")) return { Icon::WALLBUY, "wall-buy" };
            if (Has(cls, "weapon_")) return { Icon::ITEM, "item" };
            if (Has(cls, "trigger")) return { Icon::TRIGGER, "trigger" };
            if (Has(tn, "ending")) return { Icon::ENDING, "ending" };
            if (inst.kind == Kind::SCRIPT_MODEL) return { Icon::CUBE, "script model (no mesh read)" };
            if (inst.kind == Kind::SPAWNED) return { Icon::CUBE, "spawned by the map script (no mesh read)" };
            if (inst.kind == Kind::STATIC_MODEL) return { Icon::CUBE, "static model (no mesh read)" };
            return { Icon::STRUCT, "script struct" };
        }

        // the short name over an icon: the struct's role when its targetname is a group's
        std::string ShortName(const Instance& inst) {
            std::string tn{ Field(inst, "targetname") };
            std::string nw{ Field(inst, "script_noteworthy") };
            if (!nw.empty() && (tn.empty() || Has(tn, "_spawns") || Has(tn, "_barrier"))) return nw;
            if (!tn.empty()) return tn;
            std::string cls{ Field(inst, "classname") };
            return cls.empty() ? inst.name : cls;
        }

        void DrawIcon(ImDrawList* dl, ImVec2 c, float s, Icon icon, ImU32 accent, bool hot) {
            const ImU32 bg{ IM_COL32(18, 20, 24, 225) };
            const ImU32 fg{ IM_COL32(238, 238, 238, 255) };
            dl->AddCircleFilled(c, s, bg);
            dl->AddCircle(c, s, accent, 0, hot ? 3.0f : 2.0f);
            float u{ s * 0.62f };
            auto P = [&](float x, float y) { return ImVec2(c.x + x * u, c.y + y * u); };
            switch (icon) {
            case Icon::PLAYER:
                dl->AddCircleFilled(P(0, -0.42f), 0.3f * u, fg);
                dl->AddRectFilled(P(-0.5f, -0.02f), P(0.5f, 0.72f), fg, 0.4f * u, ImDrawFlags_RoundCornersTop);
                break;
            case Icon::ZOMBIE:
                dl->AddCircleFilled(P(0, -0.15f), 0.58f * u, fg);
                dl->AddRectFilled(P(-0.32f, 0.2f), P(0.32f, 0.68f), fg, 0.1f * u);
                dl->AddCircleFilled(P(-0.24f, -0.15f), 0.17f * u, bg);
                dl->AddCircleFilled(P(0.24f, -0.15f), 0.17f * u, bg);
                dl->AddLine(P(-0.12f, 0.45f), P(-0.12f, 0.68f), bg, 1.5f);
                dl->AddLine(P(0.12f, 0.45f), P(0.12f, 0.68f), bg, 1.5f);
                break;
            case Icon::DOG:
                dl->AddCircleFilled(P(0, 0.28f), 0.36f * u, fg);
                dl->AddCircleFilled(P(-0.5f, -0.18f), 0.17f * u, fg);
                dl->AddCircleFilled(P(-0.18f, -0.5f), 0.17f * u, fg);
                dl->AddCircleFilled(P(0.18f, -0.5f), 0.17f * u, fg);
                dl->AddCircleFilled(P(0.5f, -0.18f), 0.17f * u, fg);
                break;
            case Icon::SPAWN:
                dl->AddRectFilled(P(-0.18f, -0.7f), P(0.18f, 0.05f), fg);
                dl->AddTriangleFilled(P(-0.55f, 0.0f), P(0.55f, 0.0f), P(0, 0.7f), fg);
                break;
            case Icon::BARRIER:
                for (float y : { -0.62f, -0.15f, 0.32f }) dl->AddRectFilled(P(-0.7f, y), P(0.7f, y + 0.3f), fg, 0.08f * u);
                break;
            case Icon::BOX: {
                dl->AddRect(P(-0.62f, -0.62f), P(0.62f, 0.62f), fg, 0.12f * u, 0, 2.0f);
                ImVec2 t{ ImGui::CalcTextSize("?") };
                dl->AddText(ImVec2(c.x - t.x / 2, c.y - t.y / 2), fg, "?");
                break;
            }
            case Icon::PAP: {
                ImVec2 t{ ImGui::CalcTextSize("P") };
                dl->AddText(ImVec2(c.x - t.x / 2, c.y - t.y / 2), fg, "P");
                dl->AddCircle(c, 0.75f * u, fg, 0, 1.5f);
                break;
            }
            case Icon::PERK:
                dl->AddRectFilled(P(-0.38f, -0.12f), P(0.38f, 0.72f), fg, 0.18f * u);
                dl->AddRectFilled(P(-0.15f, -0.58f), P(0.15f, -0.1f), fg);
                dl->AddRectFilled(P(-0.22f, -0.72f), P(0.22f, -0.56f), fg);
                break;
            case Icon::WALLBUY:
                dl->AddRectFilled(P(-0.72f, -0.32f), P(0.72f, 0.02f), fg, 0.06f * u);
                dl->AddRectFilled(P(-0.5f, 0.0f), P(-0.18f, 0.6f), fg, 0.06f * u);
                dl->AddRectFilled(P(0.05f, 0.0f), P(0.25f, 0.28f), fg);
                break;
            case Icon::ITEM:
                dl->AddNgon(c, 0.6f * u, fg, 6, 2.0f);
                dl->AddCircleFilled(c, 0.2f * u, fg);
                break;
            case Icon::TRIGGER:
                for (int side = 0; side < 4; side++) {
                    for (int k = 0; k < 3; k++) {
                        float a{ -0.62f + k * 0.48f }, b{ a + 0.3f };
                        ImVec2 p0{}, p1{};
                        if (side == 0) { p0 = P(a, -0.62f); p1 = P(b, -0.62f); }
                        if (side == 1) { p0 = P(a, 0.62f); p1 = P(b, 0.62f); }
                        if (side == 2) { p0 = P(-0.62f, a); p1 = P(-0.62f, b); }
                        if (side == 3) { p0 = P(0.62f, a); p1 = P(0.62f, b); }
                        dl->AddLine(p0, p1, fg, 2.0f);
                    }
                }
                break;
            case Icon::SCENE:
                dl->AddRectFilled(P(-0.62f, -0.12f), P(0.62f, 0.62f), fg, 0.08f * u);
                dl->AddRectFilled(P(-0.62f, -0.5f), P(0.62f, -0.24f), fg);
                for (float x : { -0.4f, 0.0f, 0.4f }) dl->AddLine(P(x, -0.5f), P(x + 0.2f, -0.24f), bg, 2.0f);
                break;
            case Icon::ENDING:
                dl->AddLine(P(-0.45f, -0.7f), P(-0.45f, 0.7f), fg, 2.0f);
                dl->AddTriangleFilled(P(-0.42f, -0.7f), P(0.6f, -0.38f), P(-0.42f, -0.06f), fg);
                break;
            case Icon::STRUCT:
                dl->AddQuadFilled(P(0, -0.6f), P(0.6f, 0), P(0, 0.6f), P(-0.6f, 0), fg);
                break;
            default:
                // a cube: its outline and the edges to its near corner
                dl->AddNgon(c, 0.65f * u, fg, 6, 1.5f);
                for (int k = 0; k < 3; k++) {
                    float a{ XM_PI / 6 + k * XM_PI * 2 / 3 };
                    dl->AddLine(c, ImVec2(c.x + std::cos(a) * 0.65f * u, c.y + std::sin(a) * 0.65f * u), fg, 1.5f);
                }
                break;
            }
        }

        // the icons on screen this frame, far to near
        struct ScreenIcon {
            int index;
            ImVec2 pos;
            float distance;
        };
        std::vector<ScreenIcon> screenIcons{};
        int hoveredIcon{ -1 };
        bool showIcons{ true };

        void LayoutIcons(const XMMATRIX& vp, ImVec2 origin, float w, float h, ImVec2 mouse, bool hovered, float radius) {
            screenIcons.clear();
            hoveredIcon = -1;
            if (!showIcons) return;
            XMVECTOR eye{ XMLoadFloat3(&cam.pos) };
            for (size_t i = 0; i < world.instances.size(); i++) {
                const Instance& inst{ world.instances[i] };
                if (inst.mesh >= 0 || !Visible(inst)) continue;
                XMVECTOR top{ XMVectorSet(inst.origin[0], inst.origin[1], inst.origin[2] + inst.maxs[2] * inst.scale, 1) };
                float d{ XMVectorGetX(XMVector3Length(top - eye)) };
                if (d > 6000) continue;
                ImVec2 p{};
                if (!ToScreen(vp, top, origin, w, h, p)) continue;
                p.y -= radius + 2;
                if (p.x < origin.x - radius || p.y < origin.y - radius || p.x > origin.x + w + radius || p.y > origin.y + h + radius) continue;
                screenIcons.push_back({ (int)i, p, d });
            }
            std::sort(screenIcons.begin(), screenIcons.end(), [](const ScreenIcon& a, const ScreenIcon& b) { return a.distance > b.distance; });
            if (!hovered) return;
            for (const ScreenIcon& si : screenIcons) {
                float dx{ mouse.x - si.pos.x }, dy{ mouse.y - si.pos.y };
                if (dx * dx + dy * dy <= (radius + 2) * (radius + 2)) hoveredIcon = si.index; // the nearest wins (last)
            }
        }

        void DrawIcons(ImDrawList* dl, float radius) {
            ImFont* font{ ImGui::GetFont() };
            float fontSize{ ImGui::GetFontSize() * 0.85f };
            for (const ScreenIcon& si : screenIcons) {
                const Instance& inst{ world.instances[si.index] };
                IconInfo info{ IconOf(inst) };
                bool hot{ si.index == hoveredIcon || si.index == selected };
                ImU32 accent{ IM_COL32((int)(inst.color[0] * 255), (int)(inst.color[1] * 255), (int)(inst.color[2] * 255), 255) };
                if (si.index == selected) accent = IM_COL32(255, 220, 60, 255);
                DrawIcon(dl, si.pos, radius, info.icon, accent, hot);
                // its name when near, hovered or selected
                if (hot || si.distance < 700) {
                    std::string name{ ShortName(inst) };
                    ImVec2 t{ font->CalcTextSizeA(fontSize, FLT_MAX, 0, name.c_str()) };
                    ImVec2 at{ si.pos.x - t.x / 2, si.pos.y + radius + 2 };
                    dl->AddRectFilled(ImVec2(at.x - 3, at.y - 1), ImVec2(at.x + t.x + 3, at.y + t.y + 1), IM_COL32(18, 20, 24, 170), 3);
                    dl->AddText(font, fontSize, at, hot ? IM_COL32(255, 235, 150, 255) : IM_COL32(225, 225, 225, 235), name.c_str());
                }
            }
        }

        const char* KindName(Kind k) {
            switch (k) {
            case Kind::STATIC_MODEL: return "static model (gfx_map)";
            case Kind::SCRIPT_MODEL: return "script model (entity list)";
            case Kind::SPAWNED: return "spawned by the map script";
            default: return "entity (entity list)";
            }
        }

        /*
         * The selected object's config: the json objects it's made from (its binding's targets, its own entry), read
         * from their files and edited as trees (strings, numbers, booleans, 2-4 number arrays on one row; right click a
         * key to remove it), each saved on its own: its binding's tools run after, then the map reloads.
         */
        struct ConfigView {
            int instance{ -1 };
            size_t generation{ (size_t)-1 };
            std::vector<scene::ConfigEntry> entries{};
            std::vector<json> values{}; // as edited
            std::vector<json> saved{};  // as the file has it
            std::vector<std::string> errors{};
            std::vector<std::string> newKeys{};
        } config{};

        void LoadConfig() {
            config = {};
            config.instance = selected;
            config.generation = builtGeneration;
            const Instance* inst{ SelectedInstance() };
            if (!inst) return;
            config.entries = world.ConfigOf(*inst);
            for (const scene::ConfigEntry& e : config.entries) {
                std::string error{};
                json v{ scene::ReadConfig(e, error) };
                config.values.push_back(v);
                config.saved.push_back(v);
                config.errors.push_back(error);
                config.newKeys.emplace_back();
            }
        }

        // 2 to 4 numbers: one row of fields
        bool SmallVector(const json& v) {
            if (!v.is_array() || v.size() < 2 || v.size() > 4) return false;
            for (const json& e : v) {
                if (!e.is_number()) return false;
            }
            return true;
        }

        bool JsonChildren(json& v);

        // a scalar's (or a small vector's) field: true when it changed
        bool JsonValue(json& v) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (v.is_string()) return ImGui::InputText("##v", v.get_ptr<std::string*>());
            if (v.is_boolean()) {
                bool b{ v.get<bool>() };
                if (!ImGui::Checkbox("##v", &b)) return false;
                v = b;
                return true;
            }
            if (v.is_number_integer()) {
                int64_t x{ v.get<int64_t>() };
                if (!ImGui::DragScalar("##v", ImGuiDataType_S64, &x, 1.0f)) return false;
                v = x;
                return true;
            }
            if (v.is_number_float()) {
                double d{ v.get<double>() };
                if (!ImGui::DragScalar("##v", ImGuiDataType_Double, &d, 0.1f, nullptr, nullptr, "%g")) return false;
                v = d;
                return true;
            }
            if (SmallVector(v)) {
                double d[4]{};
                for (size_t k = 0; k < v.size(); k++) d[k] = v[k].get<double>();
                if (!ImGui::DragScalarN("##v", ImGuiDataType_Double, d, (int)v.size(), 0.5f, nullptr, nullptr, "%g")) return false;
                // whole numbers stay integers where they were
                for (size_t k = 0; k < v.size(); k++) {
                    if (v[k].is_number_integer() && d[k] == std::floor(d[k])) v[k] = (int64_t)d[k];
                    else v[k] = d[k];
                }
                return true;
            }
            ImGui::TextDisabled("%s", v.is_null() ? "null" : v.dump().c_str());
            return false;
        }

        // one key's row: the key (a tree node for objects and arrays) and its value
        bool JsonRow(const std::string& key, json& v, bool& remove) {
            bool changed{};
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            bool container{ (v.is_object() || v.is_array()) && !SmallVector(v) };
            bool open{};
            if (container) open = ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            else ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
            if (ImGui::BeginPopupContextItem("##key")) {
                if (ImGui::MenuItem("Remove")) remove = true;
                ImGui::EndPopup();
            }
            ImGui::TableSetColumnIndex(1);
            if (container) {
                ImGui::TextDisabled(v.is_object() ? "{ %zu }" : "[ %zu ]", v.size());
                if (open) {
                    changed |= JsonChildren(v);
                    ImGui::TreePop();
                }
            }
            else {
                changed |= JsonValue(v);
            }
            return changed;
        }

        bool JsonChildren(json& v) {
            bool changed{};
            if (v.is_object()) {
                std::string remove{};
                for (auto it{ v.begin() }; it != v.end(); ++it) {
                    ImGui::PushID(it.key().c_str());
                    bool rm{};
                    changed |= JsonRow(it.key(), it.value(), rm);
                    if (rm) remove = it.key();
                    ImGui::PopID();
                }
                if (!remove.empty()) {
                    v.erase(remove);
                    changed = true;
                }
            }
            else if (v.is_array()) {
                int remove{ -1 };
                for (size_t i = 0; i < v.size(); i++) {
                    ImGui::PushID((int)i);
                    bool rm{};
                    changed |= JsonRow("[" + std::to_string(i) + "]", v[i], rm);
                    if (rm) remove = (int)i;
                    ImGui::PopID();
                }
                if (remove >= 0) {
                    v.erase((size_t)remove);
                    changed = true;
                }
            }
            return changed;
        }

        void DrawConfig(App& app) {
            if (config.instance != selected || config.generation != builtGeneration) LoadConfig();
            const Instance* inst{ SelectedInstance() };
            if (!inst) return;
            ImGui::SeparatorText("Config");
            if (config.entries.empty()) {
                ImGui::TextDisabled("Nothing in a file to edit (it's from the map script's layout).");
                return;
            }
            bool busy{ app.scripts.running };
            size_t moved{ world.MovedCount() };
            for (size_t i = 0; i < config.entries.size(); i++) {
                const scene::ConfigEntry& e{ config.entries[i] };
                ImGui::PushID((int)i);
                std::string path{};
                for (const json& k : e.path) path += k.is_string() ? "." + k.get<std::string>() : "[" + k.dump() + "]";
                std::string header{ ToUtf8(fs::relative(e.file, app.project.dir)) + " " + path };
                bool dirty{ config.values[i] != config.saved[i] };
                if (dirty) header += " *";
                if (ImGui::CollapsingHeader((header + "###entry").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::PushTextWrapPos(0);
                    ImGui::TextDisabled("%s", e.what.c_str());
                    auto gen{ world.generated.find(e.file.lexically_normal()) };
                    if (gen != world.generated.end()) ImGui::TextColored(ImVec4(1, 0.7f, 0.35f, 1), "a tool rewrites this file: %s", gen->second.c_str());
                    ImGui::PopTextWrapPos();
                    if (!config.errors[i].empty()) {
                        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", config.errors[i].c_str());
                        ImGui::PopID();
                        continue;
                    }
                    json& v{ config.values[i] };
                    ImGui::BeginDisabled(busy);
                    if (ImGui::BeginTable("##config", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
                        ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthStretch, 0.42f);
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.58f);
                        if (v.is_object() || v.is_array()) JsonChildren(v);
                        else {
                            bool rm{};
                            JsonRow("value", v, rm);
                        }
                        // a new key (a string, typed over after)
                        if (v.is_object()) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            ImGui::InputTextWithHint("##newkey", "new key", &config.newKeys[i]);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::BeginDisabled(config.newKeys[i].empty() || v.contains(config.newKeys[i]));
                            if (ImGui::SmallButton("Add")) {
                                v[config.newKeys[i]] = "";
                                config.newKeys[i].clear();
                            }
                            ImGui::EndDisabled();
                        }
                        ImGui::EndTable();
                    }
                    ImGui::BeginDisabled(!dirty || moved > 0);
                    if (ImGui::Button("Save")) {
                        std::string error{};
                        if (!scene::SaveConfig(e, v, error)) {
                            Logf(LogLevel::ERR, "config: %s", error.c_str());
                        }
                        else {
                            config.saved[i] = v;
                            if (!e.run.empty()) {
                                std::string list{};
                                for (const std::string& s : e.run) list += (list.empty() ? "" : ", ") + s;
                                Logf(LogLevel::INFO, "running %s, then the map reloads", list.c_str());
                                app.scripts.Start(app.project.dir, e.run);
                            }
                            else {
                                app.project.Reload();
                            }
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Revert")) v = config.saved[i];
                    ImGui::EndDisabled();
                    ImGui::EndDisabled();
                    if (dirty && moved > 0) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("save or revert the moves first");
                    }
                    if (!e.run.empty()) {
                        std::string list{};
                        for (const std::string& s : e.run) list += (list.empty() ? "" : ", ") + s;
                        ImGui::PushTextWrapPos(0);
                        ImGui::TextDisabled("saving runs %s", list.c_str());
                        ImGui::PopTextWrapPos();
                    }
                }
                ImGui::PopID();
            }
        }

        // saved: what the files have is where the objects are now
        void MarkSaved() {
            for (Instance& inst : world.instances) {
                std::copy(inst.origin, inst.origin + 3, inst.savedOrigin);
                std::fill(inst.followed, inst.followed + 3, 0.0f);
                std::fill(inst.scaleDelta, inst.scaleDelta + 3, 0.0f);
                inst.savedScale = inst.scale;
                std::copy(inst.angles, inst.angles + 3, inst.savedAngles);
            }
        }

        // the moves saved, then the map's tools run (they reload it after) or the map reloaded
        void SaveMoves(App& app) {
            std::vector<std::string> scripts{};
            std::string error{};
            Logf(LogLevel::INFO, "saving %zu move(s):", world.MovedCount());
            if (!world.Save(app.project, scripts, error)) {
                Logf(LogLevel::ERR, "save failed: %s", error.c_str());
                return;
            }
            MarkSaved();
            if (!scripts.empty()) {
                std::string list{};
                for (const std::string& s : scripts) list += (list.empty() ? "" : ", ") + s;
                Logf(LogLevel::INFO, "saved; running %s, then the map reloads", list.c_str());
                app.scripts.Start(app.project.dir, scripts);
            }
            else {
                Log(LogLevel::INFO, "saved, the map reloads");
                app.project.Reload();
            }
        }

        // Ctrl+D (a copy of the selected object) / Delete (a stone out): written into its file, the unsaved moves saved
        // first (the map rebuilds), the copy selected once it has
        void CopyOrRemove(App& app, bool remove) {
            Instance* sel{ SelectedInstance() };
            if (!sel || app.scripts.running) return;
            std::vector<std::string> scripts{}, more{};
            std::string error{};
            bool saved{};
            if (size_t moved{ world.MovedCount() }) {
                Logf(LogLevel::INFO, "saving %zu move(s) first:", moved);
                if (!world.Save(app.project, scripts, error)) {
                    Logf(LogLevel::ERR, "save failed: %s", error.c_str());
                    return;
                }
                saved = true;
            }
            scene::Selection pick{};
            bool ok{ remove ? world.Remove(*sel, more, error) : world.Duplicate(*sel, more, pick, error) };
            if (saved) MarkSaved();
            if (!ok) Logf(LogLevel::WARN, "%s: %s", remove ? "delete" : "ctrl+D", error.c_str());
            if (!ok && !saved) return;
            for (const std::string& s : more) if (std::find(scripts.begin(), scripts.end(), s) == scripts.end()) scripts.push_back(s);
            if (ok) copyPick = pick;
            if (!scripts.empty()) app.scripts.Start(app.project.dir, scripts);
            else app.project.Reload();
        }

        void DrawSidePanel(App& app) {
            bool busy{ app.scripts.running };
            ImGui::SeparatorText("Selected");
            if (Instance* sel{ SelectedInstance() }) {
                Instance& inst{ *sel };
                ImGui::TextWrapped("%s", inst.label.c_str());
                ImGui::TextDisabled("%s", inst.board > 0 ? "a barrier's board (moves its window)" : KindName(inst.kind));
                // a board's saving is its barrier's
                const Instance* owner{ inst.board > 0 ? BarrierOf(inst) : &inst };
                const scene::Binding* b{ owner ? world.BindingOf(*owner) : nullptr };
                Allowed a{ AllowedFor(inst) };

                if (!a.Editable()) {
                    ImGui::PushTextWrapPos(0);
                    ImGui::TextColored(ImVec4(1, 0.7f, 0.35f, 1), "Locked: %s", !inst.locked.empty() ? inst.locked.c_str() : b ? b->locked.c_str() : "");
                    ImGui::PopTextWrapPos();
                    ImGui::Text("origin %.1f %.1f %.1f", inst.origin[0], inst.origin[1], inst.origin[2]);
                    ImGui::Text("angles %.1f %.1f %.1f", inst.angles[0], inst.angles[1], inst.angles[2]);
                }
                else {
                    ImGui::BeginDisabled(busy);
                    // the fields: x / y always, z and the angles as the binding writes them; a group's only its axes, by
                    // the gizmo's mode (moves, or the scale numbers / the model's scale)
                    float o[3]{ inst.origin[0], inst.origin[1], inst.origin[2] };
                    const scene::Target* g{ GroupOf(inst) };
                    if (gizmoMode == GizmoMode::SCALE) {
                        if (a.uniform) {
                            float sc{ inst.scale };
                            ImGui::SetNextItemWidth(-70);
                            bool ch{ ImGui::DragFloat("Scale", &sc, 0.01f, 0.05f, 20, "%.3f") };
                            if (ImGui::IsItemActivated()) PushUndo(selected);
                            if (ch) inst.scale = std::max(0.05f, sc);
                        }
                        else if (g && a.Scales()) {
                            bool on[3]{ a.sx, a.sy, a.sz };
                            const char* labels[3]{};
                            for (int k = 0; k < 2; k++) {
                                labels[k] = g->scaleXY ? "Size +" : k == g->axis ? "Width +" : k == g->AcrossAxis() ? "Depth +" : k == 0 ? "Size x +" : "Size y +";
                            }
                            labels[2] = "Height +";
                            for (int k = 0; k < 3; k++) {
                                if (!on[k]) continue;
                                float v{ inst.scaleDelta[k] };
                                ImGui::SetNextItemWidth(-70);
                                bool ch{ ImGui::DragFloat(labels[k], &v, 0.5f, 0, 0, "%.2f") };
                                if (ImGui::IsItemActivated()) PushUndo(selected);
                                if (ch) inst.scaleDelta[k] = v;
                            }
                            ImGui::PushTextWrapPos(0);
                            ImGui::TextDisabled("%s", g->scaleXY ? "the room's size: each wall out by this, and its height (they stretch as a preview)" :
                                "grows on both sides along / across, up from its base");
                            ImGui::PopTextWrapPos();
                        }
                        else {
                            ImGui::TextDisabled("nothing to scale here (W: move)");
                        }
                    }
                    else if (!a.move) {
                        ImGui::TextDisabled("it only scales (R: scale)");
                    }
                    else if (!g || !g->origin.empty()) {
                        ImGui::SetNextItemWidth(-70);
                        bool changed{ a.z ? ImGui::DragFloat3("Origin", o, 0.5f, 0, 0, "%.2f") : ImGui::DragFloat2("Origin", o, 0.5f, 0, 0, "%.2f") };
                        if (ImGui::IsItemActivated()) PushUndo(selected);
                        if (changed) std::copy(o, o + 3, inst.origin);
                        if (!a.z) ImGui::TextDisabled("z %.2f (kept: the tools put it on the floor)", inst.origin[2]);
                    }
                    else {
                        // a group's fields by role, each its own change (what it follows of its group's left out)
                        const char* labels[3]{};
                        for (int k = 0; k < 2; k++) labels[k] = k == g->axis ? "Along +" : k == g->AcrossAxis() ? "Across +" : "Move +";
                        labels[2] = "Up +";
                        bool on[3]{ a.x, a.y, a.z };
                        for (int k = 0; k < 3; k++) {
                            if (!on[k]) continue;
                            float v{ inst.origin[k] - inst.savedOrigin[k] - inst.followed[k] };
                            ImGui::SetNextItemWidth(-70);
                            bool ch{ ImGui::DragFloat(labels[k], &v, 0.5f, 0, 0, "%.2f") };
                            if (ImGui::IsItemActivated()) PushUndo(selected);
                            if (ch) inst.origin[k] = inst.savedOrigin[k] + inst.followed[k] + v;
                        }
                        ImGui::PushTextWrapPos(0);
                        ImGui::TextDisabled("%s", !g->follow.empty() ? "moves alone (its window's moves still take it along)" : "moves its whole group (fixed by its wall)");
                        ImGui::PopTextWrapPos();
                    }
                    if (a.full) {
                        float ang[3]{ inst.angles[0], inst.angles[1], inst.angles[2] };
                        ImGui::SetNextItemWidth(-70);
                        bool ch{ ImGui::DragFloat3("Angles", ang, 0.5f, 0, 0, "%.1f") };
                        if (ImGui::IsItemActivated()) PushUndo(selected);
                        if (ch) std::copy(ang, ang + 3, inst.angles);
                    }
                    else if (a.yaw) {
                        float yaw{ inst.angles[1] };
                        ImGui::SetNextItemWidth(-70);
                        bool ch{ ImGui::DragFloat("Yaw", &yaw, 0.5f, 0, 0, "%.1f") };
                        if (ImGui::IsItemActivated()) PushUndo(selected);
                        if (ch) inst.angles[1] = yaw;
                        if (g && !g->yawKey.empty()) {
                            ImGui::TextDisabled("turns alone (%+.1f into %s's %s)", Turn(inst.angles[1] - inst.savedAngles[1]), ToUtf8(g->file.filename()).c_str(), g->yawKey.c_str());
                        }
                    }
                    else {
                        ImGui::TextDisabled("its rotation isn't saved anywhere: it stays");
                    }
                    if (inst.Moved()) {
                        ImGui::TextColored(ImVec4(1, 0.8f, 0.35f, 1), "moved");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Revert it")) {
                            PushUndo(selected);
                            Revert(inst);
                        }
                    }
                    ImGui::EndDisabled();

                    // where it's saved
                    ImGui::PushTextWrapPos(0);
                    auto rel = [&](const fs::path& p) { return ToUtf8(fs::relative(p, app.project.dir)); };
                    if (!b || b->direct) {
                        ImGui::TextDisabled("saved into %s %s[%d]", rel(inst.source).c_str(), inst.kind == Kind::STATIC_MODEL ? "staticModels" : "entities", inst.sourceIndex);
                        auto gen{ world.generated.find(inst.source.lexically_normal()) };
                        if (!b && gen != world.generated.end()) {
                            ImGui::TextColored(ImVec4(1, 0.7f, 0.35f, 1), "%s: a tool can undo this move", gen->second.c_str());
                        }
                    }
                    if (b) {
                        for (const scene::Target& t : b->targets) {
                            std::string path{};
                            for (const json& k : t.path) path += k.is_string() ? "." + k.get<std::string>() : "[" + k.dump() + "]";
                            ImGui::TextDisabled("saved into %s %s", rel(t.file).c_str(), path.c_str());
                        }
                        for (const scene::GscPatch& g : b->gsc) ImGui::TextDisabled("and %s's %s() s.%s", rel(g.file).c_str(), g.function.c_str(), g.field.c_str());
                        if (!b->run.empty()) {
                            std::string list{};
                            for (const std::string& s : b->run) list += (list.empty() ? "" : ", ") + s;
                            ImGui::TextDisabled("then runs %s", list.c_str());
                        }
                        if (!b->note.empty()) ImGui::TextDisabled("%s", b->note.c_str());
                    }
                    ImGui::PopTextWrapPos();
                }

                if (inst.scale != 1) ImGui::Text("scale %.3f", inst.scale);
                if (inst.mesh >= 0) {
                    const scene::Mesh& m{ world.meshes[inst.mesh] };
                    ImGui::TextDisabled("%zu triangles, %zu parts", m.indices.size() / 3, m.parts.size());
                }
                else {
                    ImGui::TextDisabled("drawn as a box (no mesh the editor reads)");
                }
                ImGui::BeginDisabled(busy);
                if (ImGui::SmallButton("Copy (ctrl+D)")) CopyOrRemove(app, false);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("a stone's or the sill's copy goes into room.json's stones, on top of it; a free model or entity's into its own file, beside it. Unsaved moves are saved first");
                if (b && b->removable) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete")) CopyOrRemove(app, true);
                }
                ImGui::EndDisabled();
                if (ImGui::SmallButton("Show its file")) {
                    SelectFile(inst.source);
                    Logf(LogLevel::INFO, "%s selected: see the Zone or Folder tab's preview", ToUtf8(inst.source.filename()).c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ToUtf8(inst.source).c_str());
                if (!inst.model.empty()) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Its model")) SelectFile(inst.model);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ToUtf8(inst.model).c_str());
                }
            }
            else {
                ImGui::TextDisabled("Click something in the view.");
            }

            DrawConfig(app);

            ImGui::SeparatorText("Moving");
            // the gizmo's mode (W / R over the view)
            GizmoMode mode{ gizmoMode };
            if (ImGui::RadioButton("Move (W)", gizmoMode == GizmoMode::MOVE)) gizmoMode = GizmoMode::MOVE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale (R)", gizmoMode == GizmoMode::SCALE)) gizmoMode = GizmoMode::SCALE;
            if (gizmoMode != mode) gizmo.drag = Handle::NONE;
            ImGui::Checkbox("Snap", &snapMove);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            const float sizes[]{ 1, 2, 4, 8, 16, 32, 64 };
            char sizeLabel[16];
            snprintf(sizeLabel, sizeof(sizeLabel), "%g", snapSize);
            if (ImGui::BeginCombo("##snapsize", sizeLabel)) {
                for (float s : sizes) {
                    char l[16];
                    snprintf(l, sizeof(l), "%g", s);
                    if (ImGui::Selectable(l, s == snapSize)) snapSize = s;
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Turn 15", &snapRotate);
            size_t moved{ world.MovedCount() };
            ImGui::BeginDisabled(!moved || busy);
            char saveLabel[48];
            snprintf(saveLabel, sizeof(saveLabel), "Save %zu move%s", moved, moved == 1 ? "" : "s");
            if (ImGui::Button(saveLabel)) SaveMoves(app);
            ImGui::SameLine();
            if (ImGui::Button("Revert all")) {
                for (Instance& inst : world.instances) Revert(inst);
                undo.clear();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(undo.empty() || busy);
            if (ImGui::Button("Undo")) {
                UndoStep step{ undo.back() };
                undo.pop_back();
                if (step.index < (int)world.instances.size()) ApplyUndo(step, world.instances[step.index]);
            }
            ImGui::EndDisabled();
            if (busy) ImGui::TextColored(ImVec4(1, 0.8f, 0.35f, 1), "running the map's tools...");
            if (world.bindingsFile.empty()) {
                ImGui::TextDisabled("No editor_bindings.json: moves go into gfx_map / entitylist.");
            }

            if (ImGui::CollapsingHeader("Show", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Static models", &showStatic);
                ImGui::Checkbox("Script models", &showScript);
                ImGui::Checkbox("Entities", &showEntities);
                ImGui::SameLine();
                ImGui::Checkbox("Icons", &showIcons);
                ImGui::Checkbox("Grid", &showGrid);
            }

            if (ImGui::CollapsingHeader("Lighting")) {
                ImGui::Checkbox("Game lighting", &look.game);
                ImGui::SameLine();
                HelpMarker("the map's lighting (the Lighting & sky tab, live): its sun's direction and color, sun shadows, the probe "
                    "color as the ambient tinted by the sky, a haze toward the sky, the sky image. Off: a plain light");
                ImGui::BeginDisabled(!look.game);
                ImGui::Checkbox("Shadows", &look.shadows);
                ImGui::SameLine();
                ImGui::Checkbox("Sky", &look.sky);
                ImGui::SameLine();
                ImGui::Checkbox("Haze", &look.haze);
                float amb{ look.ambient >= 0 ? look.ambient : look.mapAmbient };
                ImGui::SetNextItemWidth(-90);
                if (ImGui::SliderFloat("Ambient", &amb, 0, 1.5f, "%.2f")) look.ambient = amb;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("the map's: %.2f (its light grid against its sun, kept readable); right click: back to it", look.mapAmbient);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) look.ambient = -1;
                ImGui::SetNextItemWidth(-90);
                ImGui::SliderFloat("Sun", &look.sun, 0, 2, "%.2f");
                ImGui::SetNextItemWidth(-90);
                ImGui::SliderFloat("Sky light", &look.skyBright, 0.1f, 4, "%.2f");
                ImGui::TextDisabled("sun pitch %.1f yaw %.1f, %.0f lux", look.sunPitch, look.sunYaw, look.sunIlluminance);
                ImGui::TextDisabled("sky %s", r.skySrv ? ToUtf8(r.skyPath.filename()).c_str() : "none (a gradient)");
                ImGui::EndDisabled();
            }

            if (ImGui::CollapsingHeader("Camera")) {
                ImGui::SetNextItemWidth(-90);
                ImGui::DragFloat("Speed", &cam.speed, 5, 25, 20000, "%.0f u/s");
                ImGui::SetNextItemWidth(-90);
                ImGui::SliderFloat("FOV", &cam.fov, 30, 110, "%.0f");
                if (ImGui::Button("Frame all")) FrameAll();
                ImGui::SameLine();
                ImGui::BeginDisabled(selected < 0);
                if (ImGui::Button("Frame selected")) Frame(world.instances[selected]);
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Rebuild")) RebuildScene(app);
            }

            ImGui::SeparatorText("Objects");
            static ImGuiTextFilter filter{};
            filter.Draw("##objfilter", -1);
            ImGui::BeginChild("##objects", ImVec2(0, std::max(ImGui::GetContentRegionAvail().y, 220.0f)), ImGuiChildFlags_Border);
            for (size_t i = 0; i < world.instances.size(); i++) {
                const Instance& inst{ world.instances[i] };
                if (!Visible(inst) || !filter.PassFilter(inst.label.c_str())) continue;
                ImGui::PushID((int)i);
                ImGui::ColorButton("##c", ImVec4(inst.color[0], inst.color[1], inst.color[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(10, 10));
                ImGui::SameLine();
                if (ImGui::Selectable(inst.label.c_str(), (int)i == selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    selected = OwnerIndex((int)i);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) Frame(inst);
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }

    bool DumpImage(const fs::path& image, const fs::path& out, bool hdr) {
        // a dumped image decoded by the GPU: its colors over its alpha (gray), its mip 0
        const char* src{ R"(
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);
float4 VS(uint id : SV_VertexID) : SV_POSITION { float2 p = float2((id << 1) & 2, id & 2); return float4(p * float2(2, -2) + float2(-1, 1), 0, 1); }
float4 PS(float4 pos : SV_POSITION) : SV_TARGET {
    uint w, h; tex0.GetDimensions(w, h);
    float2 uv = float2(pos.x / w, (pos.y % h) / h);
    float4 t = tex0.SampleLevel(samp0, uv, 0);
    return pos.y < h ? float4(t.rgb, 1) : float4(t.aaa, 1);
}
// an HDR image (the sky): its colors only, tone mapped
float4 PSHdr(float4 pos : SV_POSITION) : SV_TARGET {
    uint w, h; tex0.GetDimensions(w, h);
    float3 c = tex0.SampleLevel(samp0, float2(pos.x * 4 / w, pos.y * 4 / h), 0).rgb * 0.3;
    return float4(c / (1 + c), 1);
}
)" };
        if (!r.tried) Init();
        ID3D11ShaderResourceView* srv{ LoadDumpedImage(image) };
        if (!srv) return false;
        ID3D11Resource* res{};
        srv->GetResource(&res);
        D3D11_TEXTURE2D_DESC sd{};
        ((ID3D11Texture2D*)res)->GetDesc(&sd);
        res->Release();
        ID3DBlob* vsb{};
        ID3DBlob* psb{};
        D3DCompile(src, strlen(src), "dump", nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsb, nullptr);
        D3DCompile(src, strlen(src), "dump", nullptr, nullptr, hdr ? "PSHdr" : "PS", "ps_4_0", 0, 0, &psb, nullptr);
        if (!vsb || !psb) return false;
        ID3D11VertexShader* vs{};
        ID3D11PixelShader* ps{};
        device->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs);
        device->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps);
        // an HDR image at a quarter of its size, its colors only
        UINT w{ hdr ? sd.Width / 4 : sd.Width }, h{ hdr ? sd.Height / 4 : sd.Height * 2 };
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        ID3D11Texture2D* rt{};
        ID3D11RenderTargetView* rtv{};
        device->CreateTexture2D(&td, nullptr, &rt);
        device->CreateRenderTargetView(rt, nullptr, &rtv);
        context->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp{ 0, 0, (float)w, (float)h, 0, 1 };
        context->RSSetViewports(1, &vp);
        context->RSSetState(r.raster);
        context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        context->OMSetDepthStencilState(r.noDepth, 0);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs, nullptr, 0);
        context->PSSetShader(ps, nullptr, 0);
        context->PSSetShaderResources(0, 1, &srv);
        context->PSSetSamplers(0, 1, &r.sampler);
        context->Draw(3, 0);
        td.Usage = D3D11_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* staging{};
        device->CreateTexture2D(&td, nullptr, &staging);
        context->CopyResource(staging, rt);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        bool ok{};
        if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            std::vector<uint8_t> rgba((size_t)w * h * 4);
            for (UINT y = 0; y < h; y++) memcpy(rgba.data() + (size_t)y * w * 4, (const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch, (size_t)w * 4);
            context->Unmap(staging, 0);
            // the alpha's spread, in tenths
            int hist[11]{};
            for (size_t i = hdr ? 0 : (size_t)w * sd.Height * 4; i < rgba.size(); i += 4) hist[rgba[i] * 10 / 255]++;
            std::string spread{};
            for (int k = 0; k <= 10; k++) spread += std::to_string(hist[k]) + " ";
            Logf(LogLevel::INFO, "%s: %ux%u, alpha by tenths: %s", ToUtf8(image.filename()).c_str(), sd.Width, sd.Height, spread.c_str());
            ok = WritePng(out, w, h, rgba.data());
        }
        Release(staging);
        Release(rtv);
        Release(rt);
        Release(vs);
        Release(ps);
        Release(vsb);
        Release(psb);
        srv->Release();
        return ok;
    }

    bool DumpSky(const fs::path& out) {
        if (r.skyShown.size() != (size_t)SKY_W * SKY_H * 4) return false;
        return WritePng(out, SKY_W, SKY_H, r.skyShown.data());
    }

    void SetViewCamera(float x, float y, float z, float yaw, float pitch) {
        cam.pos = XMFLOAT3(x, y, z);
        cam.yaw = yaw;
        cam.pitch = pitch;
        // kept over the first build's look
        builtGeneration = builtGeneration == (size_t)-1 ? (size_t)-2 : builtGeneration;
    }

    void SelectObject(const std::string& labelPart) {
        for (size_t i = 0; i < world.instances.size(); i++) {
            if (world.instances[i].label.find(labelPart) != std::string::npos) {
                selected = OwnerIndex((int)i);
                return;
            }
        }
    }

    bool GroupTest(App& app, const fs::path& outDir) {
        if (!r.tried) Init();
        if (!r.ready) return false;
        RebuildScene(app);
        auto pick = [&](const char* label) {
            for (size_t i = 0; i < world.instances.size(); i++) {
                if (world.instances[i].label.find(label) != std::string::npos) return (int)i;
            }
            Logf(LogLevel::ERR, "grouptest: no %s", label);
            return -1;
        };
        auto logGroup = [&](const char* what) {
            for (const Instance& m : world.instances) {
                if (!m.Moved()) continue;
                Logf(LogLevel::INFO, "grouptest %s: %s moved %+.1f %+.1f %+.1f, scale +%.1f %.1f %.1f x%.2f", what, m.label.c_str(),
                    m.origin[0] - m.savedOrigin[0], m.origin[1] - m.savedOrigin[1], m.origin[2] - m.savedOrigin[2],
                    m.scaleDelta[0], m.scaleDelta[1], m.scaleDelta[2], m.scale);
            }
        };
        auto logAllowed = [&](const char* what) {
            Allowed a{ AllowedFor(world.instances[selected]) };
            Logf(LogLevel::INFO, "grouptest: %s move %d (x %d y %d z %d yaw %d) scale x %d y %d z %d uniform %d", what, a.move, a.x, a.y, a.z,
                a.yaw, a.sx, a.sy, a.sz, a.uniform);
        };
        // the window's sill, moved alone (its gizmo on its bottom middle): 16 along the wall (south, x), 4 across,
        // lifted 5; then scaled: 6 out each side along (12 longer), 2 across (4 deeper), 8 taller; turned 20 (its scale
        // handles turn with it)
        selected = pick("enh_sill_");
        if (selected < 0) return false;
        logAllowed("sill");
        XMVECTOR at{ GizmoAt(world.instances[selected]) };
        Logf(LogLevel::INFO, "grouptest: sill gizmo at %.1f %.1f %.1f", XMVectorGetX(at), XMVectorGetY(at), XMVectorGetZ(at));
        Instance& sill{ world.instances[selected] };
        sill.origin[0] += 16;
        sill.origin[1] += 4;
        sill.origin[2] += 5;
        sill.scaleDelta[0] = 6;
        sill.scaleDelta[1] = 2;
        sill.scaleDelta[2] = 8;
        sill.angles[1] += 20;
        gizmoMode = GizmoMode::SCALE;
        XMVECTOR hx{ HandleAxis(sill, Handle::X) };
        gizmoMode = GizmoMode::MOVE;
        Logf(LogLevel::INFO, "grouptest: sill turned 20, its x scale handle %.3f %.3f", XMVectorGetX(hx), XMVectorGetY(hx));
        SyncGroup();
        logGroup("sill");
        // then the barrier 8 along and 6 up: the window's spots and risers 8 along, the sill too (followed, its own kept);
        // turned 30 alone, its boards with it (board 3 as far from the barrier, turned 30 about it)
        selected = pick("zbarrier_zmcore_t8_basicwoodbarrier");
        if (selected < 0) return false;
        logAllowed("barrier");
        Instance& barrier{ world.instances[selected] };
        int board{ pick("room_window_a_barrier board 3") };
        if (board < 0) return false;
        auto boardAt = [&](const char* when) {
            const Instance& b{ world.instances[board] };
            float dx{ b.origin[0] - barrier.origin[0] }, dy{ b.origin[1] - barrier.origin[1] };
            Logf(LogLevel::INFO, "grouptest: board 3 %s: %.2f %.2f %.2f, %.2f from the barrier at %.1f deg", when, b.origin[0], b.origin[1], b.origin[2],
                std::sqrt(dx * dx + dy * dy), XMConvertToDegrees(std::atan2(dy, dx)));
        };
        boardAt("before");
        barrier.origin[0] += 8;
        barrier.origin[2] += 6;
        barrier.angles[1] += 30;
        SyncGroup();
        boardAt("after (moved 8, up 6, turned 30)");
        Logf(LogLevel::INFO, "grouptest: picking board 3 selects %s", world.instances[OwnerIndex(board)].label.c_str());
        logGroup("window");
        SetViewCamera(0, -170, 64, 270, 6);
        RenderToFile(app, outDir / "acts-map-editor-group-window.png", 960, 540, -1);
        // the walls: scale only, the room 32 out each wall, 8 higher, stretched as the preview
        selected = pick("enh_room_walls");
        if (selected < 0) return false;
        logAllowed("walls");
        world.instances[selected].scaleDelta[0] = 32;
        world.instances[selected].scaleDelta[2] = 8;
        // the floor (a static model without a binding): its uniform scale
        int floor{ pick("enh_floor") };
        if (floor >= 0) {
            selected = floor;
            logAllowed("floor");
            world.instances[floor].scale = 1.5f;
        }
        selected = pick("enh_room_walls");
        gizmoMode = GizmoMode::SCALE;
        SetViewCamera(-600, -600, 450, 45, 28);
        RenderToFile(app, outDir / "acts-map-editor-group-walls.png", 960, 540, -1);
        gizmoMode = GizmoMode::MOVE;
        // a stone (room.json stones), when there's one: 8 along x, turned 45, 4 wider each side
        for (size_t i = 0; i < world.instances.size(); i++) {
            Instance& s{ world.instances[i] };
            if (s.label.rfind("enh_stone_", 0) != 0) continue;
            selected = (int)i;
            logAllowed(s.label.c_str());
            s.origin[0] += 8;
            s.angles[1] += 45;
            s.scaleDelta[0] = 4;
            SyncGroup();
            break;
        }
        Logf(LogLevel::INFO, "grouptest: %zu change(s) to save", world.MovedCount());
        // written nowhere: the save's changes only logged
        std::vector<std::string> scripts{};
        std::string error{};
        bool ok{ world.Save(app.project, scripts, error, false) };
        std::string list{};
        for (const std::string& s : scripts) list += " " + s;
        Logf(ok ? LogLevel::INFO : LogLevel::ERR, "grouptest: %s (dry run), scripts:%s", ok ? "saved" : error.c_str(), list.c_str());
        return ok;
    }

    bool CopyTest(App& app) {
        if (!r.tried) Init();
        if (!r.ready) return false;
        RebuildScene(app);
        Logf(LogLevel::INFO, "copytest: %zu binding(s)", world.bindings.size());
        for (size_t i = 0; i < world.instances.size(); i++) {
            const Instance& inst{ world.instances[i] };
            if (inst.label.rfind("enh_sill_", 0) != 0 && inst.label.rfind("enh_stone_", 0) != 0) continue;
            const scene::Binding* b{ world.BindingOf(inst) };
            Logf(LogLevel::INFO, "copytest: %s at %.1f %.1f %.1f yaw %.1f, bound %d%s", inst.label.c_str(), inst.origin[0], inst.origin[1], inst.origin[2],
                inst.angles[1], b != nullptr, b && b->removable ? " (removable)" : "");
        }
        selected = -1;
        for (size_t i = 0; i < world.instances.size() && selected < 0; i++) {
            if (world.instances[i].label.rfind("enh_sill_", 0) == 0) selected = (int)i;
        }
        if (selected < 0) {
            Log(LogLevel::ERR, "copytest: no sill");
            return false;
        }
        std::vector<std::string> scripts{};
        scene::Selection pick{};
        std::string error{};
        bool ok{ world.Duplicate(world.instances[selected], scripts, pick, error) };
        std::string list{};
        for (const std::string& s : scripts) list += " " + s;
        Logf(ok ? LogLevel::INFO : LogLevel::ERR, "copytest: %s, select %s, scripts:%s", ok ? "copied" : error.c_str(), pick.label.c_str(), list.c_str());
        // delete: the first stone out (the copy stays), the sill refused
        for (const Instance& inst : world.instances) {
            if (inst.label != "enh_stone_0" && inst.label.rfind("enh_sill_", 0) != 0) continue;
            bool removed{ world.Remove(inst, scripts, error) };
            Logf(LogLevel::INFO, "copytest: delete %s: %s", inst.label.c_str(), removed ? "removed" : error.c_str());
        }
        return ok;
    }

    bool SaveTest(App& app) {
        world.Build(app.project);
        // one move of each kind: a room.json perk (and its yaw), a placements.json part, the ferris wheel (its own file,
        // placements.json and the gsc's layout), an initial spawn (its own file)
        auto move = [&](const char* label, float dx, float dy, float dz, float dyaw) {
            for (Instance& inst : world.instances) {
                if (inst.label.find(label) == std::string::npos) continue;
                inst.origin[0] += dx;
                inst.origin[1] += dy;
                inst.origin[2] += dz;
                inst.angles[1] = Wrap360(inst.angles[1] + dyaw);
                Logf(LogLevel::INFO, "savetest: moved %s", inst.label.c_str());
                return;
            }
            Logf(LogLevel::ERR, "savetest: no %s", label);
        };
        move("enh_perk_statue_0", 16, 0, 0, 90);
        move("zitem_rocketshield_part_1", 0, 0, 8, 0);
        move("enh_ferriswheel", 0, 32, 0, 0);
        move("initial_spawn_points", 8, 0, 0, 0);
        std::vector<std::string> scripts{};
        std::string error{};
        bool ok{ world.Save(app.project, scripts, error) };
        std::string list{};
        for (const std::string& s : scripts) list += " " + s;
        Logf(ok ? LogLevel::INFO : LogLevel::ERR, "savetest: %s, scripts:%s", ok ? "saved" : error.c_str(), list.c_str());
        return ok;
    }

    bool ViewportLoading() {
        return loading.step != LoadStep::NONE;
    }

    void SetViewportContext(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        device = dev;
        context = ctx;
    }

    bool RenderToFile(App& app, const fs::path& out, int w, int h, int view) {
        if (!r.tried) Init();
        if (!r.ready) return false;
        if (builtZone != app.project.zone.path || builtGeneration != app.project.generation) RebuildScene(app);
        const float target[3]{ 0, 0, 0 };
        // -1 the camera as it is (SetViewCamera), 0 the origin from a corner, 1 everything, 2 straight down on the origin
        // (+x right, +y up), 3 + i close on the i-th object with a mesh drawn from the dumped stock models
        if (view >= 0) {
            cam.yaw = 45;
            cam.pitch = 28;
        }
        if (view < 0) {
        }
        else if (view >= 3) {
            int n{ view - 3 };
            for (const Instance& inst : world.instances) {
                if (inst.mesh < 0 || inst.model.empty() || inst.kind == Kind::STATIC_MODEL) continue;
                if (n-- == 0) {
                    Frame(inst);
                    break;
                }
            }
            if (n >= 0) return false;
        }
        else if (view == 1) FrameAll();
        else if (view == 2) {
            cam.yaw = 90;
            cam.pitch = 89.9f;
            LookAt(target, 1100);
        }
        else LookAt(target, 900);
        UpdateLook(app);
        Render(w, h);

        // the resolved image read back
        D3D11_TEXTURE2D_DESC td{};
        r.resolved->GetDesc(&td);
        td.Usage = D3D11_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* staging{};
        if (FAILED(device->CreateTexture2D(&td, nullptr, &staging))) return false;
        context->CopyResource(staging, r.resolved);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        bool ok{};
        if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            std::vector<uint8_t> rgba((size_t)w * h * 4);
            for (int y = 0; y < h; y++) {
                memcpy(rgba.data() + (size_t)y * w * 4, (const uint8_t*)mapped.pData + (size_t)y * mapped.RowPitch, (size_t)w * 4);
            }
            context->Unmap(staging, 0);
            for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
            ok = WritePng(out, w, h, rgba.data());
        }
        staging->Release();
        Logf(ok ? LogLevel::INFO : LogLevel::ERR, "rendered %s", ToUtf8(out).c_str());
        return ok;
    }

    void ShutdownViewport() {
        for (GpuMesh& g : r.meshes) Free(g);
        r.meshes.clear();
        for (GpuMesh& g : pendingMeshes) Free(g);
        pendingMeshes.clear();
        Free(r.cube);
        for (auto& [p, srv] : r.textures) {
            if (srv) srv->Release();
        }
        r.textures.clear();
        FreeTargets();
        Release(r.meshVS);
        Release(r.meshPS);
        Release(r.meshLayout);
        Release(r.lineVS);
        Release(r.linePS);
        Release(r.lineLayout);
        Release(r.frameCB);
        Release(r.objectCB);
        Release(r.lineVB);
        Release(r.sampler);
        Release(r.raster);
        Release(r.depth);
        Release(r.noDepth);
        Release(r.depthRead);
        Release(r.blend);
        Release(r.rasterCull);
        Release(r.depthVS);
        Release(r.skyVS);
        Release(r.skyPS);
        Release(r.statsPS);
        Release(r.rasterShadow);
        Release(r.shadowSampler);
        Release(r.skySampler);
        Release(r.shadowSrv);
        Release(r.shadowDsv);
        Release(r.shadowTex);
        Release(r.skySrv);
        Release(r.skyFix);
        r.skyShown.clear();
        r.skyPath.clear();
        r.ready = false;
    }

    void DrawViewportPanel(App& app) {
        if (r.tried && !r.ready) {
            ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "The 3D view can't start (see the log).");
            return;
        }
        // (re)loading a step a frame: the first load (or another zone) behind the loading screen, naming the step the
        // next frame runs; a reload of the same zone built aside while the old scene stays drawn
        bool stale{ !r.tried || builtZone != app.project.zone.path || builtGeneration != app.project.generation };
        if (stale && (loading.step == LoadStep::NONE || loading.step == LoadStep::UPLOAD || loading.step == LoadStep::SKY)) {
            bool reload{ r.ready && builtZone == app.project.zone.path && !world.instances.empty() };
            StartLoading(app, reload || (loading.step != LoadStep::NONE && loading.background));
        }
        if (loading.step != LoadStep::NONE) {
            if (loading.background) {
                LoadingStep(app, 0.025f);
            }
            else {
                if (loading.shown) LoadingStep(app, 0.025f);
                loading.shown = true;
                if (loading.step != LoadStep::NONE) {
                    DrawLoading();
                    return;
                }
                if (!r.ready) return;
            }
        }

        // the view and the side panel, its width kept as the window resizes
        static float sideWidth{ 360 };
        SplitLeft("##viewsplit", sideWidth, 240, 260, true);

        ImVec2 avail{ ImGui::GetContentRegionAvail() };
        int w{ std::max(16, (int)avail.x) };
        int h{ std::max(16, (int)avail.y - 4) };
        ImGuiIO& io{ ImGui::GetIO() };

        ImVec2 origin{ ImGui::GetCursorScreenPos() };
        ImGui::InvisibleButton("##view", ImVec2((float)w, (float)h),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        bool hovered{ ImGui::IsItemHovered() };
        bool active{ ImGui::IsItemActive() };
        float aspect{ (float)w / h };

        // looking and panning read the mouse grabbed (no stop at the screen's edge)
        bool grabbing{ active && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) };
        ImVec2 motion{ GrabMotion(grabbing, ImVec2(origin.x + w * 0.5f, origin.y + h * 0.5f)) };

        // fly: right mouse looks, WASD / QE (or ctrl / space) move, shift faster, the wheel sets the speed
        if (active && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            cam.yaw -= motion.x * 0.15f;
            cam.pitch = std::clamp(cam.pitch + motion.y * 0.15f, -89.0f, 89.0f);
            float step{ cam.speed * io.DeltaTime * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 4.0f : 1.0f) };
            XMVECTOR pos{ XMLoadFloat3(&cam.pos) };
            if (ImGui::IsKeyDown(ImGuiKey_W)) pos += Forward() * step;
            if (ImGui::IsKeyDown(ImGuiKey_S)) pos -= Forward() * step;
            if (ImGui::IsKeyDown(ImGuiKey_D)) pos += Right() * step;
            if (ImGui::IsKeyDown(ImGuiKey_A)) pos -= Right() * step;
            if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) pos += XMVectorSet(0, 0, step, 0);
            if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) pos -= XMVectorSet(0, 0, step, 0);
            XMStoreFloat3(&cam.pos, pos);
            if (io.MouseWheel != 0) cam.speed = std::clamp(cam.speed * (io.MouseWheel > 0 ? 1.25f : 0.8f), 25.0f, 20000.0f);
        }
        else if (active && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            // pan
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            float scale{ cam.speed / 400.0f };
            XMVECTOR pos{ XMLoadFloat3(&cam.pos) };
            XMVECTOR up{ XMVector3Cross(Right(), Forward()) };
            pos += Right() * (-motion.x * scale) + up * (motion.y * scale);
            XMStoreFloat3(&cam.pos, pos);
        }
        else if (hovered && io.MouseWheel != 0) {
            XMVECTOR pos{ XMLoadFloat3(&cam.pos) };
            XMStoreFloat3(&cam.pos, pos + Forward() * (io.MouseWheel * cam.speed * 0.25f));
        }
        // the gizmo: its handle under the mouse, a drag along an arrow or round the ring; a click elsewhere selects
        XMMATRIX vp{ ViewProj(aspect) };
        float nx{ (io.MousePos.x - origin.x) / w * 2 - 1 };
        float ny{ 1 - (io.MousePos.y - origin.y) / h * 2 };
        XMVECTOR ro{}, rd{};
        Ray(nx, ny, aspect, ro, rd);
        Instance* sel{ SelectedInstance() };
        bool canEdit{ !app.scripts.running };
        float iconRadius{ ImGui::GetFontSize() * 0.7f };
        bool looking{ ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle) };
        LayoutIcons(vp, origin, (float)w, (float)h, io.MousePos, hovered && !looking && gizmo.drag == Handle::NONE, iconRadius);
        if (gizmo.drag == Handle::NONE) {
            gizmo.hover = sel && canEdit && hovered && !looking ? HitGizmo(*sel, vp, origin, (float)w, (float)h, io.MousePos) : Handle::NONE;
        }
        // a click: the gizmo's handle, else an icon, else what's under the mouse
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            if (gizmo.hover != Handle::NONE && sel) {
                gizmo.drag = gizmo.hover;
                std::copy(sel->origin, sel->origin + 3, gizmo.startOrigin);
                XMVECTOR at{ GizmoAt(*sel) };
                const float a3[3]{ XMVectorGetX(at), XMVectorGetY(at), XMVectorGetZ(at) };
                for (int k = 0; k < 3; k++) gizmo.anchor[k] = a3[k] - sel->origin[k];
                std::copy(sel->angles, sel->angles + 3, gizmo.startAngles);
                std::copy(sel->scaleDelta, sel->scaleDelta + 3, gizmo.startScaleDelta);
                gizmo.startScale = sel->scale;
                bool ok{ gizmo.drag == Handle::RING ? RingAngle(ro, rd, gizmo.startParam) : AxisParam(ro, rd, HandleAxis(*sel, gizmo.drag), gizmo.startParam) };
                if (ok) PushUndo(selected);
                else gizmo.drag = Handle::NONE;
            }
            else if (hoveredIcon >= 0) {
                selected = OwnerIndex(hoveredIcon);
            }
            else {
                selected = OwnerIndex(Pick(ro, rd));
            }
        }
        if (gizmo.drag != Handle::NONE) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !sel) {
                gizmo.drag = Handle::NONE;
            }
            else if (gizmo.drag == Handle::RING) {
                float angle{};
                if (RingAngle(ro, rd, angle)) {
                    float yaw{ gizmo.startAngles[1] + angle - gizmo.startParam };
                    if (snapRotate) yaw = std::round(yaw / snapAngle) * snapAngle;
                    sel->angles[1] = Wrap360(yaw);
                }
            }
            else {
                float param{};
                XMVECTOR axis{ HandleAxis(*sel, gizmo.drag) };
                if (AxisParam(ro, rd, axis, param)) {
                    int k{ gizmo.drag == Handle::X ? 0 : gizmo.drag == Handle::Y ? 1 : 2 };
                    float delta{ param - gizmo.startParam };
                    if (gizmoMode == GizmoMode::SCALE && AllowedFor(*sel).uniform) {
                        // the whole model by the drag against its half size on that axis (twice its size a half out)
                        float half{ std::max(1.0f, (sel->maxs[k] - sel->mins[k]) * 0.5f * gizmo.startScale) };
                        float sc{ std::max(0.05f, gizmo.startScale * (1 + delta / half)) };
                        if (snapMove) sc = std::max(0.05f, std::round(sc * 20) / 20);
                        sel->scale = sc;
                    }
                    else if (gizmoMode == GizmoMode::SCALE) {
                        // the binding's scale number of that axis
                        float v{ gizmo.startScaleDelta[k] + delta };
                        if (snapMove) v = std::round(v / snapSize) * snapSize;
                        sel->scaleDelta[k] = v;
                    }
                    else {
                        float v{ gizmo.startOrigin[k] + delta };
                        if (snapMove) v = std::round(v / snapSize) * snapSize;
                        sel->origin[k] = v;
                    }
                }
            }
        }
        // the gizmo's mode: W move, R scale (not while flying: W moves the camera then)
        if (hovered && !looking && gizmo.drag == Handle::NONE && !io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoMode = GizmoMode::MOVE;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoMode = GizmoMode::SCALE;
        }
        if (hovered && selected >= 0 && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            Frame(world.instances[selected]);
        }
        if (canEdit && !undo.empty() && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z) && !ImGui::GetIO().WantTextInput) {
            UndoStep step{ undo.back() };
            undo.pop_back();
            if (step.index < (int)world.instances.size()) {
                ApplyUndo(step, world.instances[step.index]);
                selected = step.index;
            }
        }
        // ctrl+D: a copy; delete (over the view): a stone out (not while flying: ctrl goes down, D right)
        if (canEdit && sel && !looking && gizmo.drag == Handle::NONE && !io.WantTextInput) {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)) CopyOrRemove(app, false);
            else if (hovered && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) CopyOrRemove(app, true);
        }

        SyncGroup();
        UpdateLook(app);
        Render(w, h);
        ImDrawList* dl{ ImGui::GetWindowDrawList() };
        dl->AddImage((ImTextureID)r.srv, origin, ImVec2(origin.x + w, origin.y + h));
        dl->PushClipRect(origin, ImVec2(origin.x + w, origin.y + h), true);
        DrawIcons(dl, iconRadius);
        dl->PopClipRect();
        if (hoveredIcon >= 0 && gizmo.hover == Handle::NONE) {
            const Instance& inst{ world.instances[hoveredIcon] };
            ImGui::SetTooltip("%s\n%s", IconOf(inst).what, inst.label.c_str());
        }
        char info[256];
        snprintf(info, sizeof(info), "camera %.0f %.0f %.0f  yaw %.0f pitch %.0f  speed %.0f   %s", cam.pos.x, cam.pos.y, cam.pos.z, cam.yaw, cam.pitch,
            cam.speed, gizmoMode == GizmoMode::SCALE ? "[scale: R]" : "[move: W]");
        dl->AddText(ImVec2(origin.x + 8, origin.y + 6), IM_COL32(230, 230, 230, 220), info);
        dl->AddText(ImVec2(origin.x + 8, origin.y + 6 + ImGui::GetTextLineHeight()), IM_COL32(170, 175, 185, 200),
            "right mouse: look + WASD / QE move (shift faster, wheel speed)   middle: pan   wheel: dolly   left: select / drag the arrows and ring   W / R: move / scale   F: frame   ctrl+Z: undo   ctrl+D: copy   delete: a stone out");
        // the map's tools running, then the reload built aside: the old scene meanwhile
        if (app.scripts.running || (loading.background && loading.step != LoadStep::NONE)) {
            const char* busyText{ app.scripts.running ? "Running the map's tools..." : "Updating the view..." };
            dl->AddText(ImVec2(origin.x + 8, origin.y + 6 + ImGui::GetTextLineHeight() * 2), ImGui::GetColorU32(ImGuiCol_CheckMark), busyText);
        }

        SplitRight();
        ImGui::BeginChild("##side");
        DrawSidePanel(app);
        ImGui::EndChild();
        SplitEnd();
    }
}
