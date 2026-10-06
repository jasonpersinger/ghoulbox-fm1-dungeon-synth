# BLE MIDI への経路 (方針メモと下書き)

結論から: **本線 (1.x) には入れない。入れるなら「BLE 版」の別ビルドで、失うものを明記して出す。**
数字の根拠は [flash-savings-research.md](flash-savings-research.md) 第 5 節 (SDK の demo_ble を実リンク:
Flash 280,576 B、静的 RAM 39 KB + ヒープ)。要望は Issue #17 (open、+1 と同種の声が 3 件)。

## 1. 何を失うか

| 失うもの | 理由 | 規模 |
| --- | --- | --- |
| SLICE エンジン | Flash: BLE 版の空きを作る (−32 KB、BREAK 込み) | エンジン 1 本 |
| USB シリアルコンソール | Flash −5.5 KB (デバッグ機能) | 開発者向け |
| ディレイの最大時間 | RAM: `dly_buf` 128 KB → 64 KB で +64 KB | 2.97 s → 1.49 s |
| GRAIN か SLICER のどちらか | RAM: `gr_p` 28 KB / `sl_buf` 32 KB。ディレイ半減で足りなければ | エンジン or インサート 1 つ |
| 工場サンプルの音質 | Flash: フェーズ 4 (16 kHz、ループ短縮) まで実施が前提 | PIANO/FLUTE/SAX |
| 音切れゼロの保証 | BLE の IRQ が音声 ISR の上で動く。接続中はレンダ時間が削られる | 実機で測るまで不明 |
| ベアメタルの純度 | FreeRTOS と SDK ランタイム (bitcode) を同梱。hal/ 限定のレジスタ検査もホストテストも届かない部分ができる | 設計 |

Flash の収支: フェーズ 1–4 後 260〜270 KB + SLICE/CDC 外し 37 KB ≈ 300 KB の空きに対し、BLE が 240〜260 KB。余裕 40〜60 KB。
RAM の収支: 現状の空き 23 KB + ディレイ半減 64 KB = 87 KB に対し、BLE が 60〜80 KB。GRAIN/SLICER を削れば +28〜32 KB。

## 2. ライセンス

- Felucca は GPL-3.0-only。SDK の `btctrler.a` / `btstack.a` / `wl_rf_common.a` / `cpu.a` / `system.a` はソース無しの
  LLVM bitcode。これを同梱した実行ファイルは、GPL の「対応するソース」を提供できないので、現状のライセンスのままでは配布できない。
- 取れる道は二つ。
  1. **別配布**: BLE 版の `.fwsc` を Felucca 本体とは別の成果物にし、GPL 部分のソースと、Apache-2.0 の SDK バイナリ (再配布可) を
     組み合わせた「集合物」として出す。リンクして一体化している以上、GPL 原理主義的にはグレー。
  2. **例外条項**: `LICENSE` に「JieLi AC79 SDK のライブラリとリンクして配布することを許す」追加許諾 (GPL §7 の追加的許可) を書く。
     全著作権者の同意が要る。外部コントリビュータは PR #2 (keremimo) のみなので、現時点なら現実的。
- どちらでも `LICENSING.md` の「No vendor material」「Radio (Felucca never enables the Bluetooth / Wi-Fi radio)」は書き換えになる。
- 電波: 無線を有効にするファームは、公式ファームが取った認証 (FCC/CE 等) の前提から外れる。配布時の注意書きが要る。

## 3. 経路 (やるなら、この順)

| 段階 | 内容 | ゲート |
| --- | --- | --- |
| 0 | フェーズ 1–3 (本線、BLE と無関係) | 済むまで BLE に手を付けない |
| 1 | **捨てプロトタイプ**: SDK の demo_ble に BLE MIDI サービス (GATT 1 本: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700 / 7772E5DB-3868-4112-A1A9-F2669D106BF3) と、音声 ISR 相当のダミー負荷 (2.9 ms 周期で 70 % 占有) を足し、接続中・通知送信中のレンダ遅延を測る | 遅延が ISR 周期の 10 % 以内なら続行、超えたら終了 |
| 2 | ライセンスの決定 (上の 1 か 2) と Issue #17 への方針回答 | 決まるまで実装しない |
| 3 | 本線にビルドフラグ `FELUCCA_BLE` (既定 0) と MIDI の入口だけ用意: `midi_event()` へ BLE 受信を流す口、送信は USB MIDI と同じ `midi_out_event()` の分岐。`FELUCCA_BLE=1` は `FELUCCA_SLICE=0 FELUCCA_CDC=0` を強制、`DLY_MAX` を半分に | 本線の挙動は不変 (テスト全通過) |
| 4 | BLE 版ビルド: SDK ランタイムのアダプタ (FreeRTOS を Felucca の main loop と共存させる: 音声 ISR と TIMER5 はそのまま、BT タスクは RTOS に)、VM 領域 (0x93000..0x97000) をボンディング保存に割当 | 実機でフェーズ 1 の測定を再現 |
| 5 | 配布: `felucca-ble-X.Y.fwsc` を別ファイルで、README に失うものを明記 | |

## 4. 下書き: README (英語、「Features」の後か「Support」の前に 1 段落)

```markdown
## Bluetooth MIDI

Felucca has no Bluetooth MIDI, and the 1.x releases will not get it. The stock firmware's Bluetooth
runs on the chip vendor's closed-source Bluetooth libraries. Felucca is free software (GPL-3.0-only)
and links no vendor code, so those libraries cannot simply go in; and they are big: measured on the
vendor's own BLE-only example, about 250 KB of flash and 60-80 KB of RAM on a part with 512 KB of RAM,
which Felucca already uses for its voices, effects and screen.

A separate "BLE edition" is being looked at. It would have to give things up: the SLICE engine, the
USB serial console, half of the delay's maximum time, possibly GRAIN or the SLICER, and it would carry
the vendor's Bluetooth binaries under their own licence. Whether it happens depends on one measurement
first: that Bluetooth traffic does not interrupt the audio. Progress is tracked in
[issue #17](https://github.com/hugelton/Felucca/issues/17).

Until then: USB MIDI works with phones, tablets and computers over a cable (class compliant, no driver),
and the TRS MIDI input takes a Bluetooth-MIDI-to-TRS adapter or a keyboard's MIDI out.
```

## 5. 下書き: Issue #17 への返信 (英語、投稿後にクローズ: state "not planned")

```markdown
Thanks everyone for the interest. Here is where Bluetooth MIDI stands, and why this issue is being
closed for now.

1. **Putting BLE in means losing other things.** The vendor's Bluetooth stack (controller, host and
   the RTOS it needs) takes about 250 KB of flash and 60-80 KB of RAM, measured by linking the vendor's
   own BLE-only example with Felucca's toolchain. To make that room, Felucca would have to drop the
   SLICE engine and the USB serial console, halve the delay's maximum time, and possibly lose GRAIN or
   the SLICER. This is not a software problem we can code our way around: the FM-1's flash chip is
   1 MB (and its RAM 512 KB), and Felucca already uses it for the thirteen engines, the samples, the
   effects and the screen.

2. **There is a licence problem with the SoC vendor's Bluetooth libraries.** They are closed binaries.
   Felucca is GPL-3.0 and links no vendor code, so they cannot go into the firmware as it is; a BLE
   edition would have to be a separate download with its own licence terms.

If people are prepared to give those things up, we will consider a separate BLE edition in the next
major update. Until then, the ways in are USB MIDI (phones, tablets and computers over a cable,
class compliant, no driver) and the TRS MIDI input (with a Bluetooth-MIDI-to-TRS adapter or a
keyboard's MIDI out).

Closing this for now; it will be reopened when that work starts.
```

## 6. やらないこと

- 本線 1.x に BLE を入れること。
- フェーズ 1 の測定なしに SDK ランタイムの組み込みを始めること。
- ライセンスの決定前に BLE 版をビルドして配ること。
