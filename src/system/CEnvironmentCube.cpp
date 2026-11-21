#include	<cmath>

#include	"CEnvironmentCube.h"
#include	"renderer.h"
#include	"dx11helper.h"
#include	"../application.h"

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;

namespace {

	//--------------------------------------------------------------------------
	// b7 / b9 の実レイアウト（shader/env_bindings.hlsl と 1:1 に揃えること）
	//--------------------------------------------------------------------------
	struct CBCamera {
		XMFLOAT4 Pos;		// xyz = カメラ位置
		XMFLOAT4 Right;		// xyz = 右方向 x tan(fovY/2) x aspect
		XMFLOAT4 Up;		// xyz = 上方向 x tan(fovY/2)
		XMFLOAT4 Forward;	// xyz = 前方向（単位ベクトル）
	};
	static_assert(sizeof(CBCamera) == 64);

	struct CBEnv {
		float Exposure;		// 露出 [EV]
		float Padding[3];
	};
	static_assert(sizeof(CBEnv) == 16);

	//--------------------------------------------------------------------------
	// 小さなベクトル演算（SimpleMath に依存しないよう float 直書き）
	//--------------------------------------------------------------------------
	XMFLOAT3 Normalize3(const XMFLOAT3& v)
	{
		const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		if (len <= 1e-8f) return XMFLOAT3(0.0f, 1.0f, 0.0f);
		return XMFLOAT3(v.x / len, v.y / len, v.z / len);
	}
	XMFLOAT3 Cross3(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		return XMFLOAT3(a.y * b.z - a.z * b.y,
						a.z * b.x - a.x * b.z,
						a.x * b.y - a.y * b.x);
	}

} // namespace

//==============================================================================
// Init
//==============================================================================
bool CEnvironmentCube::Init()
{
	ID3D11Device* device = Renderer::GetDevice();
	if (device == nullptr) {
		return false;
	}

	// ---- 定数バッファ（b7 / b9）--------------------------------------------
	if (!CreateConstantBuffer(device, sizeof(CBCamera), m_cameraBuffer.GetAddressOf())) {
		MessageBox(nullptr, L"CreateBuffer(constant buffer Camera b7) error", L"Error", MB_OK);
		return false;
	}
	if (!CreateConstantBuffer(device, sizeof(CBEnv), m_envBuffer.GetAddressOf())) {
		MessageBox(nullptr, L"CreateBuffer(constant buffer Env b9) error", L"Error", MB_OK);
		return false;
	}

	return true;
}

//==============================================================================
// Update : b7（カメラ）と b9（露出）を転送する
//==============================================================================
void CEnvironmentCube::Update(const Camera& camera)
{
	if (!m_cameraBuffer || !m_envBuffer) {
		return;
	}

	ID3D11DeviceContext* devcontext = Renderer::GetDeviceContext();

	// ---- b7 : カメラ ---------------------------------------------------------
	// fovY とアスペクト比は camera.cpp の Draw() と同じ値を使うこと。
	{
		const Vector3 pos    = camera.GetPosition();
		const Vector3 lookat = camera.GetLookat();
		const Vector3 up     = camera.GetUP();

		XMFLOAT3 forward = Normalize3(XMFLOAT3(lookat.x - pos.x, lookat.y - pos.y, lookat.z - pos.z));
		XMFLOAT3 right   = Normalize3(Cross3(XMFLOAT3(up.x, up.y, up.z), forward));	// 左手系
		XMFLOAT3 upv     = Cross3(forward, right);

		constexpr float fovY   = DirectX::XMConvertToRadians(45.0f);	// camera.cpp と合わせる
		const float     aspect = static_cast<float>(Application::GetWidth())
							   / static_cast<float>(Application::GetHeight());
		const float tanHalf = std::tan(fovY * 0.5f);

		CBCamera cb{};
		cb.Pos     = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);
		cb.Right   = XMFLOAT4(right.x * tanHalf * aspect, right.y * tanHalf * aspect, right.z * tanHalf * aspect, 0.0f);
		cb.Up      = XMFLOAT4(upv.x * tanHalf, upv.y * tanHalf, upv.z * tanHalf, 0.0f);
		cb.Forward = XMFLOAT4(forward.x, forward.y, forward.z, 0.0f);
		devcontext->UpdateSubresource(m_cameraBuffer.Get(), 0, nullptr, &cb, 0, 0);
	}

	// ---- b9 : 露出 ------------------------------------------------------------
	{
		CBEnv cb{};
		cb.Exposure = m_settings.exposure;
		devcontext->UpdateSubresource(m_envBuffer.Get(), 0, nullptr, &cb, 0, 0);
	}
}

//==============================================================================
// Bind : b7 / b9 をセットする
//==============================================================================
void CEnvironmentCube::Bind() const
{
	ID3D11DeviceContext* devcontext = Renderer::GetDeviceContext();

	devcontext->VSSetConstantBuffers(7, 1, m_cameraBuffer.GetAddressOf());
	devcontext->PSSetConstantBuffers(7, 1, m_cameraBuffer.GetAddressOf());
	devcontext->VSSetConstantBuffers(9, 1, m_envBuffer.GetAddressOf());
	devcontext->PSSetConstantBuffers(9, 1, m_envBuffer.GetAddressOf());
}
