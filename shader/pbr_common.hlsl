#ifndef PBR_COMMON_HLSL
#define PBR_COMMON_HLSL

//==============================================================================
// pbr_common.hlsl
//
//  PBR で共通に使う小さな関数群。common.hlsl とは別ファイルにしてあるので、
//  既存のシェーダには一切影響しない。
//==============================================================================

static const float PBR_PI = 3.141592653589793f;

//------------------------------------------------------------------------------
// sRGB <-> リニア
//
//  PBR の計算はすべてリニア空間で行う。一方、
//   - PNG / JPG のアルベドテクスチャは sRGB で符号化されている（読むときに戻す）
//   - バックバッファは R8G8B8A8_UNORM（_SRGB ではない）なので、
//     ハードウェアが変換してくれない。最後に自分でエンコードする
//------------------------------------------------------------------------------
float3 SRGBToLinear(float3 c)
{
    c = saturate(c);
    return lerp(c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f), step(0.04045f, c));
}

float3 LinearToSRGB(float3 c)
{
    c = saturate(c);
    return lerp(c * 12.92f, 1.055f * pow(c, 1.0f / 2.4f) - 0.055f, step(0.0031308f, c));
}

//------------------------------------------------------------------------------
// トーンマップ
//
//  HDR（太陽は 10 万を超える）を 0〜1 に収める。Narkowicz の ACES 近似。
//  正確な ACES ではないが 5 個の定数だけで済み、見た目は十分近い。
//------------------------------------------------------------------------------
float3 TonemapACES(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

/// 露出 → トーンマップ → sRGB エンコードまでを一括で行う。
/// この 3 つは必ずセットで、しかも PBR のパスの中だけで完結させること。
/// 外に出すと既存のワールド軸・関節表示・ImGui まで巻き込んでしまう。
float3 ToDisplay(float3 linearColor, float exposureEV)
{
    return LinearToSRGB(TonemapACES(linearColor * exp2(exposureEV)));
}

#endif // PBR_COMMON_HLSL
