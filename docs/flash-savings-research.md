# Felucca Flash 節約リサーチ (1.0.1 時点)

対象: `build/felucca.bin` (アプリスロット, XIP) と 1 MiB SPI NOR 全体の配置。
計測は本リポジトリ 20c275e を JieLi toolchain (clang 4.0.1, 20250324.1) で実際にビルドして取得した
(`tools/build.py` の `build_app()` と同じ手順。SDK は不要)。数値はすべてバイト。

## 1. 現状

| 項目 | 値 |
| --- | --- |
| アプリイメージ (.text+.rodata+.ram_text+.data) | 476,996 |
| アプリスロット `APP_SLOT` (`app.ld` XIP LENGTH 0x8DFBC) | 581,564 |
| 空き | 104,568 (18 %) |
| RAM .data+.bss | 88,884 / 98,304 (空き 9.4 KiB) |
| POOL | 329,952 / 344,064 (空き 13.8 KiB) |

RAM も残り 1 割なので「テーブルを RAM に移す」「起動時に生成して RAM に置く」系の案は使えない。
Flash を減らす手段は (a) 定数データを減らす/圧縮する, (b) コードを減らす, (c) 配置を変える の三つ。

### 1.1 イメージの内訳 (シンボル単位、objdump -t)

| 区分 | バイト | 割合 | 主なシンボル |
| --- | ---: | ---: | --- |
| サンプル (IMA ADPCM) | 194,056 | 40.7 % | `SMP_DATA` |
| コード (.text 557 関数) | 171,604 | 36.0 % | `draw_graph` 12,284 / `fm1_main` 9,050 / `ui_draw` 8,858 / `ed_handle` 8,594 / `ui_input` 7,814 / `fm1_alnk0_irq` 6,056 |
| フォント (Inter Tight S/M/L) | 41,883 | 8.8 % | `AF_M_DATA` 13,030 / `AF_S_DATA` 10,410 / `AF_L_DATA` 4,864 / `AF_*_G` 7,296 / `AF_*_KERN+KD` 6,135 |
| アイコン (Fukiai 12/16/24 px) | 19,995 | 4.2 % | `AI12_DATA` 9,000 / `AI16_DATA` 6,528 / `AI24_DATA` 4,032 |
| DSP テーブル (`felucca_tables.h`) | 11,816 | 2.5 % | `FM6_EXP2` 4,100 / `SINE` 2,050 / `PITCH_OCT` 768 / ほか 256–514 の表 ×12 |
| 文字列リテラル | 7,951 | 1.7 % | ラベル、メニュー、コンソール |
| キーキャップ・ノブ弧 | 5,185 | 1.1 % | `KC_DATA` 4,393 |
| パラメータ記述子 | 3,396 | 0.7 % | `param_desc_t` 配列 (`ENG_*` 220 ×14, `TP`, `GP`) |
| その他 (プリセット、FM6 工場パッチ、パレット…) | ~21,000 | 4.4 % | `FM6_FACTORY` 1,024 / `PATTERNS` 468 / `PAGES` 444 … |

サンプルの内訳 (`SMP_ZONES` のオフセットから):

| セット | バイト | 内容 |
| --- | ---: | --- |
| PIANO | 41,350 | 5 ゾーン × 0.75 s, ループ無し (ワンショット) |
| FLUTE | 31,422 | 3 ゾーン × 0.95 s: アタック 0.40 s + ループ区間 0.55 s |
| SAX | 31,422 | 同上 |
| PERC (生成ドラム) | 46,276 | `gen_waves.py` 合成の 11 音 (kick/snare/hats/clap/toms/rim/cowbell/crash/ride) |
| PERC (CC0 手打楽器) | 21,536 | tamb/shaker/conga/claves/wood |
| SLICE BREAK | 22,050 | 2 s @ 22.05 kHz。PERC の生成ドラムを 16 分で並べたもの |

コードのソースファイル別内訳 (デバッグ情報で関数→ファイルを対応付け、インライン後の最終サイズ):

| ファイル | コード | 備考 |
| --- | ---: | --- |
| ui_draw.c | 16,885 | カード/カラム描画 |
| main.c | 14,356 | `fm1_main` にコンソール等が単一呼び出しでインライン |
| ui_graph.c | 14,102 | グラフ 23 種が `draw_graph` 1 関数に |
| editor.c | 10,644 | SysEx エディタ (`ed_handle`) |
| ui_input.c | 10,418 | |
| project.c | 6,422 | うち FUN1–FUN6 旧形式の読み込み `proj_import` 1,304 |
| phys_dsp.c | 6,268 | |
| audio.c | 6,108 | オーディオ ISR (per-block 関数が全部インライン) |
| ui.c | 6,068 | |
| eng_fm6.c + fm6_core.c | 5,314 | |
| fm4_convert.c | 2,374 (+データ 492) | 退役 DIGITAL エンジンの変換 |

## 2. 提案 (効果 × リスク順)

### A. データ側 (CPU コストなし、効果大)

| # | 案 | 節約 (実測/算出) | 変更箇所 | リスク・検証 |
| --- | --- | ---: | --- | --- |
| A1 | **アイコンを Huffman 符号化** (フォント M/L と同じ `huff_pack`; 描画は既存の `cv_alpha_hc`) | **−10,554** (19,560→9,006, 実測) | `gen_aa_icons.py`, `gfx.c` のアイコン blit を `cv_alpha_hc` 経由に | 描画速度微減 (アイコンは小さい)。UI レンダテストで比較 |
| A2 | **フォントの水平フェーズを 4→2 (L 2→1)** | **−16,945** (41,775→24,830, 実測) | `gen_aa_font.py` `PHASES, PHASES_L = 2, 1` | 配置精度 1/4 px → 1/2 px。`text_spacing_test.py` の閾値 0.25 px を 0.5 px に緩める必要。見た目は要確認 |
| A2' | フェーズ 1 (サブピクセル無し) | −24,317 (→17,458, 実測) | 同上 `1, 1` | カーニングはそのまま。文字間の揺れが最大 0.5 px |
| A3 | **`FM6_EXP2` を 1025→257 点** (補間は現状のまま `f>>16`) | **−3,072** | `gen_tables.py`, `fm6_core.c` `k = f >> 16` | 最大相対誤差 9.2e-7 (−121 dB)。golden ハッシュは変わる (FM6 のみ) |
| A4 | 未使用アイコン 21 個を生成から外す (`X_TRK2..4`, `X_UNDO/REDO`, `X_LOCK`, `TAPE` …) | −1,512 (12 px) | `gen_aa_icons.py` EXTRA | なし |
| A5 | キーキャップ `KC_DATA` を実行時描画 (`cv_rrect` + S フォント) | −4,393 | `gfx.c cv_keycap`, `gen_aa_keycaps.py` | 描画コスト増 (フッタのみ)。見た目をレンダテストで比較 |
| A6 | `SINE` を 1/4 波 257 点に | −1,536 | `gen_tables.py`, 参照箇所 (FM6/drum/dsp) | FM6 の per-sample に折り返し数命令。`target_budget` で確認 |
| A7 | カーニング対の下限 `--kern-min 4` | −1,227 | `build.py` 引数 | 字間が 1/16–3/16 px 変わる。`text_spacing_test` で確認 |

### B. サンプル (最大の塊 194 KB)

| # | 案 | 節約 (算出) | 備考 |
| --- | --- | ---: | --- |
| B1 | **PERC の生成ドラム 11 音を削除し、GM ノートは DRUM エンジンで鳴らす** | **−46,276** | `eng_drum.c` は既に GM 35..81 を DRUM 音で再生する (`DRUM_GM`)。SAMPLE PERC は CC0 手打楽器のみ残し、生成ドラムの GM 番号は DRUM 合成へ転送 (`eng_sample.c` note_on で分岐)。音は変わる → golden 更新 |
| B2 | **FLUTE/SAX のループ区間を短縮** (0.55 s → ~0.15 s, クロスフェードループ) | −約 25,000 | `gen_samples.py cc0_entries` でループ長を切り詰め、`sampleio` でクロスフェード。ゾーン形式は変更なし |
| B3 | **PIANO/FLUTE/SAX を 16 kHz で格納** (ゾーン毎 `rate` は既にフォーマットにある) | −約 28,500 (104 KB × 27 %) | `gen_samples.py` のセット別 `TR`。`eng_sample.c` は `z->rate` を使うので変更不要。高域が落ちる |
| B4 | PIANO を 5→3 ゾーン | −16,540 | ピッチシフト幅 ±4→±6 半音 |
| B5 | SLICE BREAK を PERC/DRUM から実行時合成 | −22,050 | 現状 BREAK は PERC 生成ドラムの並べ替えなので情報は重複。ただし SLICE は 1 本の ADPCM バッファ + デコーダ状態表を前提にしており、仮想ソース化は中規模改修。代替: BREAK を 16 kHz 化 (−6,000) |
| B6 | 工場サンプルの一部 (PIANO 等) を Web エディタの「サンプルパック」として USR スロットへ任意インストール | −41,350〜−104,000 | 製品判断。USR1–3 (80 KiB×3) に空きがあれば既存の `SMP_WRITE` でそのまま載る |

B1+B2+B3 で約 −100 KB (スロットの空きが倍になる)。

### C. コード側

実測したコンパイラ/属性の組み合わせ (`FELUCCA_SIZE` の minsize 方式を基準):

| 設定 | イメージ | 差 | 判定 |
| --- | ---: | ---: | --- |
| 現状 (-Os + main-loop 関数 minsize) | 476,996 | 0 | |
| -Os のみ (`FELUCCA_SIZE=0`) | 485,064 | +8,068 | minsize は 8 KB 効いている |
| **minsize の対象を非 DSP ファイル全部に拡張** (usb/midi/seq/chord/motion/panel/lcd/gfx/ota/params/main/engines/fm4_convert …) | 473,988 | **−3,008** | `size_fns.py SIZE_FILES` の追加だけ。`target_budget` 変化なし → **採用推奨** |
| 同上 + noinline | 474,788 | −2,208 | minsize の小関数インラインは縮む方向なので noinline は逆効果 |
| -Oz 全体 | 461,728 | −15,268 | per-sample ヘルパがコールになり CPU 増 (`slicer_track` 予算 +20 % 超過)。実機計測なしでは不可 |
| -fno-inline-functions | 464,272 | −12,724 | 節約の 2/3 が eng_*/audio/perform (DSP 側)。同様に CPU リスク |
| -mllvm -inline-threshold=0 | 472,104 | −4,892 | 同上 |
| -fdata-sections + --gc-sections / -fmerge-all-constants / -fno-jump-tables / -fshort-enums / -fomit-frame-pointer | 476,996 | 0 | 単一 TU なので効果なし |

結論: フラグだけでの安全な削減は C1 (−3 KB) まで。それ以上は CPU との交換になる。
`-fno-inline-functions` の内訳 (main.c −12.5 KB, ui_draw.c −6.3 KB, audio.c −5.3 KB, editor.c −4.0 KB …) は
「単一呼び出しの static 関数は無条件にインライン」される clang の挙動で巨大関数ができ、
長距離分岐 (6 バイト命令 3,219 個) とスピルが増えていることを示す。ただし main-loop 関数に IR で
`noinline` を付けても −576 しか縮まなかったので、巨大関数の分割自体よりも DSP のインライン方針が支配的。

| # | 案 | 節約 | 備考 |
| --- | --- | ---: | --- |
| C1 | `SIZE_FILES` を非 DSP 全ファイルに拡張 | −3,008 (実測) | 上表 |
| C2 | オーディオ ISR / 各 render の **per-block 関数** (`track_render`, `events_block`, `dv_setup`, `grain_block`…) に `__attribute__((noinline))` | 推定 −3,000〜−6,000 | ブロックごとに 1 回なので CPU 影響なし。per-sample ヘルパ (`fm6_op_run`, `dv_*_run`, `pf_svf`) は触らない。`target_budget.py` が番人 |
| C3 | `FELUCCA_CDC=0` (USB シリアルコンソール) | −5,524 (実測) | デバッグ機能。リリースビルドのみ 0 にする選択肢 |
| C4 | 退役 DIGITAL の変換 (`fm4_convert.c` + `DIGITAL_PRESETS` + `ENG_FM4_GONE`) を外す | −約 3,300 | 1.0 以前の DIGITAL 音が FM6 に変換されなくなる (互換性) |
| C5 | FUN1–FUN5 の旧プロジェクト読み込みを外す (FUN6+ のみ) | −約 1,500〜2,000 | 0.9 以前のプロジェクト非互換 |
| C6 | `FELUCCA_ICONS=0` (パラメータ毎のアイコン) | −2,308 (実測) | 見た目の後退。A1 の方が得 |
| C7 | `param_desc_t` の `min/max/def` を int8 化・`label/unit` を文字列表インデックス化 | −約 1,500 | `TP` 1,820 / `ENG_*` 3,080 の約半分 |
| — | `FELUCCA_UAC=0` / `FELUCCA_UART=0` | −1,820 / −456 | 機能に対して小さい。非推奨 |
| — | `FELUCCA_OTA=0` | −14,924 | エディタも消えるので不可 |
| — | `FELUCCA_SLICE=0` | −10,068 | BREAK 22 KB は `SMP_DATA` に残る (gen_samples.py が無条件に書く)。SLICE=0 ビルドなら BREAK も外すべき (−22,050 追加) |

### D. Flash 配置 (1 MiB)

| 範囲 | 用途 |
| --- | --- |
| 0x00000–0x04000 | ヘッダ, SDK SPL (16 KiB) |
| 0x04000–0x93000 | アプリ領域: `app_area_head` + **app slot 581,564** + `cfg_tool.bin` + `eq_cfg_hw.bin` |
| 0x93000–0x97000 | **未使用 16 KiB** (SDK の VM 記述子は 0x93000 から。Felucca の `FL_DATA_LO` は 0x97000。ローダは [0x93000,0xFC000) を更新レコードの掃除に走査するだけ) |
| 0x97000–0x9F000 | プロジェクト 4 × (A/B 4 KiB) |
| 0x9F000 | FM6 バンク A |
| 0xA0000–0xDC000 | ユーザサンプル 3 × 80 KiB |
| 0xDC000–0xE0000 | ユーザプリセット 2 × (A/B) |
| 0xE0000–0xE5000 | OTA ローダ staging (20 KiB) |
| 0xE5000–0xE9000 | **未使用 16 KiB** |
| 0xE9000–0xEA000 | SDK BTIF |
| 0xEA000–0xFC000 | SDK **USR 72 KiB — Felucca は使っていない** |
| 0xFC000–0xFE000 | 設定 A/B |
| 0xFE000 | FM6 バンク B |
| 0xFF000 | SDK key_mac |

| # | 案 | 効果 | 備考 |
| --- | --- | ---: | --- |
| D1 | 0x93000–0x97000 をアプリスロットに取り込む (`APP_SLOT`, `app.ld` XIP LENGTH, `fm1pkg_make.py`, ローダ `LDR_APP_HI`, `fl_plain_window_init` の 0x93000 境界, `console.c flr`) | +16,384 | 配置変更はインストーラ/ローダ/テスト (`ldr_test.c`, `storage_test.c`) に波及。最後の手段 |
| D2 | 0xEA000–0xFC000 (72 KiB) を「工場サンプル領域」にし、生成可能な素材 (BREAK, 生成ドラム) を初回起動時に合成して書く | アプリから −68,000 | 公式ファーム復帰時は全体が書き戻されるので無害。ただし B1/B5 のように「そもそも実行時合成」の方が単純 |
| D3 | 同 72 KiB を USR4 (第 4 ユーザサンプルスロット) に | ユーザ容量 +72 KiB | Flash 不足が「ユーザサンプル側」の話ならこれが直接解 |

## 3. 推奨ロードマップ

1. **即効・無リスク (約 −18 KB)**: A1 アイコン Huffman (−10.5 KB), A3 FM6_EXP2 (−3.1 KB), A4 未使用アイコン (−1.5 KB), C1 minsize 拡張 (−3.0 KB)。
   いずれも生成スクリプトかビルド設定の変更で、`tests/run_tests.sh` の UI レンダ/golden で確認できる。
2. **見た目を確認して採る (約 −17〜−22 KB)**: A2 フォントフェーズ 2 (−16.9 KB), A5 キーキャップ実行時描画 (−4.4 KB)。
   `build/ui_new/` の PNG で比較。`text_spacing_test.py` の閾値更新が必要。
3. **音の設計判断 (約 −70〜−100 KB)**: B1 PERC 生成ドラム→DRUM 合成 (−46 KB), B2 ループ短縮 (−25 KB), B3 16 kHz 化 (−28 KB)。
   golden を `GOLDEN_UPDATE=1` で更新。B1 は SAMPLE PERC の音が DRUM エンジンの音になる。
4. **互換性と交換 (約 −5 KB)**: C4 DIGITAL 変換、C5 旧 FUN 形式。
5. **やらない方がよい**: -Oz / -fno-inline-functions 全体適用 (CPU)、`FELUCCA_OTA=0`。

1〜3 を全部やると約 −105〜−140 KB で、アプリの空きは 104 KB → 210〜245 KB (スロットの 36〜42 %)。

## 4. 計測の再現

```
# toolchain だけで app をビルド (SDK 不要)
python3 - <<'PY'
import sys, os; sys.path.insert(0, "tools"); os.environ["JIELI_TOOLCHAIN"] = os.path.expanduser("~/.jieli/toolchain")
import build; build.OUT.mkdir(exist_ok=True); build.generate()
img, syms, dis, rt = build.build_app(); print(len(img), build.APP_SLOT - len(img))
PY
~/.jieli/toolchain/common/bin/objdump -t build/felucca.elf     # シンボルサイズ
~/.jieli/toolchain/common/bin/objdump -h build/felucca.o       # -ffunction-sections 単位
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt   # DSP ループ命令数
```

ファイル別の内訳は `clang -g -S -emit-llvm -Xclang -disable-llvm-optzns` の `!DISubprogram` で関数→ファイルを引き、
objdump -t のサイズを合計した。
