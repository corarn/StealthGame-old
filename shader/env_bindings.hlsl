#ifndef ENV_BINDINGS_HLSL
#define ENV_BINDINGS_HLSL

//==============================================================================
// env_bindings.hlsl
//
//  PBR パス（直接光のみ）が使う定数バッファの宣言。
//
//  ※ IBL・環境マップ・法線マップは使わない。
//
//  既存の common.hlsl は b0〜b6 / t0 / s0 を使っている。ここではそれを避けて
//  b7 / b8 / b9 だけを使う。common.hlsl には手を入れていないので、
//  既存のシェーダの挙動は完全にそのまま。
//
//   b7  CameraBuffer       … カメラの位置と姿勢（CEnvironmentCube が更新）
//   b8  PbrMaterialBuffer  … PBR マテリアル（CPbrMaterial が更新）
//   b9  EnvBuffer          … 露出（CEnvironmentCube が更新）
//==============================================================================

cbuffer CameraBuffer : register(b7)
{
    float4 CameraPos;       // xyz = ワールド空間のカメラ位置
    float4 CameraRight;     // xyz = 右方向 x tan(fovY/2) x aspect
    float4 CameraUp;        // xyz = 上方向 x tan(fovY/2)
    float4 CameraForward;   // xyz = 前方向（単位ベクトル）
};

cbuffer EnvBuffer : register(b9)
{
    float  Exposure;        // 露出 [EV]
    float3 EnvPadding;
};

cbuffer PbrMaterialBuffer : register(b8)
{
    float3 PbrAlbedo;              // 基本色（リニア）
    float  PbrMetallic;            // 0 = 誘電体, 1 = 金属
    float  PbrRoughness;           // 0 = 鏡, 1 = 完全に粗い
    float  PbrAO;                  // 環境遮蔽（手入力）
    int    PbrUseAlbedoTexture;    // ディフューズテクスチャを掛けるか
    int    PbrUseMaterialColor;    // .gmdl の Material.Diffuse を掛けるか

    float3 PbrF0;                  // フレネル値。PbrF0Mode = 1 のときだけ使う
    int    PbrF0Mode;              // 0 = Auto（metallic から自動）, 1 = Manual（手入力）
};

#endif // ENV_BINDINGS_HLSL
