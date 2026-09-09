# IQS9151 Driver Kconfig Reference

この文書は `drivers/input/Kconfig` の内容を、ドライバ全体の設定一覧として整理したものです。

## 1. Driver Core

|Symbol|Type|Default|役割|
| - | - | - | - |
|`CONFIG_INPUT_IQS9151`|bool|`y`|IQS9151ドライバ有効化|
|`CONFIG_INPUT_IQS9151_LOG_LEVEL`|int|`INPUT_LOG_LEVEL` (LOG有効時), それ以外 `0`|ドライバログレベル|
|`CONFIG_INPUT_IQS9151_INIT_PRIORITY`|int|`80`|ドライバ初期化優先度|

## 2. Rotation

`CONFIG_INPUT_IQS9151_ROTATE` choice により以下のいずれか1つを選択します（既定: `ROTATE_0`）。

|Symbol|Type|Default|役割|
| - | - | - | - |
|`CONFIG_INPUT_IQS9151_ROTATE_0`|bool|`y` (choice既定)|回転なし|
|`CONFIG_INPUT_IQS9151_ROTATE_90`|bool|`n`|90度回転|
|`CONFIG_INPUT_IQS9151_ROTATE_180`|bool|`n`|180度回転|
|`CONFIG_INPUT_IQS9151_ROTATE_270`|bool|`n`|270度回転|

## 3. IC Parameter Overrides

|Symbol|Type|Default|役割|
| - | - | - | - |
|`CONFIG_INPUT_IQS9151_RESOLUTION_X`|int|`2457`|X解像度設定（有効域 `0..4095`）|
|`CONFIG_INPUT_IQS9151_RESOLUTION_Y`|int|`3072`|Y解像度設定（有効域 `0..4095`）|
|`CONFIG_INPUT_IQS9151_ATI_TARGETCOUNT`|int|`400`|Trackpad ATIターゲット|
|`CONFIG_INPUT_IQS9151_DYNAMIC_FILTER_BOTTOM_SPEED`|int|`30`|Dynamic Filter Bottom Speed（有効域 `0..2047`）|
|`CONFIG_INPUT_IQS9151_DYNAMIC_FILTER_TOP_SPEED`|int|`511`|Dynamic Filter Top Speed（有効域 `0..2047`）|
|`CONFIG_INPUT_IQS9151_DYNAMIC_FILTER_BOTTOM_BETA`|int|`20`|Dynamic Filter Bottom Beta|
|`CONFIG_INPUT_IQS9151_TOUCH_SET_THRESHOLD`|int|`30`|Touch Set Threshold Multiplier（0x11CC、有効域 `0..255`）|
|`CONFIG_INPUT_IQS9151_TOUCH_CLEAR_THRESHOLD`|int|`26`|Touch Clear Threshold Multiplier（0x11CD、有効域 `0..255`）|
|`CONFIG_INPUT_IQS9151_ACTIVE_MODE_SAMPLING_PERIOD_MS`|int|`10`|Active Mode Sampling Period ms（0x11A2）|
|`CONFIG_INPUT_IQS9151_IDLE_TOUCH_MODE_SAMPLING_PERIOD_MS`|int|`50`|Idle-Touch Mode Sampling Period ms（0x11A4）|
|`CONFIG_INPUT_IQS9151_IDLE_MODE_SAMPLING_PERIOD_MS`|int|`50`|Idle Mode Sampling Period ms（0x11A6）|
|`CONFIG_INPUT_IQS9151_LP1_MODE_SAMPLING_PERIOD_MS`|int|`50`|LP1 Mode Sampling Period ms（0x11A8）|
|`CONFIG_INPUT_IQS9151_LP2_MODE_SAMPLING_PERIOD_MS`|int|`50`|LP2 Mode Sampling Period ms（0x11AA）|
|`CONFIG_INPUT_IQS9151_ACTIVE_MODE_TIMEOUT_MS`|int|`1500`|Active to Idle Mode Timeout ms（0x11B4）|
|`CONFIG_INPUT_IQS9151_ALP_SET_DEBOUNCE`|int|`2`|ALP Set Debounce（0x11D0）|
|`CONFIG_INPUT_IQS9151_ALP_CLEAR_DEBOUNCE`|int|`2`|ALP Clear Debounce（0x11D1）|
|`CONFIG_INPUT_IQS9151_STATIONARY_TOUCH_MOV_THRESHOLD`|int|`5`|Stationary Touch Movement Threshold（0x11F0）|
|`CONFIG_INPUT_IQS9151_JITTER_FILTER_DELTA`|int|`2`|Jitter Filter Delta Threshold（0x11F4）|
|`CONFIG_INPUT_IQS9151_FINGER_CONFIDENCE_THRESHOLD`|int|`20`|Finger Confidence Threshold（0x11F5）|

## 4. Gesture Detection and Thresholds

|Symbol|Type|Default|役割|
| - | - | - | - |
|`CONFIG_INPUT_IQS9151_1F_TAP_ENABLE`|bool|`y`|1F Tap 有効/無効|
|`CONFIG_INPUT_IQS9151_1F_TAP_MAX_MS`|int|`250`|1F Tap/2回目Tap 判定の最大時間|
|`CONFIG_INPUT_IQS9151_1F_TAP_MOVE`|int|`50`|1F Tap 移動しきい値|
|`CONFIG_INPUT_IQS9151_1F_PRESSHOLD_ENABLE`|bool|`y`|1F deferred-click/drag 有効/無効|
|`CONFIG_INPUT_IQS9151_1F_TAPDRAG_GAP_MAX_MS`|int|`160`|1F Tap後にBTN0を保持して2回目タッチを待つ最大時間|
|`CONFIG_INPUT_IQS9151_2F_TAP_ENABLE`|bool|`y`|2F Tap 有効/無効|
|`CONFIG_INPUT_IQS9151_2F_TAP_MAX_MS`|int|`250`|2F Tap 最大時間|
|`CONFIG_INPUT_IQS9151_2F_TAP_MOVE`|int|`50`|2F Tap 移動しきい値（重心/距離）|
|`CONFIG_INPUT_IQS9151_2F_PRESSHOLD_ENABLE`|bool|`y`|2F deferred-click/drag 有効/無効|
|`CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS`|int|`200`|2F Tap後にBTN1を保持して2回目2Fタッチを待つ最大時間|
|`CONFIG_INPUT_IQS9151_SCROLL_X_ENABLE`|bool|`y`|2F 横スクロール有効/無効|
|`CONFIG_INPUT_IQS9151_SCROLL_Y_ENABLE`|bool|`y`|2F 縦スクロール有効/無効|
|`CONFIG_INPUT_IQS9151_2F_SCROLL_START_MOVE`|int|`50`|2F Scroll 開始しきい値|
|`CONFIG_INPUT_IQS9151_2F_PINCH_ENABLE`|bool|`y`|2F Pinch 有効/無効|
|`CONFIG_INPUT_IQS9151_2F_PINCH_START_DISTANCE`|int|`100`|2F Pinch 開始しきい値|
|`CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10`|int|`40`|2F Pinch `REL_WHEEL` ゲイン（x10）|
|`CONFIG_INPUT_IQS9151_2F_PINCH_RATIO_X10`|int|`15`|2F Scroll/Pinch 判定比率（x10）。累積距離変化が累積重心移動の ratio/10 倍以上なら Pinch、未満なら Scroll。範囲 5〜50|
|`CONFIG_INPUT_IQS9151_3F_TAP_ENABLE`|bool|`y`|3F Tap 有効/無効|
|`CONFIG_INPUT_IQS9151_3F_TAP_MAX_MS`|int|`200`|3F Tap 最大時間|
|`CONFIG_INPUT_IQS9151_3F_TAP_MOVE`|int|`35`|3F Tap 移動しきい値|
|`CONFIG_INPUT_IQS9151_3F_PRESSHOLD_ENABLE`|bool|`y`|3F deferred-click/drag 有効/無効|
|`CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS`|int|`200`|3F Tap後にBTN2を保持して2回目3Fタッチを待つ最大時間|
|`CONFIG_INPUT_IQS9151_3F_SWIPE_THRESHOLD`|int|`200`|3F Swipe しきい値|

## 5. Inertia

|Symbol|Type|Default|役割|
| - | - | - | - |
|`CONFIG_INPUT_IQS9151_CURSOR_INERTIA_ENABLE`|bool|`y`|1Fカーソル慣性 有効/無効|
|`CONFIG_INPUT_IQS9151_CURSOR_INERTIA_DECAY`|int|`950`|1Fカーソル慣性 減衰率|
|`CONFIG_INPUT_IQS9151_CURSOR_INERTIA_RECENT_WINDOW_MS`|int|`60`|1Fカーソル慣性の recent-window 判定時間|
|`CONFIG_INPUT_IQS9151_CURSOR_INERTIA_STALE_GAP_MS`|int|`35`|最終1F移動から release までの最大許容時間|
|`CONFIG_INPUT_IQS9151_CURSOR_INERTIA_MIN_SAMPLES`|int|`2`|1Fカーソル慣性に必要な直近移動サンプル数|
|`CONFIG_INPUT_IQS9151_CURSOR_INERTIA_MIN_AVG_SPEED`|int|`10`|1Fカーソル慣性に必要な平均速度|
|`CONFIG_INPUT_IQS9151_SCROLL_INERTIA_ENABLE`|bool|`y`|2Fスクロール慣性 有効/無効|
|`CONFIG_INPUT_IQS9151_SCROLL_INERTIA_DECAY`|int|`980`|2Fスクロール慣性 減衰率|
|`CONFIG_INPUT_IQS9151_SCROLL_INERTIA_RECENT_WINDOW_MS`|int|`60`|2Fスクロール慣性の recent-window 判定時間|
|`CONFIG_INPUT_IQS9151_SCROLL_INERTIA_STALE_GAP_MS`|int|`35`|最終2Fスクロールから release までの最大許容時間|
|`CONFIG_INPUT_IQS9151_SCROLL_INERTIA_MIN_SAMPLES`|int|`1`|2Fスクロール慣性に必要な直近スクロールサンプル数|
|`CONFIG_INPUT_IQS9151_SCROLL_INERTIA_MIN_AVG_SPEED`|int|`4`|2Fスクロール慣性に必要な平均速度|

## 6. Report Rate

|Symbol|Type|Default|役割|
| - | - | - | - |
|`CONFIG_INPUT_IQS9151_CURSOR_REPORT_INTERVAL_MS`|int|`0`|1Fカーソル REL_X/Y の最小送信間隔(ms)。間隔内の移動は合算して 1 回で送る。0 でフレームごとに送る。BLE で送信が詰まる場合は 16〜25 程度|
|`CONFIG_INPUT_IQS9151_SCROLL_REPORT_INTERVAL_MS`|int|`0`|2Fスクロール WHEEL/HWHEEL の最小送信間隔(ms)。間隔内のスクロールは合算して 1 回で送る。0 でフレームごとに送る。BLE で送信が詰まる場合は 16〜25 程度|

間隔 > 0 のとき、アイドル後の最初の移動は即時に送り、以降は「前回送信から間隔以上経過」したフレームで合算値を送る。指を離した(1 本指でなくなった)フレーム、ボタン(K)イベントを送る直前、フレームが止まって間隔が経過したとき(遅延 work)にも残りを送る。1 本指のまま移動なしのフレームが来ても送らない。慣性(inertia)とピンチの WHEEL は対象外。

## 7. Persistence / Test

|Symbol|Type|Default|役割|
| - | - | - | - |
|`CONFIG_INPUT_IQS9151_SETTINGS`|bool|`y` (SETTINGS有効時)|ランタイムパラメータの永続化（`depends on SETTINGS`）。`tp save` で保存し、起動時の `settings_load()` で適用|
|`CONFIG_INPUT_IQS9151_TEST`|bool|`n`|ZTEST用の内部テストフック有効化（`depends on ZTEST`）|

## ランタイム調整(INPUT_IQS9151_SHELL)

`CONFIG_INPUT_IQS9151_SHELL=y`(`CONFIG_SHELL=y` が必要)で `tp` シェルコマンドが使える。
パラメータ名は `CONFIG_INPUT_IQS9151_<NAME>` の `<NAME>` を小文字化したもの(例 `1f_tap_max_ms`)。
IC レジスタ系(`tp list` の kind が `ic_u8` / `ic_u16`)は次のフレーム処理時に書き込まれる。
変更は揮発性で、`tp save` しない限り再起動すると Kconfig の値に戻る。

    tp info                  side=central|peripheral uptime_ms=<n> params=<count> saved=yes|no
    tp list                  <name> <value> <min> <max> <kind> <default>
    tp get <name>
    tp set <name> <value>
    tp reset                 Kconfig 既定に戻し、保存ブロブも削除する
    tp save                  現在の全値を settings に保存する(OK saved / ERR <errno>)
    tp reati
    tp trace on|off          T F / T E 行を LOG(INF) に出す
    tp summary on|off        試行ごとの T S 行を LOG(INF) に出す(既定 on)
    tp live on|off [hz]      フレームごとの T F 相当の情報を間引いてコールバックへ渡す(既定 off、hz 省略時 60)

`tp trace` / `tp summary` の出力を見るには、ビルド時に `CONFIG_INPUT_IQS9151_LOG_LEVEL` を 3 (INF) 以上にしておく必要がある。
`tp live` は LOG 出力ではなく `iqs9151_dev_set_frame_callback(cb, user_data)` で登録したコールバック(`struct iqs9151_frame_info`、`iqs9151_params.h`)を呼ぶための機能で、tp-tuner のリアルタイム表示など向け。`on` は hz(1..100、既定 `IQS9151_LIVE_HZ_DEFAULT`=60)を指定でき、`OK live=on hz=<n>` を返す。省略や範囲外の値は `ERR hz 1..100` になる。`off` は `OK live=off` を返し、間引き状態(直前に呼んだ指本数/hold/2f モード/時刻)をリセットする。間引き規則は、指本数・`hold`・`2f_mode` のいずれかが前回コールバック時から変わっていれば毎回呼び、変わっていなければ前回から `1000/hz` ms 以上経過したフレームだけ呼ぶ。`tp trace` の `T F` 行の出力そのものは変えない。
`T F` 行では、2本指セッションが終わるフレームで `2f_mode` が 0 になる。

### 試行要約(T S)

指の接触が始まってから、離してタップドラッグの許容間隔(`1f`/`2f`/`3f_tapdrag_gap_max_ms` の最大値)+100ms(下限 200ms・上限 1000ms)何も触れないまでを 1 試行とし、終了時に 1 行出す。この待ち時間以内の再接触は同じ試行の 2 回目以降の接触として数える。トレース ON/OFF とは独立で、`tp summary off` で止められる。

    T S <start_ms> <end_ms> <contacts> <fingers_max> <down_ms> <gap_ms> <move_sum> <centroid_move> <dist_delta> <mode2f> <btn_press_bits> <btn_release_bits> <wheel_count> <wheel_sum> <rel_count> <drops> <hold>

|値|意味|
| - | - |
|`start_ms`|最初の接触フレームの時刻(uptime ms)|
|`end_ms`|最後に指を離したフレームの時刻(アイドル待ちは含まない)|
|`contacts`|接触回数(離してアイドル待ち以内の再接触を数える)|
|`fingers_max`|試行中の最大指本数|
|`down_ms`|1 回目の接触の押下時間(接触開始→その離し)|
|`gap_ms`|1 回目の離しから 2 回目の接触までの ms(2 回目がなければ 0)|
|`move_sum`|1 本指フレームの `abs(rel_x)+abs(rel_y)` の合計|
|`centroid_move`|2 本指セッション中の重心移動 `max(abs(dx), abs(dy))` の最大値|
|`dist_delta`|2 本指セッションの指間距離変化の最終値|
|`mode2f`|2 本指モード 0=なし 1=スクロール 2=ピンチ(両方あれば後勝ち)|
|`btn_press_bits`/`btn_release_bits`|送ったボタンの押し/離し。bit n = BTN_n(0..7)|
|`wheel_count`/`wheel_sum`|REL_WHEEL/HWHEEL の送信件数と値の合計(慣性・ピンチ分も含む)|
|`rel_count`|REL_X/REL_Y の送信件数|
|`drops`|`input_report` が 0 以外を返した件数|
|`hold`|deferred-click(タップ後のボタン保持)が発生したら 1|

要約は `struct iqs9151_attempt_summary`(`iqs9151_params.h`)で、`iqs9151_dev_set_summary_callback(cb, user_data)` で登録したコールバックにも同じ内容が渡る(GATT 通知などに使う)。コールバックは `tp summary off` でも呼ばれる。試行終了から先(慣性の残りなど)に送ったイベントは次の試行に含まれない。

`iqs9151_summary_codec.c`(`iqs9151_params.h` に宣言)は、この要約を GATT/split 転送向けに `uint32_t words[IQS9151_SUMMARY_WORDS]`(10 語)へパックする。語 0=`start_ms`、1=`end_ms`、2=`contacts | fingers_max<<8 | mode2f<<16 | hold<<24`、3=`min(down_ms,65535) | min(gap_ms,65535)<<16`、4=`move_sum`、5=`centroid_move`、6=`dist_delta`(符号ビットごと `uint32_t` 化)、7=`btn_press_bits | btn_release_bits<<8 | wheel_count<<16`、8=`wheel_sum`(同様に `uint32_t` 化)、9=`rel_count | drops<<16`。`iqs9151_summary_pack`/`iqs9151_summary_unpack` で相互変換でき、`iqs9151_summary_format` は上記の `T S` 行と同じ書式の文字列を作る。

### 永続化(INPUT_IQS9151_SETTINGS)

settings キー `iqs9151/params` に 1 ブロブで保存する。書式は `uint16 version(=1)`, `uint16 count`, `int32 values[count]`(すべて little-endian、値は `tp list` の順)。
起動時は ZMK の `main()` が呼ぶ `settings_load()`(ドライバ init より後)でハンドラの `set` が動き、各値を `iqs9151_dev_param_set` で適用する。IC 系は保留ビットに積まれ次フレームで書かれる。
version か count が合わないブロブは無視して LOG(WRN) を出し、Kconfig 既定のまま動く。範囲外の値はその値だけ飛ばす。
`saved=yes|no` は「保存ブロブを読み込んだ、または今回の起動で `tp save` した」かを示し、`tp reset` で `no` に戻る。
`CONFIG_INPUT_IQS9151_SETTINGS` 無効時は `tp save` が `ERR -134`(ENOTSUP)、`saved=no` 固定になる。
