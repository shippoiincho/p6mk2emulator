PC-6001mkII Emulator for Raspberry Pi Pico
---
![Boot Menu](/pictures/screenshot01.jpg)
NEC PC-6001mkII のエミュレータです。
以下の機能を実装しています。

- メイン RAM(64KB)
- テープ
- PSG

とっても重要なことですが、**音声合成はサポートしていません**。
せっかくの PC-6001mkII ですが**しゃべりません**。
talk 文はエラーにはなりませんが、何も起きません。

---
# 配線など

16色表示に対応するために VGA の接続が変更になっています。

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

---
# テープ

いつもと違って、UART 入出力に対応していません。
LittleFS 上の CAS / P6 形式のファイルをロード・セーブに用います。

デフォルトでは、csave/cload の終了後にファイルクローズを行いません。
これは、BASIC とマシン語を一つの CASファイルに入れている場合に対応しています。

---
# ROM など

いつものように純正ROM が必要です。

- BASICROM.62
- VOICEROM.62
- CGROM60.62
- CGROM60m.62
- KANJIROM.62

`p6mk2rom_dummy.h` を `p6mk2rom.h` にコピーしたのち、
以上のファイルのデータを入れてください。

なお実機の ROM がなくても、以下の互換 ROM ＆ フォントで BASIC まで起動することを確認しています。

- [PC-6001mkII/6601用互換BASIC](http://000.la.coocan.jp/p6/basic66.html)
- [PC-6001 用互換 CGROM](http://eighttails.seesaa.net/article/305067428.html)

---
# 制限事項

- 最初に起動する際に LittleFS のフォーマットで固まることがあります。(リセットでOK)
- Pico SDK 2.0 ではうまく動かないかもしれません。(TinyUSB が固まる?)

---
# ライセンスなど

このエミュレータは以下のライブラリを使用しています。

- [Z80](https://github.com/redcode/Z80/tree/master)
- [Zeta](https://github.com/redcode/Zeta)
- [VGA ライブラリ(一部改変)](https://github.com/vha3/Hunter-Adams-RP2040-Demos/tree/master/VGA_Graphics)
- [LittleFS](https://github.com/littlefs-project/littlefs)

---
# Gallary

![GAME P6mode3](/pictures/screenshot00.jpg)
![GAME P6mode4](/pictures/screenshot02.jpg)
![GAME MK2](/pictures/screenshot03.jpg)