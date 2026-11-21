#pragma once
//==============================================================================
// CGmdlSkeleton.h
//
//  .gmdl のノード階層（＝関節）を「姿勢を持つ骨格」として保持し、
//  可視化するクラス。
//
//  ■ 役割の分担
//      CGmdlMesh          … ファイルを読む（形状・マテリアル・階層データ）
//      CGmdlSkeleton      … 階層データから「今のポーズ」を作る＋関節を描く ← ここ
//      CGmdlMeshRenderer  … 形状を描く
//
//  ■ ポーズの持ち方（← ここがポーザーへの布石）
//      ノード i のローカル行列を、こう定義している。
//
//          local[i] = Rdelta[i] * bindLocal[i] * Tdelta[i]
//
//        bindLocal[i] … ファイルに入っていた「親から見た初期姿勢」。絶対に触らない。
//        Rdelta[i]    … 関節を "自分の原点まわりに" 回す差分回転。UI で動かすのはここ。
//        Tdelta[i]    … 親空間での平行移動の差分（普段は 0。ルートの移動などに使う）。
//
//      ★ 差分がゼロなら local[i] == bindLocal[i]。
//        つまり「何も操作していない状態は、必ずバインドポーズと完全一致」する。
//        元データを直接書き換える設計にすると、一度触ったら二度と戻せなくなる。
//
//      ★ 行ベクトル規約（v' = v * M）なので「先に適用するものほど左」。
//          ・Rdelta が bindLocal より左 → 回転は関節のローカル原点まわりになる
//            （右に置くと親の原点まわりに回ってしまい、腕が肩からもげる）
//          ・Tdelta が bindLocal より右 → 平行移動は親空間で効く
//
//  ■ ワールド行列（モデル空間）
//      nodes は pre-order 格納で parent < 自分が保証されている。
//      だから再帰も stack も要らず、前から 1 パス回すだけで確定する。
//
//          world[i] = local[i] * world[parent]
//
//  ■ バインドポーズ補正（BindCorrection）  ★ 重要
//      ノード階層と offsetMatrix（逆バインドポーズ）は、本来こういう関係にある。
//
//          offsetMatrix[b] * bindWorld[ boneNode[b] ] == 単位行列
//
//      ところが変換元によっては、この式が単位行列にならないモデルがある。
//      （halchan1 / halchan2 では ちょうど 平行移動 (0, -7, +0.1) ぶんずれる。
//        つまりノード階層だけが Y 方向に 7 だけ低い位置に置かれている）
//
//      どちらが正しいかというと offsetMatrix のほう。理由は単純で、
//      **頂点はスキニング行列 offsetMatrix * nodeWorld で動かす**と決まっている以上、
//      メッシュが実際に立っている場所を決めているのは offsetMatrix だから。
//      ノード階層のほうがずれていると、関節だけがメッシュから浮く。
//
//      そこで Build() のときに、
//        ・全ボーンについて D[b] = offsetMatrix[b] * bindWorld[boneNode[b]] を求め、
//        ・「最も多くのボーンが一致する D」を多数決で選び（＝外れ値に強い）、
//        ・その逆行列をルートノードに掛ける
//      という補正を入れている。ルートに 1 回掛けるだけなので、
//      親子関係もポーズ編集の仕組みも一切壊れない。
//
//      ★ 多数決にしているのは MMD の IK 補助ボーン（左足ＩＫ など）対策。
//        あれは頂点に一切影響しないので offsetMatrix が意味を持たず、
//        平均を取ると外れ値に引きずられる。実測では 199 本中 8 本が
//        この IK ボーンで、残り 191 本はぴたりと一致した。
//
//  ■ 使い方
//      m_skeleton.Build(mesh->GetModel());          // 読み込み直後に 1 回
//      m_skeleton.Update();                         // ポーズを変えたら呼ぶ
//      m_skeleton.Draw(worldMatrix, drawOptions);   // 描画時
//==============================================================================

#include	<string>
#include	<vector>

#include	"commontypes.h"
#include	"transform.h"

// ModelFormat.h をここで include しないのは、このヘッダを使う側に
// gmdl の詳細まで引きずり込まないため（Build() の実装だけが知っていればよい）。
namespace gm31::model { struct Model; }

class CGmdlSkeleton {
public:
	/// 選択なしを表す値
	static constexpr int kNoSelection = -1;

	//--------------------------------------------------------------------------
	// 描画オプション
	//--------------------------------------------------------------------------
	struct DrawOptions {
		bool  showJoint     = true;		///< 関節を球で描く
		bool  showBone      = true;		///< 親子を線でつなぐ
		bool  showAxis      = false;	///< 関節のローカル軸（X=赤 Y=緑 Z=青）を描く
		bool  boneNodesOnly = false;	///< スキニングに使われるノードだけ描く
		bool  alwaysOnTop   = true;		///< 深度テストを切ってメッシュの手前に出す

		float jointRadius = 1.5f;		///< 関節の球の半径【ワールド単位】
		float axisLength  = 8.0f;		///< ローカル軸の長さ【ワールド単位】
		float boneWidth   = 2.0f;		///< 骨の線の太さ

		int   selected = kNoSelection;	///< 強調表示する関節（ImGui のツリーで選ぶ）
	};

	//--------------------------------------------------------------------------
	// 構築
	//--------------------------------------------------------------------------

	/// モデルのノード階層から骨格を作る。ポーズはバインドポーズで初期化される。
	void Build(const gm31::model::Model& model);

	/// 中身を捨てる（モデルの読み込みに失敗したときなど）
	void Clear();

	[[nodiscard]] bool IsValid() const noexcept { return !m_nodes.empty(); }

	//--------------------------------------------------------------------------
	// ポーズ
	//--------------------------------------------------------------------------

	/// 差分をすべてゼロに戻す（＝バインドポーズ）。Update() まで行う。
	void ResetPose();

	/// 差分 → ローカル行列 → ワールド行列 を計算し直す。
	/// ポーズを変えた後に必ず呼ぶこと。
	void Update();

	/// 関節の回転差分【ラジアン】(x=pitch, y=yaw, z=roll)。
	/// 参照を返すので ImGui::SliderAngle にそのまま渡せる。
	/// 書き換えたら Update() を呼ぶこと。
	[[nodiscard]] Vector3& JointRotation(int nodeIndex) { return m_rotation[nodeIndex]; }
	[[nodiscard]] const Vector3& JointRotation(int nodeIndex) const { return m_rotation[nodeIndex]; }

	/// 関節の平行移動差分（親空間）
	[[nodiscard]] Vector3& JointTranslation(int nodeIndex) { return m_translation[nodeIndex]; }
	[[nodiscard]] const Vector3& JointTranslation(int nodeIndex) const { return m_translation[nodeIndex]; }

	//--------------------------------------------------------------------------
	// バインドポーズ補正（クラス冒頭の解説を参照）
	//--------------------------------------------------------------------------

	/// 補正が必要なモデルだったか（＝ノード階層と offsetMatrix がずれていたか）
	[[nodiscard]] bool HasBindCorrection() const noexcept { return m_hasBindCorrection; }

	/// 補正の平行移動成分。UI に出して「何がどれだけずれていたか」を見せるため。
	[[nodiscard]] Vector3 GetBindCorrectionTranslation() const;

	/// 補正に合わなかったボーンの数（MMD の IK 補助ボーンなど）
	[[nodiscard]] int GetBindCorrectionOutlierCount() const noexcept { return m_bindOutlierCount; }

	[[nodiscard]] bool IsBindCorrectionEnabled() const noexcept { return m_applyBindCorrection; }

	/// 補正の ON / OFF。切り替えると Update() まで行う（ずれ具合を目で比べるため）。
	void SetBindCorrectionEnabled(bool enable);

	//--------------------------------------------------------------------------
	// 参照
	//--------------------------------------------------------------------------

	[[nodiscard]] int GetNodeCount() const noexcept { return static_cast<int>(m_nodes.size()); }

	[[nodiscard]] const std::string& GetNodeName(int nodeIndex) const { return m_nodes[nodeIndex].name; }
	[[nodiscard]] int  GetNodeParent(int nodeIndex) const { return m_nodes[nodeIndex].parent; }
	[[nodiscard]] bool IsBoneNode(int nodeIndex)     const { return m_nodes[nodeIndex].isBone; }

	[[nodiscard]] const std::vector<int>& GetChildren(int nodeIndex) const { return m_nodes[nodeIndex].children; }
	[[nodiscard]] const std::vector<int>& GetRoots() const noexcept { return m_roots; }

	/// スキニングに使われているノードの数（＝ボーン数）
	[[nodiscard]] int GetBoneCount() const noexcept { return static_cast<int>(m_bones.size()); }

	/// モデル空間での関節の姿勢
	[[nodiscard]] const Matrix4x4& GetNodeWorldMatrix(int nodeIndex) const { return m_world[nodeIndex]; }

	/// 画面上（ワールド空間）での関節の位置。modelWorld はシーンのワールド行列。
	[[nodiscard]] Vector3 GetNodeWorldPosition(int nodeIndex, const Matrix4x4& modelWorld) const;

	/// シェーダへ送るスキニング行列パレットを、今のポーズで作る。
	///     palette[b] = offsetMatrix[b] * world[ boneNode[b] ]
	/// ※ メッシュを実際に変形させる段階（ポーザー本番）で使う。
	///   バインドポーズ補正は world 側に入っているので、ここは何もしなくてよい。
	[[nodiscard]] std::vector<Matrix4x4> BuildSkinningPalette() const;

	//--------------------------------------------------------------------------
	// 描画
	//--------------------------------------------------------------------------

	/// 関節・骨・軸を描く。
	/// @param modelWorld シーン側のワールド行列（BuildWorldMatrix() の結果）
	/// @param options    表示設定
	///
	/// ★ 球と線はサイズを「ワールド単位」で指定する。モデル側のスケールを
	///   掛けないので、どんな単位のモデルでも関節の見た目の大きさが変わらない。
	void Draw(const Matrix4x4& modelWorld, const DrawOptions& options) const;

private:
	struct NodeInfo {
		std::string      name;
		int              parent = -1;		///< ルートは -1
		bool             isBone = false;	///< スキニングに使われているか
		std::vector<int> children;
	};

	struct BoneInfo {
		int       nodeIndex = -1;
		Matrix4x4 offsetMatrix{};			///< モデル空間 → ボーン空間（逆バインド）
	};

	/// boneNodesOnly のとき、表示対象になる最も近い祖先を返す（無ければ -1）。
	/// これをやらないと、途中に非ボーンノードが挟まった瞬間に骨が途切れる。
	[[nodiscard]] int FindDisplayedParent(int nodeIndex, bool boneNodesOnly) const;

	/// ノード階層と offsetMatrix のずれを多数決で求め、m_bindCorrection に入れる。
	/// Build() の最後、Update() の前に 1 回だけ呼ぶ。
	void DetectBindCorrection();

	std::vector<NodeInfo>  m_nodes;
	std::vector<BoneInfo>  m_bones;
	std::vector<int>       m_roots;

	std::vector<Matrix4x4> m_bindLocal;		///< ファイル由来の初期ローカル行列（不変）
	std::vector<Matrix4x4> m_world;			///< 今のポーズのワールド行列（モデル空間）

	std::vector<Vector3>   m_rotation;		///< 回転差分【ラジアン】
	std::vector<Vector3>   m_translation;	///< 平行移動差分（親空間）

	Matrix4x4 m_bindCorrection = Matrix4x4::Identity;	///< ルートに掛ける補正
	bool      m_hasBindCorrection  = false;
	bool      m_applyBindCorrection = true;
	int       m_bindOutlierCount   = 0;
};
