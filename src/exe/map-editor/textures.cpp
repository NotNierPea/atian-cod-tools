#include "editor.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace editor {
    namespace {
        ID3D11Device* device{};
        std::map<fs::path, Texture> textures{};

        Texture Load(const fs::path& path) {
            Texture tex{};
            FILE* f{};
            if (_wfopen_s(&f, path.c_str(), L"rb") || !f) {
                tex.error = "can't read the file";
                return tex;
            }
            int channels{};
            stbi_uc* pixels{ stbi_load_from_file(f, &tex.width, &tex.height, &channels, 4) };
            fclose(f);
            if (!pixels) {
                tex.error = stbi_failure_reason() ? stbi_failure_reason() : "can't decode the image";
                return tex;
            }

            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = tex.width;
            desc.Height = tex.height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA sub{ pixels, (UINT)tex.width * 4, 0 };
            ID3D11Texture2D* texture{};
            if (FAILED(device->CreateTexture2D(&desc, &sub, &texture))) {
                tex.error = "can't create the texture";
            }
            else {
                D3D11_SHADER_RESOURCE_VIEW_DESC view{};
                view.Format = desc.Format;
                view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                view.Texture2D.MipLevels = 1;
                device->CreateShaderResourceView(texture, &view, &tex.srv);
                texture->Release();
            }
            stbi_image_free(pixels);
            return tex;
        }
    }

    void SetDevice(ID3D11Device* dev) { device = dev; }

    bool WritePng(const fs::path& path, int width, int height, const uint8_t* rgba) {
        return stbi_write_png(ToUtf8(path).c_str(), width, height, 4, rgba, width * 4) != 0;
    }

    bool LoadImagePixels(const fs::path& path, std::vector<uint8_t>& rgba, int& width, int& height) {
        FILE* f{};
        if (_wfopen_s(&f, path.c_str(), L"rb") || !f) return false;
        int channels{};
        stbi_uc* pixels{ stbi_load_from_file(f, &width, &height, &channels, 4) };
        fclose(f);
        if (!pixels) return false;
        rgba.assign(pixels, pixels + (size_t)width * height * 4);
        stbi_image_free(pixels);
        return true;
    }

    const Texture& GetTexture(const fs::path& path) {
        auto it{ textures.find(path) };
        if (it == textures.end()) {
            it = textures.emplace(path, Load(path)).first;
        }
        return it->second;
    }

    void ClearTextures() {
        for (auto& [path, tex] : textures) {
            if (tex.srv) tex.srv->Release();
        }
        textures.clear();
    }
}
