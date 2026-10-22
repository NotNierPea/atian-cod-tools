#include "scene.hpp"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

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

        const char* SHADERS{ R"(
#pragma pack_matrix(row_major)
cbuffer Frame : register(b0) { float4x4 viewProj; float4 lightDir; float4 camPos; };
cbuffer Object : register(b1) { float4x4 world; float4 color; float4 params; };
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

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
    float3 base = color.rgb;
    if (params.x > 0.5) base *= tex0.Sample(samp0, i.uv).rgb;
    float ndl = saturate(dot(n, -lightDir.xyz));
    float hemi = 0.5 + 0.5 * n.z;
    float3 lit = base * (0.22 + 0.33 * hemi + 0.62 * ndl);
    if (params.y > 0.5) lit = lerp(lit, float3(1.0, 0.62, 0.18), 0.38);
    return float4(lit, 1);
}

struct LineIn { float3 pos : POSITION; float4 col : COLOR; };
struct LineOut { float4 pos : SV_POSITION; float4 col : COLOR; };
LineOut LineVS(LineIn i) { LineOut o; o.pos = mul(float4(i.pos, 1), viewProj); o.col = i.col; return o; }
float4 LinePS(LineOut i) : SV_TARGET { return i.col; }
)" };

        struct FrameConstants {
            XMFLOAT4X4 viewProj;
            XMFLOAT4 lightDir;
            XMFLOAT4 camPos;
        };
        struct ObjectConstants {
            XMFLOAT4X4 world;
            XMFLOAT4 color;
            XMFLOAT4 params; // x textured, y selected
        };
        struct LineVertex {
            float pos[3];
            uint32_t color; // ABGR (R8G8B8A8_UNORM)
        };

        struct GpuPart {
            uint32_t start{};
            uint32_t count{};
            ID3D11ShaderResourceView* srv{};
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

        GpuMesh Upload(const scene::Mesh& mesh) {
            GpuMesh g{};
            if (mesh.vertices.empty() || mesh.indices.empty()) return g;
            MakeBuffer(D3D11_BIND_VERTEX_BUFFER, mesh.vertices.data(), mesh.vertices.size() * sizeof(scene::Vertex), &g.vb);
            MakeBuffer(D3D11_BIND_INDEX_BUFFER, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t), &g.ib);
            for (const scene::Part& p : mesh.parts) {
                GpuPart gp{ p.start, p.count, nullptr };
                if (!p.texture.empty()) {
                    auto it{ r.textures.find(p.texture) };
                    if (it == r.textures.end()) {
                        ID3D11ShaderResourceView* srv{};
                        std::vector<uint8_t> rgba{};
                        int w{}, h{};
                        if (LoadImagePixels(p.texture, rgba, w, h)) {
                            // its mips made on the GPU
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
                            if (SUCCEEDED(device->CreateTexture2D(&td, nullptr, &tex))) {
                                context->UpdateSubresource(tex, 0, nullptr, rgba.data(), w * 4, 0);
                                device->CreateShaderResourceView(tex, nullptr, &srv);
                                if (srv) context->GenerateMips(srv);
                                tex->Release();
                            }
                        }
                        else {
                            Logf(LogLevel::WARN, "3d view: can't read the texture %s", ToUtf8(p.texture).c_str());
                        }
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

            D3D11_DEPTH_STENCIL_DESC dd{};
            dd.DepthEnable = TRUE;
            dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
            device->CreateDepthStencilState(&dd, &r.depth);

            UINT quality{};
            if (SUCCEEDED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality)) && quality > 0) {
                r.samples = 4;
            }
            r.cube = Upload(CubeMesh());
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

        void RebuildScene(App& app) {
            for (GpuMesh& g : r.meshes) Free(g);
            r.meshes.clear();
            world.Build(app.project);
            for (const scene::Mesh& m : world.meshes) r.meshes.push_back(Upload(m));
            builtZone = app.project.zone.path;
            builtGeneration = app.project.generation;
            selected = -1;
        }

        bool Visible(const Instance& inst) {
            switch (inst.kind) {
            case Kind::STATIC_MODEL: return showStatic;
            case Kind::SCRIPT_MODEL: return showScript;
            default: return showEntities;
            }
        }

        XMMATRIX WorldOf(const Instance& inst) {
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

        void SetObject(const XMMATRIX& w, const float color[3], bool textured, bool isSelected) {
            ObjectConstants oc{};
            XMStoreFloat4x4(&oc.world, w);
            oc.color = XMFLOAT4(color[0], color[1], color[2], 1);
            oc.params = XMFLOAT4(textured ? 1.0f : 0.0f, isSelected ? 1.0f : 0.0f, 0, 0);
            context->UpdateSubresource(r.objectCB, 0, nullptr, &oc, 0, 0);
        }

        void DrawMesh(const GpuMesh& g, const XMMATRIX& w, const float color[3], bool isSelected) {
            if (!g.vb || !g.ib) return;
            UINT stride{ sizeof(scene::Vertex) };
            UINT offset{};
            context->IASetVertexBuffers(0, 1, &g.vb, &stride, &offset);
            context->IASetIndexBuffer(g.ib, DXGI_FORMAT_R32_UINT, 0);
            for (const GpuPart& p : g.parts) {
                // textured parts take the texture's colors whole
                const float white[3]{ 1, 1, 1 };
                SetObject(w, p.srv ? white : color, p.srv != nullptr, isSelected);
                context->PSSetShaderResources(0, 1, &p.srv);
                context->DrawIndexed(p.count, p.start, 0);
            }
        }

        void Render(int w, int h) {
            if (!MakeTargets(w, h)) return;
            const float clear[4]{ 0.17f, 0.19f, 0.22f, 1 };
            context->ClearRenderTargetView(r.rtv, clear);
            context->ClearDepthStencilView(r.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
            context->OMSetRenderTargets(1, &r.rtv, r.dsv);
            D3D11_VIEWPORT vp{ 0, 0, (float)w, (float)h, 0, 1 };
            context->RSSetViewports(1, &vp);
            context->RSSetState(r.raster);
            context->OMSetDepthStencilState(r.depth, 0);
            context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

            FrameConstants fc{};
            XMStoreFloat4x4(&fc.viewProj, ViewProj((float)w / h));
            XMVECTOR light{ XMVector3Normalize(XMVectorSet(-0.45f, -0.35f, -0.82f, 0)) };
            XMStoreFloat4(&fc.lightDir, light);
            fc.camPos = XMFLOAT4(cam.pos.x, cam.pos.y, cam.pos.z, 1);
            context->UpdateSubresource(r.frameCB, 0, nullptr, &fc, 0, 0);

            ID3D11Buffer* cbs[]{ r.frameCB, r.objectCB };
            context->VSSetConstantBuffers(0, 2, cbs);
            context->PSSetConstantBuffers(0, 2, cbs);
            context->PSSetSamplers(0, 1, &r.sampler);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->IASetInputLayout(r.meshLayout);
            context->VSSetShader(r.meshVS, nullptr, 0);
            context->PSSetShader(r.meshPS, nullptr, 0);
            context->GSSetShader(nullptr, nullptr, 0);

            for (size_t i = 0; i < world.instances.size(); i++) {
                const Instance& inst{ world.instances[i] };
                if (!Visible(inst)) continue;
                XMMATRIX m{ WorldOf(inst) };
                bool sel{ (int)i == selected };
                if (inst.mesh >= 0 && inst.mesh < (int)r.meshes.size()) {
                    DrawMesh(r.meshes[inst.mesh], m, inst.color, sel);
                }
                else {
                    DrawMesh(r.cube, BoxOf(inst) * m, inst.color, sel);
                }
            }

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
                BoxLines(lines, world.instances[selected], Rgba(1, 0.85f, 0.2f, 1));
            }
            if (!lines.empty()) {
                if (lines.size() > r.lineCapacity) {
                    Release(r.lineVB);
                    r.lineCapacity = lines.size() * 2;
                    MakeBuffer(D3D11_BIND_VERTEX_BUFFER, nullptr, r.lineCapacity * sizeof(LineVertex), &r.lineVB, true);
                }
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (r.lineVB && SUCCEEDED(context->Map(r.lineVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
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
            }

            ID3D11RenderTargetView* none{};
            context->OMSetRenderTargets(1, &none, nullptr);
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
                if (!Visible(inst)) continue;
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

        const char* KindName(Kind k) {
            switch (k) {
            case Kind::STATIC_MODEL: return "static model (gfx_map)";
            case Kind::SCRIPT_MODEL: return "script model (entity list)";
            default: return "entity (entity list)";
            }
        }

        void DrawSidePanel(App& app) {
            ImGui::SeparatorText("Show");
            ImGui::Checkbox("Static models", &showStatic);
            ImGui::Checkbox("Script models", &showScript);
            ImGui::Checkbox("Entities", &showEntities);
            ImGui::Checkbox("Grid", &showGrid);

            ImGui::SeparatorText("Camera");
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

            ImGui::SeparatorText("Selected");
            if (selected >= 0 && selected < (int)world.instances.size()) {
                const Instance& inst{ world.instances[selected] };
                ImGui::TextWrapped("%s", inst.label.c_str());
                ImGui::TextDisabled("%s", KindName(inst.kind));
                ImGui::Text("origin %.1f %.1f %.1f", inst.origin[0], inst.origin[1], inst.origin[2]);
                ImGui::Text("angles %.1f %.1f %.1f", inst.angles[0], inst.angles[1], inst.angles[2]);
                if (inst.scale != 1) ImGui::Text("scale %.3f", inst.scale);
                if (inst.mesh >= 0) {
                    const scene::Mesh& m{ world.meshes[inst.mesh] };
                    ImGui::TextDisabled("%zu triangles, %zu parts", m.indices.size() / 3, m.parts.size());
                }
                else {
                    ImGui::TextDisabled("drawn as a box (no mesh the editor reads)");
                }
                if (ImGui::SmallButton("Show its file")) {
                    SelectFile(inst.source);
                    Logf(LogLevel::INFO, "%s selected: see the Zone or Folder tab's preview", ToUtf8(inst.source.filename()).c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ToUtf8(inst.source).c_str());
            }
            else {
                ImGui::TextDisabled("Click something in the view.");
            }

            ImGui::SeparatorText("Objects");
            static ImGuiTextFilter filter{};
            filter.Draw("##objfilter", -1);
            ImGui::BeginChild("##objects", ImVec2(0, 0), ImGuiChildFlags_Border);
            for (size_t i = 0; i < world.instances.size(); i++) {
                const Instance& inst{ world.instances[i] };
                if (!Visible(inst) || !filter.PassFilter(inst.label.c_str())) continue;
                ImGui::PushID((int)i);
                ImGui::ColorButton("##c", ImVec4(inst.color[0], inst.color[1], inst.color[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(10, 10));
                ImGui::SameLine();
                if (ImGui::Selectable(inst.label.c_str(), (int)i == selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    selected = (int)i;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) Frame(inst);
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
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
        cam.yaw = 45;
        cam.pitch = 28;
        // 0 the origin from a corner, 1 everything, 2 straight down on the origin (+x right, +y up)
        if (view == 1) FrameAll();
        else if (view == 2) {
            cam.yaw = 90;
            cam.pitch = 89.9f;
            LookAt(target, 1100);
        }
        else LookAt(target, 900);
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
        r.ready = false;
    }

    void DrawViewportPanel(App& app) {
        if (!r.tried) Init();
        if (!r.ready) {
            ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "The 3D view can't start (see the log).");
            return;
        }
        if (builtZone != app.project.zone.path || builtGeneration != app.project.generation) {
            bool first{ builtGeneration == (size_t)-1 };
            RebuildScene(app);
            // the first look: the origin from above a corner
            if (first) {
                const float target[3]{ 0, 0, 0 };
                cam.yaw = 45;
                cam.pitch = 28;
                LookAt(target, 900);
            }
        }

        if (!ImGui::BeginTable("##viewsplit", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImGui::GetContentRegionAvail())) {
            return;
        }
        ImGui::TableSetupColumn("view", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("side", ImGuiTableColumnFlags_WidthFixed, 340);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

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

        // fly: right mouse looks, WASD / QE (or ctrl / space) move, shift faster, the wheel sets the speed
        if (active && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            cam.yaw -= io.MouseDelta.x * 0.15f;
            cam.pitch = std::clamp(cam.pitch + io.MouseDelta.y * 0.15f, -89.0f, 89.0f);
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
            float scale{ cam.speed / 400.0f };
            XMVECTOR pos{ XMLoadFloat3(&cam.pos) };
            XMVECTOR up{ XMVector3Cross(Right(), Forward()) };
            pos += Right() * (-io.MouseDelta.x * scale) + up * (io.MouseDelta.y * scale);
            XMStoreFloat3(&cam.pos, pos);
        }
        else if (hovered && io.MouseWheel != 0) {
            XMVECTOR pos{ XMLoadFloat3(&cam.pos) };
            XMStoreFloat3(&cam.pos, pos + Forward() * (io.MouseWheel * cam.speed * 0.25f));
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            float nx{ (io.MousePos.x - origin.x) / w * 2 - 1 };
            float ny{ 1 - (io.MousePos.y - origin.y) / h * 2 };
            XMVECTOR ro{}, rd{};
            Ray(nx, ny, aspect, ro, rd);
            selected = Pick(ro, rd);
        }
        if (hovered && selected >= 0 && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            Frame(world.instances[selected]);
        }

        Render(w, h);
        ImDrawList* dl{ ImGui::GetWindowDrawList() };
        dl->AddImage((ImTextureID)r.srv, origin, ImVec2(origin.x + w, origin.y + h));
        char info[256];
        snprintf(info, sizeof(info), "camera %.0f %.0f %.0f  yaw %.0f pitch %.0f  speed %.0f", cam.pos.x, cam.pos.y, cam.pos.z, cam.yaw, cam.pitch, cam.speed);
        dl->AddText(ImVec2(origin.x + 8, origin.y + 6), IM_COL32(230, 230, 230, 220), info);
        dl->AddText(ImVec2(origin.x + 8, origin.y + 6 + ImGui::GetTextLineHeight()), IM_COL32(170, 175, 185, 200),
            "right mouse: look + WASD / QE move (shift faster, wheel speed)   middle: pan   wheel: dolly   left: select   F: frame");

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##side");
        DrawSidePanel(app);
        ImGui::EndChild();
        ImGui::EndTable();
    }
}
