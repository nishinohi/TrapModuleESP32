# ハードウェア
狩猟用罠検知モジュール
## 概要
携帯電波網からインターネットに接続して罠の検知情報を通知するシステム。  
メッシュネットワークを構築して広範囲、複数の狩猟用罠の作動状況を監視する。
## 使用方法
モジュールは以下の2種類の起動モードがある。基本的には **「設置モードで起動後、任意の位置にモジュールを配置し、スマホからモジュールの設定を罠モードに変更する」** という運用を想定している。
* **設置モード**  
罠を設置する際の起動モード。  
モジュールは起動後、稼働し続ける。メッシュネットワークの接続状態を表すLEDの点滅状態を確認しながら、モジュールのメッシュネットワークが途切れない範囲にモジュールを設置する。また、このとき現在時刻や位置情報、稼働時間帯を設定する。
* **罠モード**  
罠の監視をおこなう際の起動モード。  
モジュールは稼働時間帯の間は1時間毎に起動、停止を繰り返す。起動後は設定した時間分稼働し続け、稼働中に罠の作動を検知した場合親機に検知情報を送信する。

## 環境構築
環境構築は下記のリンクを参照。  
[ESP-WROOM-02を買ったので、環境構築をメモしておく](https://tpedia.tech.gr.jp/20160111434/)  

### 回路に必要なもの
* [ブレッドボード](http://akizukidenshi.com/catalog/g/gP-00315/) - 回路を組むためのもの
* [ESP-WROOM-02](http://akizukidenshi.com/catalog/g/gK-09758/) - 子機同士の通信と罠検知
* [USBシリアル変換モジュール](http://akizukidenshi.com/catalog/g/gK-01977/) - 
ESPモジュールにデータを書き込むのに必要
* [3.3Vレギュレーター](http://akizukidenshi.com/catalog/g/gI-09261/) - 3.3Vならなんでもいい  
* [10kΩ * 5](http://akizukidenshi.com/catalog/g/gR-25103/) - 
プルアップ、プルダウン用
* コンデンサ - 適当な電気容量のもの(0.1μF、47μF一本づつくらい)

### 利用ライブラリ  
* [ESP8266 core for Arduino](https://github.com/esp8266/Arduino.git) - ver2.4.1対応
* [painlessMesh](https://gitlab.com/painlessMesh/painlessMesh.git) - メッシュネットワークI/Fのラッパ。
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - painlessMeshで使用
* [TaskScheduler](https://github.com/arkhipenko/TaskScheduler.git) - painlessMeshで使用
* [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP.git) - painlessMeshで使用
* [AsyncTCP](https://github.com/me-no-dev/AsyncTCP.git) - painlessMeshで使用

## モジュール機能
### 基本機能
* **携帯電波網利用機能**  
親モジュールのみの機能。罠の検知情報をSORACOM Beamを用いて閉域網のMQTTサーバーからTLS1.2でAWS IoTに送信する。GPIO4をTX、GPIO5をRXとしてSIM5320とUARTで通信。
* **メッシュネットワーク**  
モジュールでメッシュネットワークを作成し、直接親機と接続されていない子モジュールでも検知情報の送受信をおこなえる。  
GPIO13が通信確認用LED出力ピン。LEDはメッシュネットワークで接続しているモジュールの数だけ点滅する。
* **罠検知**  
GPIO14の入力がHIGHになった場合検知情報を送信する。
* **設置モード強制移行スイッチ**
GPIO12をLOWにした状態で起動させると、モジュールを設置モードで起動できる。
* **モジュール設定確認・変更**  
子機のWiFiに接続し、子機のIPにブラウザで接続することで現在の設定値の確認、変更が可能。

### 回路図
回路図(trapModule/schematic)を参照。  

### バッテリ残量チェックについて
分圧用の抵抗が無い場合は「#define BATTERY_CHECK」をコメントアウトし、バッテリ残量チェックを無効化すること。ただし、バッテリ残量チェックを無効化した状態でバッテリでモジュールを動かすと、放電終止電圧を超えても動作し続けるため、稼働したまま放置すると電池が壊れるので注意。開発等でPCとUSBで接続して使うなど、バッテリ以外で起動させる場合には機能をOFFにしといたほうがいい。