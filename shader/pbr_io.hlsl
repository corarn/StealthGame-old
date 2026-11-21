#ifndef PBR_IO_HLSL
#define PBR_IO_HLSL

//==============================================================================
// pbr_io.hlsl
//
//  PBR パス用の VS -> PS 受け渡し構造体。
//
//  既存の PS_IN には SV_POSITION / COLOR0 / TEXCOORD0 しか無く、
//  ワールド座標も法線も入っていない（頂点シェーダで陰影を計算し終えていたため）。
//  PBR はピクセル単位で計算するので、この 2 つが要る。
//
//  common.hlsl の PS_IN には手を入れず、新しい構造体を別に用意している。
//  こうすれば既存のシェーダは 1 行も変わらない。
//==============================================================================

struct PBR_PS_IN
{
    float4 Position    : SV_POSITION;
    float3 WorldPos    : TEXCOORD1;   // ワールド空間の位置（視線ベクトル V の計算で使う）
    float3 WorldNormal : TEXCOORD2;   // ワールド空間の法線（正規化はピクセル側で再度行う）
    float2 TexCoord    : TEXCOORD0;
    float4 Diffuse     : COLOR0;      // 頂点カラー
};

#endif // PBR_IO_HLSL
