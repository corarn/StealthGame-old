#include	"CPbrMaterial.h"
#include	"renderer.h"
#include	"dx11helper.h"

namespace {

	/// b8 の実レイアウト。shader/env_bindings.hlsl の PbrMaterialBuffer と
	/// 並び・パッキングを完全に一致させること（16 バイト境界 x 3 行 = 48 バイト）。
	struct CBPbrMaterial {
		DirectX::XMFLOAT3 Albedo;        float Metallic;          // 1 行目
		float Roughness; float AO;
		int   UseAlbedoTexture;          int   UseMaterialColor;  // 2 行目
		DirectX::XMFLOAT3 F0;            int   F0Mode;            // 3 行目
	};
	static_assert(sizeof(CBPbrMaterial) == 48, "b8 のレイアウトが env_bindings.hlsl とずれています");

	constexpr UINT kPbrMaterialSlot = 8;   // register(b8)

} // namespace

bool CPbrMaterial::Init()
{
	ID3D11Device* device = Renderer::GetDevice();
	if (device == nullptr) {
		return false;
	}

	bool sts = CreateConstantBuffer(
		device,
		sizeof(CBPbrMaterial),
		m_buffer.GetAddressOf());
	if (!sts) {
		MessageBox(nullptr, L"CreateBuffer(constant buffer PbrMaterial) error", L"Error", MB_OK);
		return false;
	}

	m_dirty = true;
	return true;
}

void CPbrMaterial::Bind() const
{
	if (!m_buffer) {
		return;
	}

	ID3D11DeviceContext* devcontext = Renderer::GetDeviceContext();

	// 値が変わったときだけ GPU へ転送する（毎フレーム Bind してもコストは一定）
	if (m_dirty) {
		CBPbrMaterial cb{};
		cb.Albedo           = m_params.albedo;
		cb.Metallic         = m_params.metallic;
		cb.Roughness        = m_params.roughness;
		cb.AO               = m_params.ao;
		cb.UseAlbedoTexture = m_params.useAlbedoTexture ? 1 : 0;
		cb.UseMaterialColor = m_params.useMaterialColor ? 1 : 0;
		cb.F0               = m_params.f0;
		cb.F0Mode           = m_params.f0Manual ? 1 : 0;

		devcontext->UpdateSubresource(m_buffer.Get(), 0, nullptr, &cb, 0, 0);
		m_dirty = false;
	}

	devcontext->VSSetConstantBuffers(kPbrMaterialSlot, 1, m_buffer.GetAddressOf());
	devcontext->PSSetConstantBuffers(kPbrMaterialSlot, 1, m_buffer.GetAddressOf());
}
