# IQS9151 Gesture Specification (Implementation Synced)

この文書は、`iqs9151_work_cb()` / `iqs9151_one_finger_update()` /
`iqs9151_two_finger_update()` / `iqs9151_three_finger_update()` の
**現実装準拠仕様**です。

## 0. 前提

- IC内蔵ジェスチャレジスタ (`0x101C` / `0x101E`) は使用しない。
- IC側ジェスチャ設定ブロック (`0x11F6..0x1217`) への書き込みは行わない。
- 判定は `0x1014..0x102E` の座標・フラグを基準に、ドライバ側状態機械で行う。
- `SHOW_RESET` 検出時は後続判定を打ち切り、状態を即時リセットする。

## 1. 入力フレーム

|項目|アドレス|型|意味|
| - | - | - | - |
|Relative X|`0x1014`|`int16`|相対X移動量|
|Relative Y|`0x1016`|`int16`|相対Y移動量|
|Info Flags|`0x1020`|`uint16`|`SHOW_RESET` 等|
|Trackpad Flags|`0x1022`|`uint16`|`finger_count` / movement / confidence|
|Finger1 X/Y|`0x1024` / `0x1026`|`uint16`|1本指座標|
|Finger2 X/Y|`0x102C` / `0x102E`|`uint16`|2本指座標|

## 2. 正規化ルール

- `finger_count == 0` のとき、`prev_frame` の座標をクリアする。
- `finger1` / `finger2` の confidence が無効、または座標が `0xFFFF` のとき、
  `prev_frame` の値をフォールバックとして使う。
- `SHOW_RESET` のとき:
  - pinch中なら `INPUT_BTN_7` release を送る
  - `hold_button` が押下中なら release を送る
  - 1F/2F/3Fセッションと慣性状態をリセットする

## 3. 判定パラメータ（既定値）

- 共通:
  - `IQS9151_TAP_REENTRY_WINDOW_MS = 30` (固定)
- 1F:
  - `ONE_FINGER_TAP_MAX_MS = 120` (`CONFIG_INPUT_IQS9151_1F_TAP_MAX_MS`)
  - `ONE_FINGER_TAP_MOVE = 25` (`CONFIG_INPUT_IQS9151_1F_TAP_MOVE`)
  - `ONE_FINGER_CLICK_HOLD_MAX_MS = 230`
    (`CONFIG_INPUT_IQS9151_1F_TAPDRAG_GAP_MAX_MS`)
  - `ONE_FINGER_TAPDRAG_GAP_MAX_MS = 230`
    (`CONFIG_INPUT_IQS9151_1F_TAPDRAG_GAP_MAX_MS`)
- 2F:
  - `TWO_FINGER_TAP_MAX_MS = 130` (`CONFIG_INPUT_IQS9151_2F_TAP_MAX_MS`)
  - `TWO_FINGER_TAP_MOVE = 30` (`CONFIG_INPUT_IQS9151_2F_TAP_MOVE`)
  - `TWO_FINGER_CLICK_HOLD_MAX_MS = 200`
    (`CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS`)
  - `TWO_FINGER_TAPDRAG_GAP_MAX_MS = 200`
    (`CONFIG_INPUT_IQS9151_2F_TAPDRAG_GAP_MAX_MS`)
  - `TWO_FINGER_SCROLL_START_MOVE = 50` (`CONFIG_INPUT_IQS9151_2F_SCROLL_START_MOVE`)
  - `TWO_FINGER_PINCH_START_DISTANCE = 80` (`CONFIG_INPUT_IQS9151_2F_PINCH_START_DISTANCE`)
  - `TWO_FINGER_PINCH_WHEEL_GAIN_X10 = 40` (`CONFIG_INPUT_IQS9151_2F_PINCH_WHEEL_GAIN_X10`)
  - `TWO_FINGER_PINCH_RATIO_X10 = 15` (`CONFIG_INPUT_IQS9151_2F_PINCH_RATIO_X10`)
  - `TWO_FINGER_RELEASE_PENDING_MAX_MS = 150` (固定)
  - `TWO_FINGER_ONE_LEAD_MAX_MS = 120` (固定)
- 3F:
  - `THREE_FINGER_TAP_MAX_MS = 180` (`CONFIG_INPUT_IQS9151_3F_TAP_MAX_MS`)
  - `THREE_FINGER_TAP_MOVE = 30` (`CONFIG_INPUT_IQS9151_3F_TAP_MOVE`)
  - `THREE_FINGER_CLICK_HOLD_MAX_MS = 230`
    (`CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS`)
  - `THREE_FINGER_TAPDRAG_GAP_MAX_MS = 230`
    (`CONFIG_INPUT_IQS9151_3F_TAPDRAG_GAP_MAX_MS`)
  - `THREE_FINGER_RELEASE_PENDING_MAX_MS = 150` (固定)
  - `THREE_FINGER_ONE_LEAD_MAX_MS = 120` (固定)
  - `THREE_FINGER_TWO_LEAD_MAX_MS = 120` (固定)

### 3.1 Kconfig管理パラメータ（ジェスチャ関連）

ドライバ全体の `Kconfig` 一覧は
`documents/iqs9151_kconfig_reference.md` を参照してください。

## 4. ジェスチャ仕様

### 4.1 1F

- 1F Tap:
  - 候補開始: `finger_count==1` かつ
    `prev==0` または `TAP_REENTRY_WINDOW_MS` 内に `finger_count==0`
  - 成立: `finger_count==0` かつ
    `elapsed<=ONE_FINGER_TAP_MAX_MS` かつ `abs(dx/dy)<=ONE_FINGER_TAP_MOVE`
  - `CONFIG_INPUT_IQS9151_1F_PRESSHOLD_ENABLE=y` のとき:
    - `INPUT_BTN_0` を即 press し、`ONE_FINGER_CLICK_HOLD_MAX_MS` まで保持
    - 監視中に2回目タッチが来なければ timeout で release（単クリック確定）
  - `CONFIG_INPUT_IQS9151_1F_PRESSHOLD_ENABLE=n` のとき:
    - `INPUT_BTN_0` click（press+release）
- 1F 2回目タッチ（Tap後監視中）:
  - 開始条件: 1回目Tap成立後、`ONE_FINGER_CLICK_HOLD_MAX_MS` 以内の `0->1`
  - 2回目Tap成立:
    - 2回目の `elapsed<=ONE_FINGER_TAP_MAX_MS`
    - `abs(dx/dy)<=ONE_FINGER_TAP_MOVE`
    - 出力: 先に保持中 `INPUT_BTN_0` を release してから `INPUT_BTN_0` click
      （ダブルクリック相当）
  - 2回目Touch継続:
    - 上記Tap条件を外れた場合は Drag扱いとして `INPUT_BTN_0` press を維持
    - finger-up で `INPUT_BTN_0` release
- 1F Cursor Inertia:
  - 発動: `1->0` release かつ hold release 由来でない場合のみ
  - 直近 `CONFIG_INPUT_IQS9151_CURSOR_INERTIA_RECENT_WINDOW_MS` ms の
    1F移動サンプル (`REL_X/Y`) を評価する
  - 条件:
    - 最終移動サンプルから release まで
      `<= CONFIG_INPUT_IQS9151_CURSOR_INERTIA_STALE_GAP_MS`
    - 直近窓内の移動サンプル数
      `>= CONFIG_INPUT_IQS9151_CURSOR_INERTIA_MIN_SAMPLES`
    - 直近窓内の平均速度
      `>= CONFIG_INPUT_IQS9151_CURSOR_INERTIA_MIN_AVG_SPEED`
    - 優勢軸の符号が揃ったサンプル数
      `>= CONFIG_INPUT_IQS9151_CURSOR_INERTIA_MIN_SAMPLES`
  - seed: 窓内総移動量を経過時間で割り、10ms基準速度へ換算した値を使う
  - release 前に停止して stale gap を超えた場合は発動しない

### 4.2 2F

- 2F Tap:
  - 候補開始:
    - `prev==0` または `TAP_REENTRY_WINDOW_MS` 内に `finger_count==0`
    - または `1->2` one-lead が有効 (`TWO_FINGER_ONE_LEAD_MAX_MS` 内)
  - 候補維持:
    - `elapsed<=TWO_FINGER_TAP_MAX_MS`
    - `abs(centroid_dx/dy)<=TWO_FINGER_TAP_MOVE`
    - `abs(distance_delta)<=TWO_FINGER_TAP_MOVE`
  - 段階リリース:
    - `2->1` で `release_pending` に入り、`TWO_FINGER_RELEASE_PENDING_MAX_MS` 以内の
      `1->0` をTap成立として扱う
  - `CONFIG_INPUT_IQS9151_2F_PRESSHOLD_ENABLE=y` のとき:
    - `INPUT_BTN_1` を即 press し、`TWO_FINGER_CLICK_HOLD_MAX_MS` まで保持
    - 監視中に2回目2Fタッチが来なければ timeout で release（単クリック確定）
  - `CONFIG_INPUT_IQS9151_2F_PRESSHOLD_ENABLE=n` のとき:
    - `INPUT_BTN_1` click（press+release）
- 2F 2回目タッチ（Tap後監視中）:
  - 開始条件:
    - 1回目2F Tap成立後、`TWO_FINGER_CLICK_HOLD_MAX_MS` 以内の `0->2`
    - または `0->1->2` one-lead（2回目タッチでも許容）
  - 2回目Tap成立:
    - 2回目の `elapsed<=TWO_FINGER_TAP_MAX_MS`
    - `abs(centroid_dx/dy)<=TWO_FINGER_TAP_MOVE`
    - `abs(distance_delta)<=TWO_FINGER_TAP_MOVE`
    - 出力: 先に保持中 `INPUT_BTN_1` を release してから `INPUT_BTN_1` click
      （ダブルクリック相当）
  - 2回目Touch継続:
    - 上記Tap条件を外れた場合は Drag扱いとして `INPUT_BTN_1` press を維持
    - Drag中に `2->1` へ遷移した場合も hold は維持し、1F相対移動は `REL_X/Y` として送出
    - 全指離し (`finger_count==0`) で `INPUT_BTN_1` release
- 2F Scroll:
  - 開始: `mode==NONE` かつ
    `max(abs(centroid_dx), abs(centroid_dy)) >= TWO_FINGER_SCROLL_START_MOVE`
    かつ後述の Scroll/Pinch 判定で Scroll が選ばれたとき
  - 出力: `REL_HWHEEL` / `REL_WHEEL`（設定有効軸のみ）
  - Scroll Inertia:
    - `scroll_ended` 時に、直近
      `CONFIG_INPUT_IQS9151_SCROLL_INERTIA_RECENT_WINDOW_MS` ms の
      2F scroll サンプルを評価する
    - 最終 scroll サンプルから release まで
      `<= CONFIG_INPUT_IQS9151_SCROLL_INERTIA_STALE_GAP_MS`
      かつ平均速度/サンプル数条件を満たす場合のみ発動する
    - release 前に停止して stale gap を超えた場合は発動しない
    - scroll inertia 動作中に、新規 1F 接触 (`0->1`) が始まった場合は
      scroll inertia を停止する
  - 終端が `2->1->0` になった場合でも、残り1指は 1Fカーソルへフォールバックさせない
    - 尾部の `finger_count==1` では `REL_X/Y` を送出しない
    - 尾部の `finger_count==1` / `1->0` では cursor inertia の seed/start を行わない
    - 尾部の `2->1` は新規 1F 接触 (`0->1`) とは扱わず、
      この tail だけを理由に scroll inertia は停止しない
    - 抑止は全指離し (`finger_count==0`) まで維持する
- 2F Pinch:
  - 開始: `mode==NONE` かつ
    `abs(distance_delta) >= TWO_FINGER_PINCH_START_DISTANCE`
    かつ後述の Scroll/Pinch 判定で Pinch が選ばれたとき
  - `REL_WHEEL` は `step_dist` を基に
    `wheel = (step_dist * TWO_FINGER_PINCH_WHEEL_GAIN_X10) / (12 * 10)` 相当で算出
    （余りはフレーム間で保持）
  - 出力: `INPUT_BTN_7` press/release + `REL_WHEEL`
  - 終端が `2->1->0` になった場合でも、残り1指は 1Fカーソルへフォールバックさせない
    - 尾部の `finger_count==1` では `REL_X/Y` を送出しない
    - 尾部の `finger_count==1` / `1->0` では cursor inertia の seed/start を行わない
    - 抑止は全指離し (`finger_count==0`) まで維持する
- Scroll/Pinch 判定（`mode==NONE` の各フレームで評価）:
  - `abs_center = max(abs(centroid_dx), abs(centroid_dy))`、
    `abs_dist = abs(distance_delta)`
  - `scroll_ok = scroll有効 かつ abs_center >= TWO_FINGER_SCROLL_START_MOVE`
  - `pinch_ok = pinch有効 かつ abs_dist >= TWO_FINGER_PINCH_START_DISTANCE`
  - `dist_dominates = abs_dist * 10 >= abs_center * CONFIG_INPUT_IQS9151_2F_PINCH_RATIO_X10`
  - 上から順に最初に当てはまったものを採用し、どれにも当てはまらなければ
    `NONE` のまま次フレームを待つ:
    1. `pinch_ok && dist_dominates` → Pinch
    2. `scroll_ok && !dist_dominates` → Scroll
    3. `scroll_ok` かつ pinch 無効 → Scroll
    4. `pinch_ok` かつ scroll 無効 → Pinch
    5. `scroll_ok && abs_center >= 2 * TWO_FINGER_SCROLL_START_MOVE` → Scroll
       （比率が曖昧でも重心移動が絶対量で十分大きい）
    6. `pinch_ok && abs_dist >= 2 * TWO_FINGER_PINCH_START_DISTANCE` → Pinch
       （同上、距離変化側）
  - 一度 mode が決まったら 2F セッション終了まで固定

### 4.3 3F

- 3F Tap:
  - 候補開始:
    - `prev==0` または `TAP_REENTRY_WINDOW_MS` 内に `finger_count==0`
    - または `1->3` one-lead (`THREE_FINGER_ONE_LEAD_MAX_MS` 内)
    - または `2->3` two-lead (`THREE_FINGER_TWO_LEAD_MAX_MS` 内、2F tap条件内)
  - 候補維持:
    - `elapsed<=THREE_FINGER_TAP_MAX_MS`
    - `abs(dx/dy)<=THREE_FINGER_TAP_MOVE`（`finger1` 追跡）
  - 段階リリース:
    - `3->(2/1)` で `release_pending` に入り、
      `THREE_FINGER_RELEASE_PENDING_MAX_MS` 以内の `->0` をTap成立として扱う
  - `CONFIG_INPUT_IQS9151_3F_PRESSHOLD_ENABLE=y` のとき:
    - `INPUT_BTN_2` を即 press し、`THREE_FINGER_CLICK_HOLD_MAX_MS` まで保持
    - 監視中に2回目3Fタッチが来なければ timeout で release（単クリック確定）
  - `CONFIG_INPUT_IQS9151_3F_PRESSHOLD_ENABLE=n` のとき:
    - `INPUT_BTN_2` click（press+release）
- 3F 2回目タッチ（Tap後監視中）:
  - 開始条件: 1回目3F Tap成立後、`THREE_FINGER_CLICK_HOLD_MAX_MS` 以内の `0->3`
  - 2回目Tap成立:
    - 2回目の `elapsed<=THREE_FINGER_TAP_MAX_MS`
    - `abs(dx/dy)<=THREE_FINGER_TAP_MOVE`
    - 出力: 先に保持中 `INPUT_BTN_2` を release してから `INPUT_BTN_2` click
      （ダブルクリック相当）
  - 2回目Touch継続:
    - 上記Tap条件を外れた場合は Drag扱いとして `INPUT_BTN_2` press を維持
    - Drag中に `3->1` へ遷移した場合も hold は維持し、1F相対移動は `REL_X/Y` として送出
    - 全指離し (`finger_count==0`) で `INPUT_BTN_2` release
- 3F Swipe:
  - 1回目3F接触中のみ判定（2回目監視中は無効）
  - 条件: `abs(dx)` または `abs(dy)` が
    `CONFIG_INPUT_IQS9151_3F_SWIPE_THRESHOLD` を超過
  - 出力: 方向別 `INPUT_BTN_3/4/5/6` click
  - 1ショット: 成立後に `three_swipe_sent=true` を保持し、
    3本指接触が終了するまで同一接触中は再送しない

## 5. 優先度・排他

- 2F/3F の Tap は `release_pending` により段階リリースを許容する。
- `hold_button` は単一ラッチ。
  別Tap/Hold成立時は先に既存holdをreleaseし、新イベントは同フレーム抑止する。
- 遷移フレームでは、旧セッション終了処理と新セッション開始判定を同一フレームで扱う。
  そのため `1->2`, `1->3`, `2->3` の lead 判定をサポートする。

## 6. 出力イベント

- Click:
  - 1F Tap:
    - `CONFIG_INPUT_IQS9151_1F_PRESSHOLD_ENABLE=y`: `INPUT_BTN_0` press -> (timeout または2回目タッチ判定後) release
    - `CONFIG_INPUT_IQS9151_1F_PRESSHOLD_ENABLE=n`: `INPUT_BTN_0` click
  - 1F Double Tap: 1回目保持中 `INPUT_BTN_0` release + `INPUT_BTN_0` click
  - 2F Tap:
    - `CONFIG_INPUT_IQS9151_2F_PRESSHOLD_ENABLE=y`: `INPUT_BTN_1` press -> (timeout または2回目タッチ判定後) release
    - `CONFIG_INPUT_IQS9151_2F_PRESSHOLD_ENABLE=n`: `INPUT_BTN_1` click
  - 2F Double Tap: 1回目保持中 `INPUT_BTN_1` release + `INPUT_BTN_1` click
  - 3F Tap:
    - `CONFIG_INPUT_IQS9151_3F_PRESSHOLD_ENABLE=y`: `INPUT_BTN_2` press -> (timeout または2回目タッチ判定後) release
    - `CONFIG_INPUT_IQS9151_3F_PRESSHOLD_ENABLE=n`: `INPUT_BTN_2` click
  - 3F Double Tap: 1回目保持中 `INPUT_BTN_2` release + `INPUT_BTN_2` click
- Hold:
  - 1F TapDrag: 1回目Tap時点から `INPUT_BTN_0` press を維持し、2回目タッチUPでrelease
  - 2F TapDrag: 1回目Tap時点から `INPUT_BTN_1` press を維持し、全指離しでrelease
  - 3F TapDrag: 1回目Tap時点から `INPUT_BTN_2` press を維持し、全指離しでrelease
- Swipe:
  - 3F: `INPUT_BTN_3/4/5/6` click
- Pinch:
  - `INPUT_BTN_7` press/release + `REL_WHEEL`
- Relative:
  - 1Fで `REL_X/Y`
  - 2F/3F TapDrag中でも `finger_count==1` なら `REL_X/Y` を送出（holdは維持）
  - 2F Scroll/Pinchで `REL_WHEEL/HWHEEL`

## 7. 改訂履歴

- 2026-02-25: 初版ドラフト
- 2026-02-27: 実装追従更新
  - 2F/3F の `release_pending` 記載追加
  - `TAP_MAX_MS` を 1F/2F/3F で明示分離 (`150/150/180`)
  - 高優先パラメータ（Tap時間/移動、Hold最短、2F Scroll開始、2F Pinch開始）を `Kconfig` 化
  - `Kconfig` 管理の一覧を `documents/iqs9151_kconfig_reference.md` へ分割
  - 3F `1->3` / `2->3` lead 許容を記載
  - 現在の閾値 (`SCROLL_START_MOVE=50`, `PINCH_START_DISTANCE=80`) に更新
  - 3F Swipe の1ショット実装を `three_swipe_sent` ラッチ方式に更新
- 2026-02-28: HOLD候補の失効ルールを追記
  - 1F/2F/3F Hold は、Hold移動閾値を一度でも超えた接触では再成立しない仕様に更新
- 2026-03-04: 1F Hold を TapDrag 方式へ変更
  - 1回目短タップ後、`ONE_FINGER_TAPDRAG_GAP_MAX_MS` 以内の2回目タッチで Hold 判定
  - 1F Hold のラッチ解除方式を廃止し、2回目タッチUPで即 release に更新
- 2026-03-05: 1F Tap/Drag を deferred-click 方式へ更新
  - 1回目Tapで `INPUT_BTN_0` を即 press し、`ONE_FINGER_CLICK_HOLD_MAX_MS` まで保持
  - 2回目タッチなしは timeout release、2回目短タップは double click、継続タッチは drag継続に更新
  - 1F既定閾値を `TAP_MAX_MS=120`, `CLICK_HOLD_MAX_MS=230` に更新
- 2026-03-05: 2F/3F Tap/Drag を deferred-click 方式へ更新
  - 2F/3Fも 1回目Tapで `BTN1/BTN2` を即 press し、`*_CLICK_HOLD_MAX_MS=230` まで保持
  - 2回目タッチなしは timeout release、2回目短タップは double click、継続タッチは drag継続に更新
  - 2F/3F の `*_HOLD_MIN_MS` は互換項目（現行仕様では未使用）へ整理
- 2026-03-05: 2Fの実測ログ（Run ID: 20260305_134715）に基づく閾値調整
  - 2F既定閾値を `TAP_MAX_MS=130`, `CLICK_HOLD_MAX_MS=200` に更新
  - 2F deferred-click 監視中の2回目タッチで `0->1->2` one-lead を許容
- 2026-03-06: `*_HOLD_MIN_MS` 設定と到達不能 Hold 判定を削除
  - `drivers/input/Kconfig` から `1F/2F/3F_HOLD_MIN_MS` を削除
  - 2F/3F の到達不能 Hold 判定ブロックを `iqs9151.c` から削除
- 2026-03-06: 2F/3F TapDrag中の `2->1` / `3->1` を仕様として明記
  - hold (`BTN1/BTN2`) 維持中でも 1F `REL_X/Y` を送出する挙動を追加記載
- 2026-03-08: 2F Scroll/Pinch 終端の `2->1->0` で 1Fカーソル系を抑止
  - Scroll/Pinch 尾部の `finger_count==1` では `REL_X/Y` を出さず、
    cursor inertia の seed/start も全指離しまで抑止する仕様に更新
- 2026-03-16: inertia 発動条件を recent-window / stale-gap 判定へ更新
  - 1F cursor inertia は release 直前の短時間窓に十分な速度と方向の持続がある場合のみ発動
  - 指を停止したまま release した場合は stale gap により inertia を抑止
  - 2F scroll inertia も同様に recent-window / stale-gap 判定へ更新
- 2026-04-09: 2F scroll inertia の停止条件に新規 1F 接触 (`0->1`) を追加
  - scroll inertia 動作中に新しい 1F タッチが始まった場合は inertia を停止する
  - 既存の `2->1->0` tail 抑止仕様とは独立で、tail の `2->1` は停止条件に含めない
