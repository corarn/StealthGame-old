#ifndef PBR_BRDF_HLSL
#define PBR_BRDF_HLSL

//==============================================================================
// pbr_brdf.hlsl  ―― Cook-Torrance の各項
//
//  鏡面反射は次の形で書ける。
//
//                    D(H) * G(V,L) * F(V,H)
//      f_specular = -------------------------
//                       4 * (N・V) * (N・L)
//
//   D … 微小面のうち、どれだけがハーフベクトル H を向いているか（法線分布）
//   G … 微小面どうしが遮り合って光が届かない割合（幾何減衰）
//   F … 入射角によって反射率が上がる度合い（フレネル）
//
//  この 3 つを掛けて 4(N・V)(N・L) で割る、という形は変わらない。中身の関数を
//  差し替えるのが「BRDF モデルを変える」ということ。ここでは今の標準である
//  GGX / Smith / Schlick を使う。
//==============================================================================

#include "pbr_common.hlsl"

//------------------------------------------------------------------------------
// D 項：GGX（Trowbridge-Reitz）
//
//  roughness を 2 乗した alpha を使うのが慣例（Disney 由来）。こうすると
//  スライダを直線的に動かしたときの見た目の変化が自然になる。
//------------------------------------------------------------------------------
float D_GGX(float NdotH, float roughness)
{
    const float a  = roughness * roughness;
    const float a2 = a * a;
    const float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PBR_PI * d * d, 1e-7f);
}

//------------------------------------------------------------------------------
// G 項：Smith + Schlick-GGX
//
//  k = (roughness + 1)^2 / 8 は直接光用の定義（Karis 2013）。
//------------------------------------------------------------------------------
float G_SchlickGGX(float NdotX, float k)
{
    return NdotX / max(NdotX * (1.0f - k) + k, 1e-7f);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    const float r = roughness + 1.0f;
    const float k = (r * r) / 8.0f;                 // 直接光用
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}

//------------------------------------------------------------------------------
// F 項：Schlick 近似
//------------------------------------------------------------------------------
float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

#endif // PBR_BRDF_HLSL
