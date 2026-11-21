#ifndef SHADOW_BINDINGS_HLSL
#define SHADOW_BINDINGS_HLSL

//==============================================================================
// shadow_bindings.hlsl  ―― 影ステージの「スタブ」
//
//  pbrPS.hlsl はカスケードシャドウ（段階6）を前提に書かれているが、
//  このプロジェクトにはまだ影のパスが無い。そこで同じ名前・同じ形の関数を
//  用意して「影は常に無し（1.0）」を返すようにしてある。
//
//  影ステージを導入するときは、このファイルを本物の shadow_bindings.hlsl に
//  差し替えるだけでよい。pbrPS.hlsl 側は 1 行も変えずに影対応になる。
//
//  ShadowParams2 の意味（本物と同じ）
//    y = カスケード数（0 なら影なし）
//    z = カスケードのデバッグ色分け表示（0 で無効）
//==============================================================================

static const float4 ShadowParams2 = float4(0.0f, 0.0f, 0.0f, 0.0f);

/// 影の減衰率を返す。1.0 = 影なし / 0.0 = 完全な影。
/// スタブなので常に 1.0。out の cascade は 0 を返す。
float ComputeShadow(float3 worldPos, float3 N, float3 L,
                    float viewDepth, out int cascade)
{
    cascade = 0;
    return 1.0f;
}

/// カスケードの色分け表示用（スタブでは白＝無着色）
float3 CascadeDebugColor(int cascade)
{
    return float3(1.0f, 1.0f, 1.0f);
}

#endif // SHADOW_BINDINGS_HLSL
