PC-6001mkII Emulator for Raspberry Pi Pico
---
![Boot Menu](/pictures/screenshot01.jpg)
NEC PC-6001mkII のエミュレータです。
以下の機能を実装しています。

- メイン RAM(64KB)
- テープ
- PSG / FM 音源
- FD (6601)
- 一部の拡張カートリッジ(非SR のみ)

とっても重要なことですが、**音声合成はサポートしていません**。
せっかくの PC-6001mkII ですが**しゃべりませんし歌いません**。
talk 文はエラーにはなりませんが、何も起きません。

---
# 配線

Pico と VGA コネクタやブザー/スピーカーなどを以下のように配線します。

- GPIO0 VGA:H-SYNC
- GPIO1 VGA:V-SYNC
- GPIO2 VGA:Blue0 (330 Ohm)
- GPIO3 VGA:Blue1 (680 Ohm)
- GPIO4 VGA:Red0 (330 Ohm)
- GPIO5 VGA:Red1 (680 Ohm)
- GPIO6 VGA:Green0 (330 Ohm)
- GPIO7 VGA:Green1 (680 Ohm)
- GPIO10 Audio

VGA の色信号は以下のように接続します(三色とも)

```
Blue0 --- 330 Ohm resister ---+
                              |
                              |
Blue1 --- 680 Ohm resister ---+---> VGA Blue
```

このほかに VGA、Audio の　GND に Pico の　GND を接続してください。
I2S DAC を使う場合には FM音源の項を参照してください。

---
# 設定

おもな設定は以下の通りです。

- 機種選択
- 音源選択
- 拡張ROM

## 機種選択

コンパイルの際には以下の行の内の一つをコメントアウトしてください。

```
#define MACHINE_PC6001MK2
//#define MACHINE_PC6001MK2SR
//#define MACHINE_PC6601
//#define MACHINE_PC6601SR
```

PC-6601/SR を選択するとフロッピードライブが有効になります。

## 音源選択

デフォルトでは、オリジナルの PSG 音源を用いて PWM で音が出力されます。
`#define USE_FMGEN` を有効にすると、PSG および YM-2203 のエミュレーションに fmgen を使用します。
fmgen 使用時は `#define USE_I2S` も有効にすると、I2S DAC を経由して音を出力します。(PCM5102A でテスト済)
I2S DAC との接続は以下のようになります。DAC 側の設定については使用する DAC の説明を参照してください。

- GPIO14 DATA
- GPIO15 BCLK
- GPIO16 LRCLK

なお、fmgen の PWM 出力は「とりあえず音が出る」レベルだと思ってください。
PWM 出力で FM 音源不要であれば fmgen をオフにした方がいいと思います。

## 拡張ROM

`#define USE_EXT_ROM` を有効にすると拡張 ROM (最大16KB) が有効になります。
また同時に `#define USE_SENSHI_CART` を有効にすると 戦士のカートリッジ & RAM を使用するモードになります。
メモリ容量の関係で SR では使えません。
拡張ROM のデータは `p6mk2extrom.h` の中に入れてください。

内藤さんの[XeGrader 体験版](https://codeknowledge.livedoor.blog/archives/29983607.html)の動作を確認しています。

拡張ROM の有効無効は、メニューの Reset/Power Cycle で切り替えできますので、Reset すれば Basic Menu が表示されます。
(拡張 RAM は有効のままです)

---
# ROM など

著作権の関係で ROM は含まれていません。

PC-6001mk2/6601 として使う場合は、
`p6mk2rom_dummy.h` を `p6mk2rom.h` にコピーしたのち、
以下のファイルのデータを入れてください。

- BASICROM.62 または BASICROM.66
- VOICEROM.62 または VOICEROM.66
- CGROM60.62  または CGROM60.66
- CGROM60m.62 または CGROM66.66
- KANJIROM.62 または KANJIROM.66

なお実機の ROM がなくても、以下の互換 ROM ＆ フォントで BASIC まで起動することを確認しています。

- [PC-6001mkII/6601用互換BASIC](http://000.la.coocan.jp/p6/basic66.html)
- [PC-6001 用互換 CGROM](http://eighttails.seesaa.net/article/305067428.html)

PC-6001mk2SR/6601SR として使う場合には、
この場合、`p6srrom.h` に以下のデータが必要です

- SYSTEMROM1.64 または SYSTEMROM1.68
- SYSTEMROM2.64 または SYSTEMROM2.68
- CGROM68.64 または CGROM68.68

なお、PC-6001mk2SR と PC-6601SR は内部の機種判別フラグで切り替えているだけで、ROM の中身は同じです。

---
# コンバイル済みバイナリ

Pico SDK がわからなくてビルドできないという声をいただきましたので、コンパイル済みバイナリを `prebuild` ディレクトリ以下に置きました。

- p6mk2emulator.uf2                 PC-6001mk2 用(PWM 出力)
- p6mk2emulator_extrom.uf2          PC-6001mk2 用(戦士カートリッジ有効/PWM 出力)
- pc5001mk2sr.uf2               PC-6001mk2SR 用(I2S DAC 出力)
- pc6001mk2sr_pwm.uf2           PC-6001mk2SR 用(PWM 出力)
- pc6001mk2sr_pwm2.uf2          PC-6001mk2SR 用(fmgen/PWM 出力)

各 uf2 を、Pico に書き込むのと合わせて、ROM ファイルを Pico に置きます。

picotool を使う場合は、以下の通りで行けると思います。
(picotool は pico-sdk に含まれています)

```
PC-6001mk2 の場合
$ picotool load -v -x BASICROM.62 -t bin -o 0x10060000
$ picotool load -v -x VOICEROM.62 -t bin -o 0x10070000
$ picotool load -v -x CGROM60.62  -t bin -o 0x10074000
$ picotool load -v -x CGROM60m.62 -t bin -o 0x10078000
$ picotool load -v -x KANJIROM.62 -t bin -o 0x10068000
(拡張ROM)
$ picotool load -v -x extrom.bin -t bin -o 0x10040000

PC-6001mk2SR の場合
$ picotool load -v -x SYSTEMROM1.64 -t bin -o 0x10058000
$ picotool load -v -x SYSTEMROM2.64 -t bin -o 0x10068000
$ picotool load -v -x CGROM68.64    -t bin -o 0x10078000
```

(6601の場合には該当するファイル名に読み替えてください)

---
# キーボード

Pico の USB 端子に、OTG ケーブルなどを介して USB キーボードを接続します。
USB キーボードに存在しないキーは以下のように割り当てています。

- STOP → Pause/Break
- MODE → ScrollLock
- PAGE → PageUp
- カナ → カタカナ・ひらがな
- GRAPH → ALT

また F12 でメニュー画面に移ります。
テープイメージの操作ができます。

---
# VGA

今回は 16 色対応になっているので、1 pixel 1 バイト使用しています。
一応 64 色出ることになっているので、初代 PC-6001 の色をエミュレートするモードもあります。

mk2 の実装を残したまま SR 対応を行ったため、解像度切り替え時に PIO を再設定します。
SR モードで 40 桁(screen2) と 80桁(screen3) を切り替えると少し時間がかかります。

全画面の書き換えに時間がかかるために、高速に画面を切り替えるタイプのソフトでは動作が遅くなります。
`#define USE_REDRAW_CORE1` を有効にすると、画面の書き換え処理を省略して CPU の動作を優先します。

---
# テープ

いつもと違って、UART 入出力に対応していません。
LittleFS 上の CAS / P6 形式のファイルをロード・セーブに用います。

デフォルトでは、csave/cload の終了後にファイルクローズを行いません。
これは、BASIC とマシン語を一つの CASファイルに入れている場合に対応しています。

また、テープ読み込みの高速化機能を持っています。

メニュー画面の Load の下にある `C:  S:` がそれぞれの状態を示しています。

- C: 自動ファイルクローズ
- S: テープ読み込み高速化

デフォルトではどちらもオフです。

LittleFS の扱い方については、
[こちらの記事を参照](https://shippoiincho.github.io/posts/39/)してください。

---
# FD

PC-6601/SR の内蔵フロッピードライブ一台をエミュレートします。

D88 形式のイメージファイルの読み込みに対応しています。
テープイメージと同様に LittleFS 上に書き込んでください。

超手抜き実装なので、直接 FDC を操作するようなソフトは動作しないと思います。

---
# 制限事項

- 最初に起動する際に LittleFS のフォーマットで固まることがあります。(リセットでOK)
- Pico SDK 2.0 ではうまく動かないかもしれません。(2.1 以降推奨)
- 現状 Pico2 では動作不安定です。SDK 2.2 で直る予定です。
- 音声合成割り込みが未実装ですので、SR モードで talk 文を使うと固まるかもしれません。
- SR での動作については検証が十分です

---
# ライセンスなど

このエミュレータは以下のライブラリを使用しています。

- [Z80](https://github.com/redcode/Z80/tree/master)
- [Zeta](https://github.com/redcode/Zeta)
- [VGA ライブラリ(一部改変)](https://github.com/vha3/Hunter-Adams-RP2040-Demos/tree/master/VGA_Graphics)
- [LittleFS](https://github.com/littlefs-project/littlefs)
- [fmgen](http://retropc.net/cisc/m88/)
- [pico-extras](https://github.com/raspberrypi/pico-extras)

---
# Gallary

![GAME P6mode3](/pictures/screenshot00.jpg)
![GAME P6mode4](/pictures/screenshot02.jpg)
![GAME MK2](/pictures/screenshot03.jpg)