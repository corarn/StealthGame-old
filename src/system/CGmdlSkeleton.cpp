//==============================================================================
// CGmdlSkeleton.cpp
//==============================================================================
#include	<algorithm>
#include	<cmath>

#include	"CGmdlSkeleton.h"

#include	"renderer.h"
#include	"LineDrawer.h"
#include	"SphereDrawer.h"
#include	"gmdl/ModelFormat.h"

namespace {

	namespace gmdl = gm31::model;

	// ---- 表示色 -------------------------------------------------------------
	const Color COLOR_JOINT       (0.20f, 0.80f, 1.00f, 1.0f);	// 関節（水色）
	const Color COLOR_JOINT_BONE  (1.00f, 0.55f, 0.10f, 1.0f);	// スキニングに使われる関節（橙）
	const Color COLOR_JOINT_SELECT(1.00f, 0.95f, 0.20f, 1.0f);	// 選択中（黄）
	const Color COLOR_BONE        (0.85f, 0.85f, 0.85f, 1.0f);	// 骨（白）
	const Color COLOR_BONE_SELECT (1.00f, 0.95f, 0.20f, 1.0f);	// 選択中につながる骨
	const Color COLOR_AXIS_X      (1.00f, 0.25f, 0.25f, 1.0f);
	const Color COLOR_AXIS_Y      (0.25f, 1.00f, 0.25f, 1.0f);
	const Color COLOR_AXIS_Z      (0.35f, 0.55f, 1.00f, 1.0f);

	/// gmdl の行列 → エンジンの行列。
	/// どちらも「行ベクトル規約・行優先格納」で並びが同じなので、
	/// 成分をそのまま移すだけでよい（転置は不要）。
	[[nodiscard]] Matrix4x4 ToEngineMatrix(const gmdl::Matrix4x4& s) noexcept
	{
		Matrix4x4 d;
		d._11 = s.m[0][0]; d._12 = s.m[0][1]; d._13 = s.m[0][2]; d._14 = s.m[0][3];
		d._21 = s.m[1][0]; d._22 = s.m[1][1]; d._23 = s.m[1][2]; d._24 = s.m[1][3];
		d._31 = s.m[2][0]; d._32 = s.m[2][1]; d._33 = s.m[2][2]; d._34 = s.m[2][3];
		d._41 = s.m[3][0]; d._42 = s.m[3][1]; d._43 = s.m[3][2]; d._44 = s.m[3][3];
		return d;
	}

	/// 行列の平行移動成分（＝その姿勢の原点）
	[[nodiscard]] Vector3 TranslationOf(const Matrix4x4& m) noexcept
	{
		return Vector3(m._41, m._42, m._43);
	}

	/// 2 つの行列がどれだけ違うか（成分ごとの差の最大値）。
	/// 「ほぼ同じ行列か？」の判定だけに使うので、これで十分。
	[[nodiscard]] float MaxAbsDiff(const Matrix4x4& a, const Matrix4x4& b) noexcept
	{
		float d = 0.0f;
		d = std::max(d, std::fabs(a._11 - b._11)); d = std::max(d, std::fabs(a._12 - b._12));
		d = std::max(d, std::fabs(a._13 - b._13)); d = std::max(d, std::fabs(a._14 - b._14));
		d = std::max(d, std::fabs(a._21 - b._21)); d = std::max(d, std::fabs(a._22 - b._22));
		d = std::max(d, std::fabs(a._23 - b._23)); d = std::max(d, std::fabs(a._24 - b._24));
		d = std::max(d, std::fabs(a._31 - b._31)); d = std::max(d, std::fabs(a._32 - b._32));
		d = std::max(d, std::fabs(a._33 - b._33)); d = std::max(d, std::fabs(a._34 - b._34));
		d = std::max(d, std::fabs(a._41 - b._41)); d = std::max(d, std::fabs(a._42 - b._42));
		d = std::max(d, std::fabs(a._43 - b._43)); d = std::max(d, std::fabs(a._44 - b._44));
		return d;
	}

	/// アフィン行列の逆行列（行ベクトル規約）。
	///     M = | R 0 |          M^-1 = |    R^-1    0 |
	///         | t 1 |                 | -t * R^-1  1 |
	/// 上 3x3 は余因子行列 / 行列式で求める。
	/// ライブラリの Invert に頼らないのは、透視変換を含まないと分かっている
	/// ここでは自前のほうが速く、依存も減るため。
	[[nodiscard]] Matrix4x4 AffineInverse(const Matrix4x4& m) noexcept
	{
		const float a = m._11, b = m._12, c = m._13;
		const float d = m._21, e = m._22, f = m._23;
		const float g = m._31, h = m._32, i = m._33;

		const float cof11 =  (e * i - f * h);
		const float cof12 = -(d * i - f * g);
		const float cof13 =  (d * h - e * g);

		const float det = a * cof11 + b * cof12 + c * cof13;
		if (std::fabs(det) < 1e-12f) {
			return Matrix4x4::Identity;			// 潰れた行列。補正できないので何もしない
		}
		const float invDet = 1.0f / det;

		Matrix4x4 r = Matrix4x4::Identity;
		// adj = 余因子行列の転置
		r._11 =  cof11 * invDet;
		r._12 = -(b * i - c * h) * invDet;
		r._13 =  (b * f - c * e) * invDet;
		r._21 =  cof12 * invDet;
		r._22 =  (a * i - c * g) * invDet;
		r._23 = -(a * f - c * d) * invDet;
		r._31 =  cof13 * invDet;
		r._32 = -(a * h - b * g) * invDet;
		r._33 =  (a * e - b * d) * invDet;

		const float tx = m._41, ty = m._42, tz = m._43;
		r._41 = -(tx * r._11 + ty * r._21 + tz * r._31);
		r._42 = -(tx * r._12 + ty * r._22 + tz * r._32);
		r._43 = -(tx * r._13 + ty * r._23 + tz * r._33);
		return r;
	}

	/// 正規化。長さ 0 のときは零ベクトルを返す（0 除算で NaN を撒かないため）。
	[[nodiscard]] Vector3 Normalized(const Vector3& v) noexcept
	{
		const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		if (len < 1e-6f) {
			return Vector3(0.0f, 0.0f, 0.0f);
		}
		const float inv = 1.0f / len;
		return Vector3(v.x * inv, v.y * inv, v.z * inv);
	}

	/// 2 点を結ぶ線を引く。
	/// LineDrawerDraw は「始点・単位方向・長さ」を受け取る仕様なので、
	/// ここで正規化してから渡す。
	void DrawLineBetween(const Vector3& from, const Vector3& to, const Color& color)
	{
		const Vector3 delta = to - from;
		const float   len   = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
		if (len < 1e-5f) {
			return;								// 長さゼロの骨は描かない
		}
		LineDrawerDraw(len, from, Vector3(delta.x / len, delta.y / len, delta.z / len), color);
	}

} // namespace

//------------------------------------------------------------------------------
void CGmdlSkeleton::Clear()
{
	m_nodes.clear();
	m_bones.clear();
	m_roots.clear();
	m_bindLocal.clear();
	m_world.clear();
	m_rotation.clear();
	m_translation.clear();

	m_bindCorrection      = Matrix4x4::Identity;
	m_hasBindCorrection   = false;
	m_applyBindCorrection = true;
	m_bindOutlierCount    = 0;
}

//------------------------------------------------------------------------------
void CGmdlSkeleton::Build(const gmdl::Model& model)
{
	Clear();

	const std::size_t count = model.nodes.size();
	if (count == 0) {
		return;
	}

	m_nodes.resize(count);
	m_bindLocal.resize(count);
	m_world.resize(count);
	m_rotation.assign(count, Vector3(0.0f, 0.0f, 0.0f));
	m_translation.assign(count, Vector3(0.0f, 0.0f, 0.0f));

	for (std::size_t i = 0; i < count; ++i) {
		const gmdl::Node& src = model.nodes[i];

		m_nodes[i].name   = src.name;
		m_nodes[i].parent = static_cast<int>(src.parent);		// kInvalidIndex(-1) がそのまま入る
		m_nodes[i].isBone = (src.boneIndex != gmdl::kInvalidIndex);

		m_bindLocal[i] = ToEngineMatrix(src.localTransform);
	}

	// 親子リンクを張る。pre-order なので親は必ず先に確定している。
	for (std::size_t i = 0; i < count; ++i) {
		const int parent = m_nodes[i].parent;
		if (parent < 0) {
			m_roots.push_back(static_cast<int>(i));
		} else {
			m_nodes[static_cast<std::size_t>(parent)].children.push_back(static_cast<int>(i));
		}
	}

	// スキニング用の情報（オフセット行列）を控えておく。
	// Model への参照は持たない ＝ モデルを差し替えても宙ぶらりんにならない。
	m_bones.reserve(model.bones.size());
	for (const gmdl::Bone& bone : model.bones) {
		BoneInfo info;
		info.nodeIndex    = static_cast<int>(bone.nodeIndex);
		info.offsetMatrix = ToEngineMatrix(bone.offsetMatrix);
		m_bones.push_back(info);
	}

	DetectBindCorrection();		// ここで m_world を素の階層で 1 回埋める
	Update();					// 補正を掛けた本番の m_world を作る
}

//------------------------------------------------------------------------------
// ノード階層と offsetMatrix のずれを検出する
//
//   本来なら offsetMatrix[b] * bindWorld[boneNode[b]] == 単位行列 になる。
//   ならないモデルがあるので、そのずれ D を求めて逆行列を補正として持っておく。
//
//   ★ 平均ではなく「多数決」で選ぶ。
//     MMD の IK 補助ボーン（左足ＩＫ 等）は頂点に影響しないので
//     offsetMatrix が当てにならず、平均を取ると全体が引きずられる。
//     実測（halchan1）では 199 本中 191 本がぴたりと一致し、
//     外れたのは IK 系の 8 本だけだった。
//------------------------------------------------------------------------------
void CGmdlSkeleton::DetectBindCorrection()
{
	m_bindCorrection    = Matrix4x4::Identity;
	m_hasBindCorrection = false;
	m_bindOutlierCount  = 0;

	if (m_bones.empty()) {
		return;
	}

	// 補正なしの素の階層でワールド行列を作る（この時点では差分もゼロ）
	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		const int parent = m_nodes[i].parent;
		m_world[i] = (parent < 0)
				   ? m_bindLocal[i]
				   : m_bindLocal[i] * m_world[static_cast<std::size_t>(parent)];
	}

	// ---- ボーンごとのずれ D[b] ---------------------------------------------
	std::vector<Matrix4x4> deltas;
	deltas.reserve(m_bones.size());

	float characteristicSize = 1.0f;			// モデルの大きさ。許容誤差の基準にする
	for (const BoneInfo& bone : m_bones) {
		if (bone.nodeIndex < 0 || static_cast<std::size_t>(bone.nodeIndex) >= m_world.size()) {
			continue;
		}
		const Matrix4x4& w = m_world[static_cast<std::size_t>(bone.nodeIndex)];
		deltas.push_back(bone.offsetMatrix * w);

		const Vector3 p = TranslationOf(w);
		characteristicSize = std::max({ characteristicSize,
										std::fabs(p.x), std::fabs(p.y), std::fabs(p.z) });
	}
	if (deltas.empty()) {
		return;
	}

	// 許容誤差はモデルの大きさに比例させる。
	// 絶対値で決め打ちすると、単位が 1 のモデルと 100 のモデルで意味が変わってしまう。
	const float tolerance = 1e-3f * characteristicSize;

	// ---- 多数決 -------------------------------------------------------------
	// N は数百程度なので O(N^2) で困らない（読み込み時に 1 回だけ）。
	std::size_t bestIndex = 0;
	int         bestVotes = -1;
	for (std::size_t i = 0; i < deltas.size(); ++i) {
		int votes = 0;
		for (std::size_t j = 0; j < deltas.size(); ++j) {
			if (MaxAbsDiff(deltas[i], deltas[j]) <= tolerance) {
				++votes;
			}
		}
		if (votes > bestVotes) {
			bestVotes = votes;
			bestIndex = i;
		}
	}

	const Matrix4x4& consensus = deltas[bestIndex];
	m_bindOutlierCount = static_cast<int>(deltas.size()) - bestVotes;

	// 単位行列なら補正不要（大半のモデルはこちら）
	if (MaxAbsDiff(consensus, Matrix4x4::Identity) <= tolerance) {
		return;
	}

	// D を打ち消す行列をルートに掛ければ、全ノードのワールドが D^-1 ぶん動く。
	//     world_new[i] = world[i] * D^-1
	//  → offset[b] * world_new = D * D^-1 = 単位行列
	m_bindCorrection    = AffineInverse(consensus);
	m_hasBindCorrection = true;
}

//------------------------------------------------------------------------------
Vector3 CGmdlSkeleton::GetBindCorrectionTranslation() const
{
	return TranslationOf(m_bindCorrection);
}

//------------------------------------------------------------------------------
void CGmdlSkeleton::SetBindCorrectionEnabled(bool enable)
{
	if (m_applyBindCorrection == enable) {
		return;
	}
	m_applyBindCorrection = enable;
	Update();
}

//------------------------------------------------------------------------------
void CGmdlSkeleton::ResetPose()
{
	const Vector3 zero(0.0f, 0.0f, 0.0f);
	m_rotation.assign(m_rotation.size(), zero);
	m_translation.assign(m_translation.size(), zero);
	Update();
}

//------------------------------------------------------------------------------
// 差分 → ローカル → ワールド
//
//   local[i] = Rdelta * bindLocal * Tdelta
//   world[i] = local[i] * world[parent]        （ルートだけ * bindCorrection）
//
// pre-order 格納（parent < i）のおかげで、この 1 ループで全ノードが確定する。
//------------------------------------------------------------------------------
void CGmdlSkeleton::Update()
{
	const Matrix4x4 rootBase = (m_hasBindCorrection && m_applyBindCorrection)
							 ? m_bindCorrection
							 : Matrix4x4::Identity;

	for (std::size_t i = 0; i < m_nodes.size(); ++i) {
		const Vector3& rot = m_rotation[i];
		const Vector3& pos = m_translation[i];

		Matrix4x4 local = m_bindLocal[i];

		// 差分がゼロのときは行列を掛けない。
		// バインドポーズを浮動小数の誤差で汚さないための小細工でもある。
		if (rot.x != 0.0f || rot.y != 0.0f || rot.z != 0.0f) {
			local = Matrix4x4::CreateFromYawPitchRoll(rot.y, rot.x, rot.z) * local;
		}
		if (pos.x != 0.0f || pos.y != 0.0f || pos.z != 0.0f) {
			local = local * Matrix4x4::CreateTranslation(pos);
		}

		const int parent = m_nodes[i].parent;
		m_world[i] = (parent < 0)
				   ? local * rootBase
				   : local * m_world[static_cast<std::size_t>(parent)];
	}
}

//------------------------------------------------------------------------------
Vector3 CGmdlSkeleton::GetNodeWorldPosition(int nodeIndex, const Matrix4x4& modelWorld) const
{
	return TranslationOf(m_world[static_cast<std::size_t>(nodeIndex)] * modelWorld);
}

//------------------------------------------------------------------------------
std::vector<Matrix4x4> CGmdlSkeleton::BuildSkinningPalette() const
{
	std::vector<Matrix4x4> palette(m_bones.size(), Matrix4x4::Identity);

	for (std::size_t i = 0; i < m_bones.size(); ++i) {
		const int nodeIndex = m_bones[i].nodeIndex;
		if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= m_world.size()) {
			continue;							// 単位行列のまま
		}
		palette[i] = m_bones[i].offsetMatrix * m_world[static_cast<std::size_t>(nodeIndex)];
	}
	return palette;
}

//------------------------------------------------------------------------------
int CGmdlSkeleton::FindDisplayedParent(int nodeIndex, bool boneNodesOnly) const
{
	int parent = m_nodes[static_cast<std::size_t>(nodeIndex)].parent;

	if (!boneNodesOnly) {
		return parent;
	}

	// ボーンでない中継ノードを飛ばして、直近のボーンノードまで遡る
	while (parent >= 0 && !m_nodes[static_cast<std::size_t>(parent)].isBone) {
		parent = m_nodes[static_cast<std::size_t>(parent)].parent;
	}
	return parent;
}

//------------------------------------------------------------------------------
void CGmdlSkeleton::Draw(const Matrix4x4& modelWorld, const DrawOptions& options) const
{
	if (m_nodes.empty()) {
		return;
	}
	if (!options.showJoint && !options.showBone && !options.showAxis) {
		return;
	}

	const std::size_t count = m_nodes.size();

	// ---- 画面上の姿勢をまとめて求めておく ----------------------------------
	// 骨の線と関節の球で同じ値を 2 回使うので、先に配列にしてしまう。
	std::vector<Matrix4x4> finalMatrix(count);
	std::vector<Vector3>   position(count);
	for (std::size_t i = 0; i < count; ++i) {
		finalMatrix[i] = m_world[i] * modelWorld;
		position[i]    = TranslationOf(finalMatrix[i]);
	}

	// メッシュに埋もれると関節が見えないので、既定では深度テストを切る。
	if (options.alwaysOnTop) {
		Renderer::SetDepthEnable(false);
	}

	// ---- 骨（親 → 子の線）---------------------------------------------------
	// 線 → 軸 → 球 の順に描く。線はジオメトリシェーダを使うので、
	// 最後に球（頂点＋ピクセルシェーダのみ）を描いて状態を戻しておく。
	if (options.showBone) {
		SetLineWidth(options.boneWidth);

		for (std::size_t i = 0; i < count; ++i) {
			if (options.boneNodesOnly && !m_nodes[i].isBone) {
				continue;
			}
			const int parent = FindDisplayedParent(static_cast<int>(i), options.boneNodesOnly);
			if (parent < 0) {
				continue;						// ルートには親が無い
			}

			const bool highlight = (options.selected == static_cast<int>(i))
								|| (options.selected == parent);

			DrawLineBetween(position[static_cast<std::size_t>(parent)],
							position[i],
							highlight ? COLOR_BONE_SELECT : COLOR_BONE);
		}
	}

	// ---- 関節のローカル軸 ---------------------------------------------------
	if (options.showAxis) {
		SetLineWidth(options.boneWidth);

		for (std::size_t i = 0; i < count; ++i) {
			if (options.boneNodesOnly && !m_nodes[i].isBone) {
				continue;
			}
			const Matrix4x4& m = finalMatrix[i];

			// 行ベクトル規約なので、各行がローカル軸のワールド方向そのもの。
			// modelWorld のスケールが乗っているため、正規化してから長さを与える。
			const Vector3 axisX = Normalized(Vector3(m._11, m._12, m._13));
			const Vector3 axisY = Normalized(Vector3(m._21, m._22, m._23));
			const Vector3 axisZ = Normalized(Vector3(m._31, m._32, m._33));

			const Vector3& o = position[i];
			const float    L = options.axisLength;

			LineDrawerDraw(L, o, axisX, COLOR_AXIS_X);
			LineDrawerDraw(L, o, axisY, COLOR_AXIS_Y);
			LineDrawerDraw(L, o, axisZ, COLOR_AXIS_Z);
		}
	}

	// ---- 関節（球）---------------------------------------------------------
	if (options.showJoint) {
		for (std::size_t i = 0; i < count; ++i) {
			if (options.boneNodesOnly && !m_nodes[i].isBone) {
				continue;
			}

			Color color = m_nodes[i].isBone ? COLOR_JOINT_BONE : COLOR_JOINT;
			float radius = options.jointRadius;

			if (options.selected == static_cast<int>(i)) {
				color  = COLOR_JOINT_SELECT;
				radius = options.jointRadius * 1.6f;	// 選択中は少し大きく
			}

			const Vector3& p = position[i];
			SphereDrawerDraw(radius, color, p.x, p.y, p.z);
		}
	}

	// 切った深度テストは必ず戻す。戻し忘れると
	// 「このシーンを通った後だけ他のシーンの前後関係が壊れる」という
	// 原因の分かりにくい不具合になる。
	if (options.alwaysOnTop) {
		Renderer::SetDepthEnable(true);
	}
}
