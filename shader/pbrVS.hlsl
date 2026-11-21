//==============================================================================
// pbrVS.hlsl
//
//  PBR 用の頂点シェーダ（スキニング無し）。
//
//  既存の vertexLightingVS.hlsl との違いは 1 点だけ。
//  **頂点シェーダではライティングを一切しない。** ワールド座標と法線をそのまま
//  ピクセルシェーダへ渡し、陰影はピクセル単位で計算する。
//  鏡面反射（ハイライト）は法線の向きで急激に変わるので、頂点で計算すると
//  三角形が大きいときにひどく粗くなる。
//
//  行列の掛け方（mul の引数の順番）は既存シェーダと完全に同じにしてある。
//  ここを変えるとプロジェクトの行列の規約と食い違って表示が壊れる。
//==============================================================================

#include "common.hlsl"
#include "pbr_io.hlsl"

PBR_PS_IN main(in VS_IN In)
{
    PBR_PS_IN Out;

    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);

    // 法線変換行列（非一様スケールを打ち消す）。既存シェーダと同じ計算。
    float3x3 normalMatrix = Inverse3x3(float3x3(World._11, World._12, World._13,
                                                World._21, World._22, World._23,
                                                World._31, World._32, World._33));
    normalMatrix = transpose(normalMatrix);

    Out.Position    = mul(In.Position, wvp);
    Out.WorldPos    = mul(In.Position, World).xyz;
    Out.WorldNormal = normalize(mul(In.Normal.xyz, normalMatrix));
    Out.TexCoord    = In.TexCoord;
    Out.Diffuse     = In.Diffuse;

    return Out;
}
