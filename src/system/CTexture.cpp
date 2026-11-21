#include	<iostream>
#include	<fstream>
#include	"CTexture.h"
#include	"stb_image.h"
#include	"renderer.h"
#include	<mutex>
#include	<unordered_map>
#include	<cstdint>
#include	<cstring>
#include	<vector>

namespace {

	// パス → 作成済み SRV のキャッシュ。
	// 同じテクスチャを複数のマテリアル／モデルが参照していると、そのたびに
	// PNG デコード（stb_image）と GPU リソース生成が走って読み込みが遅くなる。
	// SRV は ComPtr（参照カウント）なので複数の CTexture で共有して問題ない。
	struct TextureCacheEntry {
		ComPtr<ID3D11ShaderResourceView> srv;
		int width  = 0;
		int height = 0;
		int bpp    = 0;
	};
	std::unordered_map<std::string, TextureCacheEntry> g_textureCache;

	// テクスチャはモデル読み込み時に複数スレッドから並列に読まれるので、
	// キャッシュへのアクセスはこのミューテックスで保護する。
	std::mutex g_textureCacheMutex;

	//--------------------------------------------------------------------------
	// DDS（BC 圧縮テクスチャ）の読み込み
	//
	//  モデルコンバータの --dds / --bc7 が書き出す「BC 圧縮＋全ミップマップ入り」の
	//  DDS を読む最小実装。PNG と違って CPU でのデコードが不要で、ヘッダを
	//  解釈してそのまま CreateTexture2D へ渡すだけなので非常に速い。
	//
	//  対応形式
	//    - レガシーヘッダ（FourCC "DXT1" / "DXT3" / "DXT5"）       … --dds (BC3)
	//    - DX10 拡張ヘッダ（FourCC "DX10" + DXGI_FORMAT_BC1〜BC7） … --bc7 (BC7)
	//  BC7 は FourCC が存在しない新しい形式なので、必ず DX10 拡張ヘッダで書かれる。
	//  BC7 を使うには機能レベル 11_0 以上の GPU が必要（CheckFormatSupport で確認）。
	//--------------------------------------------------------------------------
#pragma pack(push, 1)
	struct DdsPixelFormat {
		std::uint32_t size;
		std::uint32_t flags;
		char          fourCC[4];
		std::uint32_t rgbBitCount;
		std::uint32_t rBitMask, gBitMask, bBitMask, aBitMask;
	};
	struct DdsHeader {
		std::uint32_t  size;				// 常に 124
		std::uint32_t  flags;
		std::uint32_t  height;
		std::uint32_t  width;
		std::uint32_t  pitchOrLinearSize;
		std::uint32_t  depth;
		std::uint32_t  mipMapCount;
		std::uint32_t  reserved1[11];
		DdsPixelFormat ddspf;
		std::uint32_t  caps, caps2, caps3, caps4;
		std::uint32_t  reserved2;
	};
	struct DdsHeaderDx10 {					// fourCC=="DX10" のときだけ続く拡張ヘッダ
		std::uint32_t dxgiFormat;
		std::uint32_t resourceDimension;
		std::uint32_t miscFlag;
		std::uint32_t arraySize;
		std::uint32_t miscFlags2;
	};
#pragma pack(pop)
	static_assert(sizeof(DdsHeader) == 124, "DDS ヘッダは 124 bytes");
	static_assert(sizeof(DdsHeaderDx10) == 20, "DX10 拡張ヘッダは 20 bytes");

	constexpr std::uint32_t kDx10DimensionTexture2D = 3;	// D3D10_RESOURCE_DIMENSION_TEXTURE2D
	constexpr std::uint32_t kDx10MiscTextureCube    = 0x4;	// D3D11_RESOURCE_MISC_TEXTURECUBE

	/// 先頭 4 バイトのマジックで DDS かどうかを判定する
	bool IsDds(const unsigned char* data, std::size_t size)
	{
		return size >= 4 && std::memcmp(data, "DDS ", 4) == 0;
	}

	/// FourCC / DX10 拡張ヘッダから DXGI フォーマットと 1 ブロックのバイト数を決める
	bool ResolveDdsFormat(const DdsHeader& header, const DdsHeaderDx10* dx10,
						  DXGI_FORMAT& outFormat, std::size_t& outBlockBytes)
	{
		if (dx10 != nullptr) {
			outFormat = static_cast<DXGI_FORMAT>(dx10->dxgiFormat);
			switch (outFormat) {
			case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM:
				outBlockBytes = 8;  return true;
			case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
				outBlockBytes = 16; return true;
			default:
				return false;		// BC 圧縮以外は対象外
			}
		}
		const auto fourCCIs = [&header](const char* cc) {
			return std::memcmp(header.ddspf.fourCC, cc, 4) == 0;
		};
		if (fourCCIs("DXT1")) { outFormat = DXGI_FORMAT_BC1_UNORM; outBlockBytes = 8;  return true; }
		if (fourCCIs("DXT3")) { outFormat = DXGI_FORMAT_BC2_UNORM; outBlockBytes = 16; return true; }
		if (fourCCIs("DXT5")) { outFormat = DXGI_FORMAT_BC3_UNORM; outBlockBytes = 16; return true; }
		return false;
	}

	/// DDS のバイト列から ID3D11Texture2D と SRV を作る（全ミップを一括転送）
	bool CreateDdsTexture(const unsigned char* data, std::size_t size,
						  ComPtr<ID3D11ShaderResourceView>& outSrv,
						  int& outWidth, int& outHeight)
	{
		constexpr std::uint32_t kDdpfFourCC = 0x4;

		if (size < 4 + sizeof(DdsHeader)) {
			return false;
		}
		DdsHeader header{};
		std::memcpy(&header, data + 4, sizeof(DdsHeader));
		if (header.size != 124 || (header.ddspf.flags & kDdpfFourCC) == 0) {
			return false;		// 非圧縮 DDS などは対象外（コンバータは FourCC 形式で書く）
		}

		std::size_t offset = 4 + sizeof(DdsHeader);

		DdsHeaderDx10 dx10{};
		const bool hasDx10 = std::memcmp(header.ddspf.fourCC, "DX10", 4) == 0;
		if (hasDx10) {
			if (size < offset + sizeof(DdsHeaderDx10)) {
				return false;
			}
			std::memcpy(&dx10, data + offset, sizeof(DdsHeaderDx10));
			offset += sizeof(DdsHeaderDx10);

			// この読み込み処理が扱うのは「1 枚の 2D テクスチャ」だけ
			// （配列テクスチャ・キューブマップ・ボリュームは対象外）
			if (dx10.resourceDimension != kDx10DimensionTexture2D ||
				dx10.arraySize > 1 || (dx10.miscFlag & kDx10MiscTextureCube) != 0) {
				std::cerr << "DDS: 2D テクスチャ以外（配列/キューブ等）には未対応です" << std::endl;
				return false;
			}
		}

		DXGI_FORMAT format     = DXGI_FORMAT_UNKNOWN;
		std::size_t blockBytes = 0;
		if (!ResolveDdsFormat(header, hasDx10 ? &dx10 : nullptr, format, blockBytes)) {
			if (hasDx10) {
				std::cerr << "DDS: 未対応の DXGI フォーマット " << dx10.dxgiFormat << std::endl;
			} else {
				std::cerr << "DDS: 未対応の FourCC "
						  << std::string(header.ddspf.fourCC, 4) << std::endl;
			}
			return false;
		}

		// GPU がそのフォーマットを 2D テクスチャとして扱えるか確認する。
		// BC7 は機能レベル 11_0 以上でないと使えないので、古い GPU / WARP では
		// CreateTexture2D が失敗する前にここで原因が分かるようにしておく。
		{
			UINT support = 0;
			const HRESULT hrSupport = Renderer::GetDevice()->CheckFormatSupport(format, &support);
			if (FAILED(hrSupport) || (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) == 0) {
				std::cerr << "DDS: この GPU は DXGI フォーマット " << static_cast<int>(format)
						  << (format == DXGI_FORMAT_BC7_UNORM || format == DXGI_FORMAT_BC7_UNORM_SRGB
							  ? "（BC7 / 機能レベル 11_0 以上が必要）" : "")
						  << " に対応していません" << std::endl;
				return false;
			}
		}

		// BC 圧縮はトップレベルの縦横が 4 の倍数でないと CreateTexture2D が失敗する
		// （コンバータは 4 の倍数に揃えて書き出すので、通常はここに来ない）
		if (header.width == 0 || header.height == 0 ||
			(header.width % 4) != 0 || (header.height % 4) != 0) {
			std::cerr << "DDS: BC 圧縮テクスチャのサイズが 4 の倍数ではありません ("
					  << header.width << "x" << header.height << ")" << std::endl;
			return false;
		}

		const UINT mipCount = (header.mipMapCount > 0) ? header.mipMapCount : 1;

		// 各ミップレベルの先頭位置とピッチを求める（BC は 4x4 texel = 1 ブロック）
		std::vector<D3D11_SUBRESOURCE_DATA> initData(mipCount);
		for (UINT level = 0; level < mipCount; ++level) {
			UINT w = header.width  >> level; if (w == 0) w = 1;
			UINT h = header.height >> level; if (h == 0) h = 1;
			const std::size_t rowPitch  = ((w + 3) / 4) * blockBytes;
			const std::size_t levelSize = rowPitch * ((h + 3) / 4);

			if (offset + levelSize > size) {
				return false;	// ファイルが途中で切れている
			}
			initData[level].pSysMem          = data + offset;
			initData[level].SysMemPitch      = static_cast<UINT>(rowPitch);
			initData[level].SysMemSlicePitch = 0;
			offset += levelSize;
		}

		D3D11_TEXTURE2D_DESC desc{};
		desc.Width            = header.width;
		desc.Height           = header.height;
		desc.MipLevels        = mipCount;
		desc.ArraySize        = 1;
		desc.Format           = format;
		desc.SampleDesc.Count = 1;
		desc.Usage            = D3D11_USAGE_DEFAULT;
		desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

		ComPtr<ID3D11Texture2D> texture;
		HRESULT hr = Renderer::GetDevice()->CreateTexture2D(&desc, initData.data(), texture.GetAddressOf());
		if (FAILED(hr)) {
			std::cerr << "DDS: CreateTexture2D に失敗 (hr=0x" << std::hex
					  << static_cast<unsigned long>(hr) << std::dec << ")" << std::endl;
			return false;
		}
		hr = Renderer::GetDevice()->CreateShaderResourceView(texture.Get(), nullptr,
															 outSrv.ReleaseAndGetAddressOf());
		if (FAILED(hr)) {
			return false;
		}
		outWidth  = static_cast<int>(header.width);
		outHeight = static_cast<int>(header.height);
		return true;
	}

} // namespace

void CTexture::ClearCache()
{
	std::lock_guard<std::mutex> lock(g_textureCacheMutex);
	g_textureCache.clear();
}

// u8対応
bool CTexture::Load(const std::u8string& filename)
{
	std::filesystem::path filepath = filename;
	m_texname = filepath.string();  // UTF-8として保持

	// 同じファイルを読んだことがあれば、デコード・リソース生成を丸ごと省略して
	// 作成済みの SRV を共有する（モデル読み込み高速化の要）。
	{
		std::lock_guard<std::mutex> lock(g_textureCacheMutex);
		if (const auto it = g_textureCache.find(m_texname); it != g_textureCache.end()) {
			m_srv    = it->second.srv;
			m_width  = it->second.width;
			m_height = it->second.height;
			m_bpp    = it->second.bpp;
			return true;
		}
	}

	std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
	if (!ifs) {
		std::cerr << "Failed to open file: " << filepath << std::endl;
		return false;
	}

	std::streamsize size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::vector<unsigned char> buffer(size);
	if (!ifs.read(reinterpret_cast<char*>(buffer.data()), size)) {
		std::cerr << "Failed to read file: " << filepath << std::endl;
		return false;
	}

	// DDS（BC 圧縮済み・ミップマップ入り）ならデコード不要でそのまま GPU へ渡す
	if (IsDds(buffer.data(), buffer.size())) {
		if (!CreateDdsTexture(buffer.data(), buffer.size(), m_srv, m_width, m_height)) {
			std::cerr << "Failed to load DDS: " << filepath << std::endl;
			return false;
		}
		m_bpp = 0;	// 圧縮フォーマットなので BPP は意味を持たない

		std::lock_guard<std::mutex> lock(g_textureCacheMutex);
		g_textureCache[m_texname] = { m_srv, m_width, m_height, m_bpp };
		return true;
	}

	unsigned char* pixels = stbi_load_from_memory(
		buffer.data(), static_cast<int>(buffer.size()),
		&m_width, &m_height, &m_bpp, STBI_rgb_alpha);

	if (!pixels) {
		std::cerr << "Failed to decode image: " << filepath << std::endl;
		return false;
	}

	ComPtr<ID3D11Texture2D> pTexture;
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = m_width * 4;

	HRESULT hr = Renderer::GetDevice()->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	hr = Renderer::GetDevice()->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	stbi_image_free(pixels);

	if (FAILED(hr)) {
		return false;
	}

	// 次回以降のためにキャッシュへ登録しておく
	{
		std::lock_guard<std::mutex> lock(g_textureCacheMutex);
		g_textureCache[m_texname] = { m_srv, m_width, m_height, m_bpp };
	}
	return true;
}

// テクスチャをロード
bool CTexture::Load(const std::string& filename)
{
	bool sts = true;


	// 画像読み込み
//	pixels = stbi_load(filename.c_str(), &m_width, &m_height, &m_bpp, 4);
//	if (pixels == nullptr) {
//		std::cout << filename.c_str() << " Load error " << std::endl;

		// u8文字文字列に　2025/7/12 by suzuki.tomoki
		std::u8string u8s(filename.begin(),filename.end());
		sts =Load(u8s);
		if (sts == false) {
			return false;
		}
		return true;
//	}

/*
// テクスチャ2Dリソース生成
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV生成
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// ピクセルイメージ解放
	stbi_image_free(pixels);

	return true;


*/
}

// テクスチャをメモリからロード
bool CTexture::LoadFromFemory(const unsigned char* Data,int len) {

	// DDS（BC 圧縮済み）ならデコード不要でそのまま GPU へ渡す。
	// .gmdl の埋め込みテクスチャ（format=="dds"）はこの分岐を通る。
	if (len > 0 && IsDds(Data, static_cast<std::size_t>(len))) {
		if (!CreateDdsTexture(Data, static_cast<std::size_t>(len), m_srv, m_width, m_height)) {
			std::cerr << "Failed to load embedded DDS" << std::endl;
			return false;
		}
		m_bpp = 0;	// 圧縮フォーマットなので BPP は意味を持たない
		return true;
	}

	bool sts = true;
	unsigned char* pixels;

	// 画像読み込み
	pixels = stbi_load_from_memory(Data, 
		len, 
		&m_width, 
		&m_height, 
		&m_bpp, 
		STBI_rgb_alpha);

	// テクスチャ2Dリソース生成
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV生成
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// ピクセルイメージ解放
	stbi_image_free(pixels);

	return true;
}

// テクスチャをGPUにセット
void CTexture::SetGPU()
{
	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->PSSetShaderResources(0, 1, m_srv.GetAddressOf());
}

// テクスチャを任意のスロットへセット（法線マップを t4 に張るときなど）
void CTexture::SetGPU(int slot)
{
	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->PSSetShaderResources(static_cast<UINT>(slot), 1, m_srv.GetAddressOf());
}
