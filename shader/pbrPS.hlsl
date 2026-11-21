//==============================================================================
// pbrPS.hlsl ―― 授業用のシンプルな PBR（直接光のみ）
//
//  計算するのは次の 2 つだけ。IBL・環境マップ・法線マップは使わない。
//
//    1. 直接光（平行光源 1 本）… Cook-Torrance
//         拡散  kD * albedo/pi * Φ * NdotL
//         鏡面  D*G*F / (4 NdotV NdotL) * Φ * NdotL
//
//    2. 一定のアンビエント（Light.Ambient）
//         影の側が完全な黒に潰れないようにするための最低限の底上げ。
//
//  F0（垂直入射の反射率）は金属かどうかで決まる。
//    誘電体（metallic = 0）… 0.04 くらいで、色は付かない。色は拡散反射側が持つ
//    金属  （metallic = 1）… albedo がそのまま反射色になり、拡散反射は消える
//  この「金属には拡散反射が無い」が metallic ワークフローの肝。
//==============================================================================

#include "common.hlsl"
#include "pbr_io.hlsl"
#include "env_bindings.hlsl"
#include "pbr_common.hlsl"
#include "pbr_brdf.hlsl"

Texture2D    g_Texture      : register(t0);
SamplerState g_SamplerState : register(s0);

// アルベドテクスチャを sRGB データとして扱うかどうか。
//   1 … シェーダ内でリニアへ戻す（CTexture が UNORM で SRV を作っている現状はこちら）
//   0 … 何もしない（CTexture を DXGI_FORMAT_R8G8B8A8_UNORM_SRGB に変えたらこちら）
#define ALBEDO_TEXTURE_IS_SRGB_DATA 1

float4 main(PBR_PS_IN In) : SV_Target
{
    float3 N = normalize(In.WorldNormal);
    float3 V = normalize(CameraPos.xyz - In.WorldPos);

    // 裏面を描いているときは法線が視線と逆を向く。そのまま使うと真っ黒になるので反転する。
    if (dot(N, V) < 0.0f) N = -N;

    const float NdotV = saturate(dot(N, V));

    // --- マテリアル -----------------------------------------------------------
    float4 base = float4(PbrAlbedo, 1.0f);
    if (PbrUseMaterialColor != 0)
    {
        base *= Material.Diffuse * In.Diffuse;
    }
    if (PbrUseAlbedoTexture != 0 && Material.TextureEnable)
    {
        float4 tex = g_Texture.Sample(g_SamplerState, In.TexCoord);
#if ALBEDO_TEXTURE_IS_SRGB_DATA
        base.rgb *= SRGBToLinear(tex.rgb);
#else
        base.rgb *= tex.rgb;
#endif
        base.a *= tex.a;
    }

    const float3 albedo    = base.rgb;
    const float  metallic  = saturate(PbrMetallic);
    // roughness の下限を切っておく。0 にすると鏡面が 1 テクセルに集中して
    // 動かしたときにチラつく（D 項の分母がほぼ 0 になる）。
    const float  roughness = clamp(PbrRoughness, 0.04f, 1.0f);
    const float  ao        = saturate(PbrAO);

    // 誘電体の垂直入射反射率は 0.04 前後。金属では albedo がそのまま F0 になる。
    // これが metallic ワークフローの定義そのもの。
    const float3 autoF0 = lerp(0.04f.xxx, albedo, metallic);

    // フレネル値は手入力にも切り替えられる（PbrF0Mode）。
    // 「F0 がどこから来るか」を差し替えているだけで、以降の式は変わらない。
    const float3 F0 = (PbrF0Mode != 0) ? saturate(PbrF0) : autoF0;

    // --- 1. 直接光（平行光源 1 本）---------------------------------------------
    // Light は common.hlsl の b4。方向は ImGui（Directional Light）から変えられる。
    float3 direct = float3(0.0f, 0.0f, 0.0f);
    if (Light.Enable)
    {
        const float3 L     = normalize(-Light.Direction.xyz);   // 「光が来る」方向
        const float3 H     = normalize(V + L);
        const float  NdotL = saturate(dot(N, L));

        if (NdotL > 0.0f)
        {
            const float  D = D_GGX(saturate(dot(N, H)), roughness);
            const float  G = G_Smith(NdotV, NdotL, roughness);
            const float3 F = F_Schlick(saturate(dot(H, V)), F0);

            const float3 specular = (D * G) * F / max(4.0f * NdotV * NdotL, 1e-4f);

            // F は「鏡面へ回った割合」。残りが拡散へ回る。金属は拡散を持たない。
            const float3 kD = (1.0f - F) * (1.0f - metallic);

            direct = (kD * albedo / PBR_PI + specular) * Light.Diffuse.rgb * NdotL;
        }
    }

    // --- 2. 一定のアンビエント --------------------------------------------------
    // 拡散の環境光は Light.Ambient で一定。金属は拡散を持たないので消す。
    const float3 ambient = albedo * Light.Ambient.rgb * ao * (1.0f - metallic);

    float3 color = direct + ambient + Material.Emission.rgb;

    // --- 表示用に変換（露出 → トーンマップ → sRGB。このパスの中だけで完結）------
    return float4(ToDisplay(color, Exposure), base.a);
}
