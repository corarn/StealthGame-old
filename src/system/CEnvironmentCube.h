#pragma once
//==============================================================================
// CEnvironmentCube.h
//
//  PBR パス（直接光のみの簡略版）が使う定数バッファ。
//  IBL・環境マップ・法線マップは使わない。
//    b7  CameraBuffer（カメラ位置・姿勢。毎フレーム Update() で転送）
//    b9  EnvBuffer   （露出）
//
//  ※ クラス名は環境マップを持っていた頃の名残り。今はカメラと露出だけを持つ。
//
//  使い方（MyModelScene を参照）
//    init :  m_env.Init();                // Renderer::Init の後
//    draw :  m_env.Update(m_camera);     // カメラ確定後に毎フレーム
//            m_env.Bind();                // PBR シェーダで描く直前
//==============================================================================

#include	<d3d11.h>
#include	<wrl/client.h>
#include	<DirectXMath.h>
#include	"NonCopyable.h"
#include	"camera.h"

class CEnvironmentCube : NonCopyable {
public:
	struct Settings {
		float exposure = 0.0f;		///< 露出 [EV]（Exposure）
	};

	/// 定数バッファを作る。Renderer::Init より後に呼ぶこと。
	bool Init();

	/// b7（カメラ）と b9（露出）を更新する。カメラが確定した後に毎フレーム呼ぶ。
	void Update(const Camera& camera);

	/// b7 / b9 を GPU にセットする。PBR シェーダで描く直前に呼ぶ。
	void Bind() const;

	bool IsValid() const { return static_cast<bool>(m_envBuffer); }

	Settings&       EditSettings()      { return m_settings; }
	const Settings& GetSettings() const { return m_settings; }

private:
	Settings m_settings{};

	Microsoft::WRL::ComPtr<ID3D11Buffer>             m_cameraBuffer;	// b7
	Microsoft::WRL::ComPtr<ID3D11Buffer>             m_envBuffer;		// b9
};
