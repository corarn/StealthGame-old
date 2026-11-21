//==============================================================================
// ModelBinary.h
//
//  自前フォーマット .gmdl のシリアライズ / デシリアライズ。
//
//  ファイル構造
//    +----------------------------------------------------------------+
//    | Header  : magic[8]="GM31MDL\0", u32 version, u32 flags,         |
//    |           u32 chunkCount, u32 reserved                          |
//    +----------------------------------------------------------------+
//    | Chunk   : char id[4], u64 payloadBytes, u8 payload[...]         |
//    | Chunk   : ...                                                   |
//    +----------------------------------------------------------------+
//
//  チャンク方式にした理由は、**知らない id は読み飛ばせる**ようにするため。
//  後からチャンクを足しても古いローダーがそのまま動く（前方互換）。
//  逆に既存チャンクの中身を変える場合は kVersion を上げること。
//
//  文字列は全て STRT チャンクの文字列テーブルへ集約し、本体からは u32 の
//  インデックスで参照する。同じマテリアル名・ボーン名が何度も出るモデルで
//  ファイルサイズが素直に縮む。
//
//  想定はリトルエンディアン（x86/x64・ARM LE）。ビッグエンディアン環境では
//  コンパイル時に落ちるようにしてある。
//==============================================================================
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "ModelFormat.h"

namespace gm31::model {

/// 既定の拡張子
inline constexpr const char* kFileExtension = ".gmdl";

/// Model をバイト列へ直列化する。
[[nodiscard]] std::vector<std::uint8_t> Serialize(const Model& model);

/// バイト列から Model を復元する。失敗したら false を返し error に理由を入れる。
[[nodiscard]] bool Deserialize(Model&                    outModel,
                               const std::uint8_t*       data,
                               std::size_t               size,
                               std::string*              error = nullptr);

/// ファイルへ書き出す。
[[nodiscard]] bool WriteToFile(const Model&                 model,
                              const std::filesystem::path& path,
                              std::string*                 error = nullptr);

/// ファイルから読み込む。
[[nodiscard]] bool ReadFromFile(Model&                       outModel,
                               const std::filesystem::path& path,
                               std::string*                 error = nullptr);

/// 人間が読める要約を返す（コンバータの確認用）。
[[nodiscard]] std::string Describe(const Model& model);

} // namespace gm31::model
