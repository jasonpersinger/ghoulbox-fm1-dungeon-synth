# Felucca Flash 節約 実施計画

前提の調査は [flash-savings-research.md](flash-savings-research.md)。本書は「安全に実施できる」ものを
順番と手順に落としたもの。決定事項: **パーカッションはサンプルを持たず合成のみ** (DRUM エンジン)。

基準値 (1.0.1, 20c275e): イメージ 476,996 B / スロット 581,564 B / 空き 104,568 B。

| フェーズ | 内容 | 見込み | 累計空き |
| --- | --- | ---: | ---: |
| 1A | SAMPLE PERC を廃止、パーカッションは DRUM 合成のみ | −67.8 KB | 172 KB |
| 1B | FLUTE / SAX を内蔵から外し、Web エディタの「サンプルパック」でユーザースロットへ (PIANO と BREAK は内蔵のまま) | −62.8 KB | 235 KB |
| 2 | 無損失データ圧縮と表の縮小、ビルド設定 | −17.6 KB | 253 KB |
| 3 | 見た目を確認して採る (フォント、キーキャップ) | −21.3 KB | 274 KB |
| 4 | 音の判断つき (任意): PIANO と BREAK の 16 kHz 化 | −17 KB | 291 KB |

内蔵サンプルは PIANO 41 KB + BREAK 22 KB = 63 KB になる (今 194 KB)。

進め方の原則:
- 1 ステップ = 1 PR。各 PR で `./build.sh` の `image ... B` 行と `tests/run_tests.sh` を通し、`tests/target_budget.py` の
  ループ命令数が増えていないことを確認する。CPU に触れないステップでは `target_budget.txt` は変わらないはず。
- 生成物 (フォント、アイコン、サンプル) の変更は `tools/gen_*.py` と、必要なら `gfx.c` の描画側だけで完結させ、
  フォーマットの番号 (エンジン番号、SET インデックス、プリセット番号、P_*/G_* の id) は一切動かさない。
- 音が変わるステップは `GOLDEN_UPDATE=1 sh tests/run_tests.sh` で golden を更新し、PR に「変わった render の一覧」を書く。

---

## フェーズ 1A: SAMPLE PERC の廃止 (−67,812 B + コード数百 B)

### 1.1 方針

- `SMP_DATA` から PERC セット (生成ドラム 11 音 46,276 B、CC0 手打楽器 5 音 21,536 B) を外す。
- **SET インデックス 4 は "PERC" の名前で残す (ゾーン 0 個)**。SET の列挙 (`SMP_ALL_NAMES`, `SMP_NALL` = 5 + USR 3)、
  USR1–3 の番号 (5, 6, 7)、プリセット番号は現状のまま。TRANH の「別名セット」と同じ考え方で、データだけ無い。
- 保存物の互換: SAMPLE で SET = PERC の音 (プロジェクト、ユーザプリセット、お気に入り、エディタの PRESET/SET/UP_PUT、
  FUN1–FUN5 のドラムトラック変換) は、読み込み時に **DRUM エンジン (KIT STD, 他は既定値, preset 0)** へ変換する。
  これは `drum_from_phys` (core.h) と `fm4_convert` が既にやっている「ロード時変換」と同じ場所に 1 本足すだけ。
- GM ノート → ドラム音の対応は DRUM 側に `DRUM_GM[47]` (35..81) として既にあり、コンガ・クラーベ・ベル・シンバルも含む。
  手打楽器 (tamb/shaker/wood) はハイハット/クラーベ系に割り当てられる。音色は変わる。
- SLICE の BREAK は gen_waves.py のドラム WAV から合成して焼くので **影響なし** (PERC セットのデータは使っていない)。

### 1.2 手順

1. `tools/gen_samples.py`
   - `main()`: `b.gm_kit(have_cc0)` の代わりに `b.sets.append(("PERC", len(b.zones), 0))` と `b.kinds["PERC"] = "kit"`。
     `SMP_NPRESETS` / `SMP_PERC_PRESET` の算出 (`perc = index of "PERC"`) はそのまま 4 になる。
   - `GM_KIT`, `GM_ROLE_WORDS`, `GM_KEEP`, `GM_FADE`, `GM_CC0_ROLES`, `gm_kit()` を削除。`gm_kit_sources(False)` と
     `gm_kit_entry()` は `break_loop()` が使うので残す (CC0 分岐は削除)。`CC0_SETS` から `("KIT", "kit")` を外す。
   - `header()`: ゾーン 0 個のセットでも `smp_set_t` が壊れないこと (`{"PERC", z0, 0}`) を確認。
   - キャッシュ鍵はスクリプト本体のハッシュに含まれるので自動で無効化される。
2. `firmware/src/eng_sample.c`
   - `smp_perc_set()` と `sample_keys()` を削除し、`ENG_SAMPLE.keys` を外す (GM 鍵盤は DRUM が持つ)。
   - `smp_user_scan` / ゾーン検証: `n == 0` のセットを「無音」として扱う箇所がないか確認 (`SMP_SETS[i].n` のループは 0 回で済む)。
3. `firmware/src/core.h`: `drum_from_phys` の隣に
   `static int drum_from_sample_perc(uint32_t engine, int16_t *e)`: `engine == 4 && (uint32_t)e[0] % SMP_NALL == SMP_PERC_PRESET`
   なら `e[]` を DRUM の既定値 (`ENG_DRUM.edit[k].def`、KIT 0 STD, KICK 0, DRV 0) にして 1 を返す。
   (`SMP_NALL`/`SMP_PERC_PRESET` は engines.c 以降で定義されるため、関数本体は `eng_drum.c` か `engines.c` に置き、core.h は宣言のみ。)
4. 変換の呼び出し (すべて `drum_from_phys` の直後に同形で):
   - `project.c` `proj_phys()` 相当のパス (全フォーマット、毎ロード、冪等) — `proj_drums_to_part()` は `d->engine = ENGI_DRUM; d->preset = 0`
     に変え、`proj_legacy_perc()` と `PROJ_DEF_KEEP` の PERC 分岐を削除。
   - `upreset.c` レコード読み込み (line 80 付近)。
   - `editor.c` `UP_PUT` / `PRESET` / `SET G_ENGSEL` の受信側、`favorites.c` のプリセット番号解決、`ui.c` の `TRK_DEF` 系。
5. `params.c enum_orig()` / `ui.c preset_orig()`: 「別名」判定を `SMP_SET_ORIG[v] != v || SMP_SETS[v].n == 0` に広げ、
   SET ノブと GRAIN の SRC (同じ `SMP_ALL_NAMES`) が PERC を飛ばすようにする。`SMP_NALIAS` の数え方も同じ条件に。
6. `eng_grain.c`: コメントの PERC 前提を直す。`GR_MAXZ 32 → 16` (ユーザスロット上限) にすれば pool RAM も僅かに減る (任意)。
7. テスト
   - `tests/hostsim.c` `host_legacy_sample_perc()` → DRUM KIT の既定値をセットする `host_drum_kit()` に (regress `job_drums`, perform_test, DRUMS=1, tracks_demo)。
   - `tests/chord_test.c` 312–316: SAMPLE PERC の kit 判定を DRUM で (`chord_kit` は `oneshot` で DRUM を kit と見る)。
   - `tests/digital_test.c` 45: `smp_perc_set()` 依存を外す。
   - `tests/project_test.c` 189–191: FUN2 → part 4 は `ENGI_DRUM` になる期待値に。
   - `GOLDEN_UPDATE=1`: 変わるのは `drums/*` (GM キット) と SAMPLE PERC を使っていた mix のみのはず。SAMPLE/PIANO..SAX の
     ハッシュは **変わってはならない** (データのオフセットが動かないため)。変わったら生成器の順序ミス。
8. 文書・付随物
   - README「drums are the DRUM engine or the SAMPLE engine's GM kit」→ DRUM のみ。`web/EDITOR_PROTOCOL.md` の 114/269/312/316/331 行。
   - `assets/samples-cc0/KIT/` と `ATTRIBUTION.txt`/`CREDITS.txt` の KIT 5 行、`tools/fetch_cc0.py` の "KIT" を削除
     (ライセンス表記は使っている素材だけにする)。
   - `BUILDING.md` の SAMPLE 説明 (「生成ドラムキット」の記述)。

### 1.3 確認

- `./build.sh`: `samples: 5 sets, 11 zones, ~104,200 B ADPCM` 程度、イメージ約 409 KB。
- `tests/run_tests.sh` 全通過 (golden 更新後)。`target_budget.py`: `sample_render` は不変、`drum_*` 不変。
- 実機: 旧プロジェクト (0.9 の FUN6/FUN7、1.0 の FUN8) を読み、ドラムトラックが DRUM で鳴る。SET ノブが PERC を飛ばす。
  Web エディタで SET を 4 に強制しても無音ではなく DRUM に変換されるか、少なくとも落ちない。

---

## フェーズ 1B: FLUTE / SAX をサンプルパックへ (−62,844 B)

### 方針

- 内蔵に残すのは PIANO (41,350 B、SET 0、別名の SET 1) と SLICE の BREAK (22,050 B、生成物)。FLUTE (SET 2) と SAX (SET 3) は
  `SMP_DATA` から外し、PERC と同じ「名前だけのゾーン 0 個のセット」として番号を残す。USR1–3 (5, 6, 7) は動かない。
- 素材は **サンプルパック**として Web エディタから入れる: ビルド時に `sampleio.user_slot` でスロット像 (`flute.fsmp`, `sax.fsmp`:
  ヘッダ 512 B + ゾーン表 + ADPCM、`web/EDITOR_PROTOCOL.md` の「User sample slot」そのもの) を作り、サイトに置く。エディタは
  既存の `SMP_BEGIN / SMP_WRITE / SMP_END` でスロットへ書く。CC0 の帰属表記 (`ATTRIBUTION.txt`) はパックに同梱。
- **退役セット → 同名スロット**: SAMPLE / GRAIN が SET 2 (FLUTE) を鳴らすとき、名前が "FLUTE" のユーザースロットがあれば
  それを鳴らす (スロットヘッダの名前で解決、`eng_sample.c` のゾーン解決に 1 段)。無ければ無音。これで旧プロジェクト・
  旧プリセット・FLUTE/SAX を SRC にした GRAIN のプリセットは、パックを入れれば今と同じ音で鳴る。保存値は書き換えない。
- SAMPLE の工場プリセットは PIANO のみ。FLUTE / SAX のプリセット行は残すが、同名スロットが無い間は PERC と同じ規則
  (ゾーン 0 個) でブラウズから隠す。GRAIN の工場プリセットで SRC が FLUTE / SAX のものは PIANO に差し替えるか、同名スロットがある
  ときだけ出す。

### 手順

1. `tools/gen_samples.py`: `CC0_SETS` の FLUTE / SAX を `"pack"` 種別にし、`SMP_DATA` には書かず `{"FLUTE", z0, 0}` の空セットを出す。
   同じ `cc0_entries()` の結果から `sampleio.user_slot()` で `build/pack/flute.fsmp`, `sax.fsmp` を書く (`tools/gen_pack.py` に分けてもよい)。
2. `eng_sample.c`: ゾーン解決に「組み込みセット n == 0 なら、同名の USR スロット (usr_nz > 0、ヘッダの名前一致) を使う」を足す。
   `eng_grain.c` の `gr_find` / `gr_stamp` も同じ解決を通す (スロットのアップロードで索引が追従する仕組みは既にある)。
3. `web/editor.html`: サンプルのパネルに「Factory pack: FLUTE → USR2, SAX → USR3」(スロットは選択可、上書き確認つき) を追加。
   `web/make_site.py` が `build/pack/*.fsmp` と `ATTRIBUTION.txt` をサイトへコピー。インストーラの最後に「サンプルパックも入れる」
   の 1 ステップ (任意) を置く。
4. UI: パック未導入で FLUTE / SAX 参照の音を選んだときの表示 (`NO PACK`、ui_draw.c のプリセット名の横)。
5. テスト: golden から `preset/SAMPLE/02_FLUTE`, `03_SAX` と GRAIN の該当行が消える (PIANO の行は不変)。新規: スロット像を
   RAM 像に載せて (`SMP_USER_XIP` のホスト経路) 名前解決で FLUTE が鳴ること、スロットが無いときに無音で落ちないこと。
   `tests/install_test.py` 相当でパックの SysEx 書き込みを一巡。
6. 文書: README (SAMPLE: PIANO built in, FLUTE / SAX and your own sounds from the editor)、`EDITOR_PROTOCOL.md`、`LICENSING.md`
   (CC0 はパック側)、`BUILDING.md` の Samples 節。

### 確認

- `./build.sh`: `samples: 5 sets, 6 zones, ~63,400 B ADPCM`、イメージ約 346 KB、空き約 235 KB。
- 実機: パック導入前は PIANO と BREAK が鳴り、FLUTE/SAX は無音で表示が出る。導入後は 0.9/1.0 のプロジェクトの FLUTE/SAX が同じ音で鳴る。
- 自分のサンプルで USR2 を上書きしたら FLUTE は再び無音になり、パック再導入で戻る。

---

## フェーズ 2: 無損失の縮小 (−17.6 KB、音も見た目も不変)

| # | 変更 | 節約 | ファイル |
| --- | --- | ---: | --- |
| 2.1 | アイコンセルを Huffman 符号化 (フォント M/L と同じ `huff_pack`、既存の `cv_alpha_hc` で描く) | −10,554 | `tools/gen_aa_icons.py`, `gfx.c` のアイコン blit, `tools/aa_raster.py` |
| 2.2 | 未使用アイコン 21 個を EXTRA から外す (`X_TRK2..4(_O)`, `X_UNDO/REDO`, `X_LOCK`, `X_PLUS/MINUS`, `X_UP/DOWN`, `X_CHECK`, `X_DOC`, `X_FX`, `X_HUGELTON`, `X_PAUSE`, `X_PLAY_O`, `X_POWER`, `TAPE`) | −1,512 | `tools/gen_aa_icons.py` (enum は `assets/icons.json` の順を保つ: EXTRA のみ削る) |
| 2.3 | `FM6_EXP2` 1025 → 257 点、`fm6_core.c` の添字を `f >> 16`、補間の分母も 16 bit に | −3,072 | `tools/gen_tables.py`, `fm6_core.c` 51–52 |
| 2.4 | `size_fns.py SIZE_FILES` に非 DSP ファイルを追加 (usb, midi_uart, midi_control, midi_clock, seq, song_chain, chord, motion, panel, lcd, gfx, ota, ota_hw, fm6_bank, slice_store, params, editor_fm6, ui_layer, ui_name, ui_slice, main, libc, storage_hw, engines, fm4_convert) | −3,008 | `tools/size_fns.py` |

手順と確認:
- 2.1: `gen_aa_icons.py` の出力に `AI<S>_HC` (符号表) と各セルのバイトオフセット表を追加。`icons.c`/`ui_draw.c` の呼び出しが
  `cv_alpha` → `cv_alpha_hc` に変わるだけ。`tests/ui_render.c` の全画面 PNG が **ピクセル単位で同一** であることを確認
  (無損失なので差分ゼロが合格条件)。描画コストは `ui_render` の draw cost 表で確認。
- 2.3: 誤差は最大 9.2e-7 (−121 dB)。FM6 の golden は変わる (丸め) ので `GOLDEN_UPDATE=1`。`tests/fm6_test.c` の許容内。
  `target_budget.py`: `fm6_op_run` 不変。
- 2.4: `FELUCCA_SIZE` の既存機構なので失敗しても環境変数 `FELUCCA_SIZE=0` で戻せる。`fm1_timer5_irq` の予算行が
  10 % 以内 (実測では不変) であることを確認。

---

## フェーズ 3: 見た目を確認して採る (−21.3 KB)

| # | 変更 | 節約 | 確認 |
| --- | --- | ---: | --- |
| 3.1 | フォントの水平フェーズ `PHASES, PHASES_L = 2, 1` | −16,945 | `build/ui_new/*.png` を 4 フェーズ版と並べて見る。`tests/text_spacing_test.py` の S/M 閾値 0.25 → 0.5 px |
| 3.2 | キーキャップ (`KC_DATA`) を `cv_rrect` + S フォントで実行時描画、ノブ弧 (584 B) は残す | −4,393 | フッタの PNG 比較。レイアウト lint (はみ出し/重なり) 通過 |

3.1 の代替: フェーズ 1 (`1, 1`) なら −24,317 だが、字間の揺れが最大 0.5 px になるので、まず 2 で止める。

---

## フェーズ 4 (任意、音の判断): 内蔵サンプルの縮小

| # | 変更 | 節約 (算出) | 備考 |
| --- | --- | ---: | --- |
| 4.1 | PIANO を 16 kHz で格納 (ゾーンの `rate` は既存。`eng_sample.c` 無変更) | −約 11,000 | 高域 8 kHz 以上が落ちる |
| 4.2 | BREAK を 16 kHz | −約 6,000 | ハイハットが鈍る。優先度低 |
| 4.3 | PIANO を 5 → 3 ゾーン | −16,540 | ピッチシフト幅 ±4 → ±6 半音 |

FLUTE / SAX はパック側なので、ループ短縮や 16 kHz 化はパックの作り方の問題になり、本体の Flash には関係しない。

ADPCM のビット数削減 (3 bit) は SAX で SNR 15 dB まで落ちるため **採らない**。zlib/LZMA 系はボイス毎のランダムアクセスと
RAM (空き 9 KB) の都合で実機では使えない (research 2.B 参照)。

---

## 採らないもの

- `-Oz` 全体 / `-fno-inline-functions` / `-mllvm -inline-threshold`: 節約の大半が DSP のインライン展開で、per-sample の呼び出しが増える。
- `FELUCCA_OTA=0`: エディタも消える。`FELUCCA_UAC=0` / `UART=0`: 機能に対して小さい。
- Flash 配置の変更 (スロット拡張 +16 KB): インストーラ/ローダ/テストに波及。フェーズ 1–3 で足りるので保留。
- 旧形式 (FUN1–FUN5) や DIGITAL 変換の削除: 互換性を失うのに −3〜5 KB。保留。

## 進捗の記録

| フェーズ | PR | イメージ | 空き | 備考 |
| --- | --- | ---: | ---: | --- |
| 0 (基準) | — | 476,996 | 104,568 | 1.0.1 |
| 1A | | | | |
| 1B | | | | |
| 2 | | | | |
| 3 | | | | |
