//==============================================================================
// pbrOneSkinVS.hlsl
//
//  PBR 用の頂点シェーダ（ワンスキン＝ボーン 4 本ブレンド）。
//
//  【既存 vertexLightingOneSkinVS.hlsl のバグを直してある】
//    旧実装は頂点位置にはボーン行列 comb を掛けているのに、法線には掛けていない。
//
//        Pos         = mul(In.Position, comb);   // 位置は変形する
//        worldNormal = mul(normal, World);       // 法線は comb を無視している
//
//    そのため関節を曲げても法線がバインドポーズのまま追従しない。頂点ライティングの
//    緩い陰影では気付きにくいが、PBR の鏡面反射を入れると即座に破綻する
//    （曲げた腕のハイライトが動かない）。ここでは comb の回転成分を法線にも掛けている。
//
//  【なぜ 3x3 で掛けるのか】
//    法線は方向ベクトルなので平行移動を受けてはいけない。4x4 のまま掛けると
//    comb の平行移動成分が乗ってしまう。float3x3 へキャストして回転・スケールだけを使う。
//
//    厳密にはスケールが非一様な場合は逆転置行列が要るが、スキニング行列に非一様
//    スケールが入ることはまず無いので、ここでは回転成分をそのまま使っている。
//==============================================================================

#include "common.hlsl"
#include "pbr_io.hlsl"

PBR_PS_IN main(in VSONESKIN_IN In)
{
    PBR_PS_IN Out;

    // ボーン 4 本の重み付き合成行列
    float4x4 comb = (float4x4) 0;
    for (int i = 0; i < 4; i++)
    {
        comb += BoneMatrix[In.BoneIndex[i]] * In.BoneWeight[i];
    }

    float4 skinnedPos    = mul(In.Position, comb);
    float3 skinnedNormal = mul(In.Normal.xyz, (float3x3) comb);   // ← 既存実装が抜かしている所

    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);

    float3x3 normalMatrix = Inverse3x3(float3x3(World._11, World._12, World._13,
                                                World._21, World._22, World._23,
                                                World._31, World._32, World._33));
    normalMatrix = transpose(normalMatrix);

    Out.Position    = mul(skinnedPos, wvp);
    Out.WorldPos    = mul(skinnedPos, World).xyz;
    Out.WorldNormal = normalize(mul(skinnedNormal, normalMatrix));
    Out.TexCoord    = In.TexCoord;
    Out.Diffuse     = In.Diffuse;

    return Out;
}
