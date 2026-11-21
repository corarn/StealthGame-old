#pragma once
//==============================================================================
// CPbrMaterial.h
//
//  PBR マテリアル（b8 の定数バッファ）。
//  レイアウトは shader/env_bindings.hlsl の PbrMaterialBuffer と 1:1 で対応する。
//
//  .gmdl（PBR 版）の PBRM チャンクに入っている値（gm31::model::PbrMaterial）を
//  そのまま Params に詰めて使う。PBR チャンクを持たない従来モデルでは、
//  従来マテリアルから近似値を作って使う（CGmdlMeshRenderer::Init を参照）。
//==============================================================================

#include	<d3d11.h>
#include	<wrl/client.h>
#include	<DirectXMath.h>
#include	"NonCopyable.h"

class CPbrMaterial
{
public:
    struct Params {
        DirectX::XMFLOAT3 albedo   { 1.0f, 1.0f, 1.0f };
        float metallic  = 0.0f;
        float roughness = 0.5f;
        float ao        = 1.0f;
        DirectX::XMFLOAT3 f0 { 0.04f, 0.04f, 0.04f };  ///< フレネル値（f0Manual のときだけ使う）
        bool  f0Manual  = false;         ///< false = metallic から自動で決める

        // ---- 以下 3 つは描画には使わない ----------------------------------
        // この簡略版では法線マップ処理をカットしてある。.gmdl（PBRM チャンク）の
        // 値を保存時に失わないよう、読み込んだ値をそのまま持ち回るだけ。
        float normalStrength   = 1.0f;   ///< 法線マップの強さ（未使用・保存用）
        bool  useNormalMap     = true;   ///< 法線マップを使うか（未使用・保存用）
        bool  flipGreenChannel = false;  ///< 緑チャンネルを反転（未使用・保存用）

        bool  useAlbedoTexture = true;   ///< .gmdl のディフューズテクスチャを掛けるか
        bool  useMaterialColor = true;   ///< .gmdl の Material.Diffuse / 頂点カラーを掛けるか
    };

    CPbrMaterial() = default;
    CPbrMaterial(const CPbrMaterial&)            = delete;
    CPbrMaterial& operator=(const CPbrMaterial&) = delete;

    /// 定数バッファを作る。Renderer::Init より後に呼ぶこと
    bool Init();

    bool IsValid() const { return static_cast<bool>(m_buffer); }

    void          SetParams(const Params& p) { m_params = p; m_dirty = true; }
    const Params& GetParams() const          { return m_params; }
    Params&       EditParams()               { m_dirty = true; return m_params; }

    /// b8 へ書き込んで VS / PS に張る
    void Bind() const;

    /// metallic ワークフローでの F0（Auto のときにシェーダが使う値）。ImGui の表示用
    static DirectX::XMFLOAT3 AutoF0(const Params& p)
    {
        return DirectX::XMFLOAT3(0.04f + (p.albedo.x - 0.04f) * p.metallic,
                                 0.04f + (p.albedo.y - 0.04f) * p.metallic,
                                 0.04f + (p.albedo.z - 0.04f) * p.metallic);
    }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
    Params       m_params{};
    mutable bool m_dirty = true;
};
