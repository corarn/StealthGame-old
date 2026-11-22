@echo off
rem 元のモデル（.glb）から、ゲームで読む .gmdl を作り直す。
rem .gmdl は 90MB あって GitHub の上限（100MiB）に近いので git に入れない。代わりにこのバッチで作る。
rem 変換ツール（modelconverterDDSBC7.exe と assimp-vc145-mtd.dll）は鈴木先生のツール。
rem 先生が作ったものなので git に入れない。授業の GP23（PBR・BC7 版）からこのフォルダに写して使う。
rem --optimize は depot では部品が減らなかったので付けない（11/22 に試した。1082 個のまま）。
rem このファイルは Shift-JIS で保存する。UTF-8 だと cmd が日本語のコメントを命令と間違えて止まる（11/22）。

cd /d %~dp0
.\modelconverterDDSBC7.exe --bc7 depot\depot.glb
pause
