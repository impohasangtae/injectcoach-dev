#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// KOKCHI - Integrated Web HMI Prototype v3.2
// Rotation + Injection Session + Result / History
// =====================================================

const char* AP_SSID = "InjectCoach-Test";
const char* AP_PASSWORD = "12345678";

WebServer server(80);

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">

<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">

  <title>KOKCHI 자가주사 수행 보조 시스템</title>

  <style>
    * {
      box-sizing: border-box;
    }

    :root {
      --bg: #f4f6f8;
      --card: #ffffff;
      --text: #20242a;
      --muted: #747d87;
      --line: #dce1e6;
      --soft: #f5f7f9;
      --dark: #20242a;
      --recent: #d7dce2;
      --history: #edf0f3;
      --warning-bg: #f3ece7;
      --warning-line: #9a6747;
      --ok-bg: #edf3ef;
      --ok-line: #617b69;
    }

    body {
      margin: 0;
      font-family: Arial, "Noto Sans KR", sans-serif;
      background: var(--bg);
      color: var(--text);
    }

    .container {
      width: min(920px, 92%);
      margin: 0 auto;
      padding: 26px 0 60px;
    }

    .header {
      margin-bottom: 18px;
    }

    .brand {
      font-size: 34px;
      font-weight: 800;
      letter-spacing: 2px;
    }

    .subtitle {
      margin-top: 5px;
      color: var(--muted);
      font-size: 14px;
    }

    .profile-row {
      margin-top: 12px;
      display: flex;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
    }

    .profile-label {
      font-size: 12px;
      color: #858d97;
      font-weight: 700;
    }

    .profile-chip {
      background: #e9edf2;
      padding: 7px 11px;
      border-radius: 999px;
      font-size: 13px;
      font-weight: 700;
    }

    /* =========================
       Stepper
       ========================= */

    .stepper {
      background: white;
      border-radius: 18px;
      padding: 18px 16px;
      margin-bottom: 16px;
      box-shadow: 0 5px 20px rgba(0,0,0,0.05);
    }

    .stepper-row {
      display: grid;
      grid-template-columns: 1fr 42px 1fr 42px 1fr;
      align-items: center;
      gap: 4px;
    }

    .step-node {
      text-align: center;
      min-width: 0;
    }

    .step-circle {
      width: 30px;
      height: 30px;
      border-radius: 50%;
      border: 2px solid #cfd5db;
      background: white;
      color: #8d959e;
      display: flex;
      align-items: center;
      justify-content: center;
      margin: 0 auto 7px;
      font-size: 12px;
      font-weight: 800;
    }

    .step-title {
      font-size: 12px;
      color: #8a929b;
      font-weight: 700;
      white-space: nowrap;
    }

    .step-line {
      height: 2px;
      background: #dce1e6;
      margin-top: -19px;
    }

    .step-node.active .step-circle {
      background: var(--dark);
      color: white;
      border-color: var(--dark);
    }

    .step-node.active .step-title {
      color: var(--dark);
      font-weight: 800;
    }

    .step-node.done .step-circle {
      background: #e8ecef;
      color: var(--dark);
      border-color: #aeb6bf;
    }

    .step-line.done {
      background: #aeb6bf;
    }

    /* =========================
       Common
       ========================= */

    .card {
      background: var(--card);
      border-radius: 18px;
      padding: 22px;
      margin-bottom: 16px;
      box-shadow: 0 5px 20px rgba(0,0,0,0.06);
    }

    .section-number {
      font-size: 12px;
      color: #9299a2;
      font-weight: 800;
      margin-bottom: 6px;
    }

    .card-title {
      font-size: 19px;
      font-weight: 800;
      margin-bottom: 7px;
    }

    .card-desc {
      font-size: 13px;
      color: var(--muted);
      line-height: 1.6;
      margin-bottom: 18px;
    }

    .step-panel {
      display: none;
    }

    .step-panel.visible {
      display: block;
    }

    .state-box {
      padding: 15px 16px;
      background: var(--soft);
      border-radius: 14px;
      margin-bottom: 16px;
    }

    .state-label {
      font-size: 12px;
      color: #7d858f;
      margin-bottom: 5px;
      font-weight: 700;
    }

    .state-value {
      font-size: 22px;
      font-weight: 800;
    }

    button {
      border: 1px solid #d9dde3;
      background: white;
      color: #252a30;
      border-radius: 12px;
      padding: 12px 14px;
      font-size: 14px;
      cursor: pointer;
      transition: 0.15s;
    }

    button:active {
      transform: scale(0.98);
    }

    button.selected {
      background: var(--dark);
      border-color: var(--dark);
      color: white;
    }

    .action-button {
      width: 100%;
      margin-top: 16px;
      background: var(--dark);
      color: white;
      border-color: var(--dark);
      font-weight: 700;
      padding: 14px;
    }

    .action-button.secondary {
      background: white;
      color: var(--dark);
      border-color: #cfd5db;
    }

    .action-button:disabled {
      background: #c8cdd3;
      border-color: #c8cdd3;
      color: white;
      cursor: default;
    }

    /* =========================
       STEP 1
       ========================= */

    .profile-rule-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
      margin-bottom: 14px;
    }

    .profile-rule-item {
      padding: 11px 10px;
      border: 1px solid var(--line);
      border-radius: 12px;
      background: #fafbfc;
    }

    .profile-rule-label {
      display: block;
      font-size: 9px;
      color: #8a929b;
      font-weight: 800;
      margin-bottom: 4px;
    }

    .profile-rule-value {
      display: block;
      font-size: 12px;
      color: var(--text);
      font-weight: 800;
      line-height: 1.35;
    }

    .rotation-principle {
      padding: 13px 14px;
      border-radius: 12px;
      background: #f7f8f9;
      margin-bottom: 18px;
      font-size: 12px;
      color: #69727c;
      line-height: 1.6;
    }

    .rotation-principle strong {
      color: var(--text);
    }

    .relative-zone-note {
      margin: -2px 0 15px;
      font-size: 10px;
      color: #9098a1;
      line-height: 1.55;
    }

    .region-buttons {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 9px;
      margin-bottom: 20px;
    }

    .map-title-row {
      display: flex;
      align-items: flex-end;
      justify-content: space-between;
      gap: 12px;
      margin-bottom: 10px;
    }

    .map-region-name {
      font-size: 16px;
      font-weight: 800;
    }

    .map-guide {
      font-size: 11px;
      color: #969da6;
    }

    .rotation-map {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 9px;
      margin-bottom: 14px;
    }

    .site-cell {
      position: relative;
      min-height: 82px;
      border: 1px solid var(--line);
      border-radius: 14px;
      background: white;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 5px;
      font-weight: 800;
      cursor: pointer;
      transition: 0.15s;
      overflow: hidden;
    }

    .site-cell.recent {
      background: var(--recent);
    }

    .site-cell.history {
      background: var(--history);
    }

    .site-cell.selected {
      border: 3px solid var(--dark);
      background: white;
    }

    .site-number {
      font-size: 18px;
    }

    .cell-badges {
      min-height: 17px;
      display: flex;
      justify-content: center;
      gap: 4px;
      flex-wrap: wrap;
    }

    .cell-badge {
      font-size: 8px;
      line-height: 1;
      padding: 4px 6px;
      border-radius: 999px;
      background: rgba(255,255,255,0.82);
      border: 1px solid rgba(70,75,82,0.18);
      color: #59616a;
      letter-spacing: 0.4px;
      font-weight: 800;
    }

    .cell-badge.selected-badge {
      background: var(--dark);
      color: white;
      border-color: var(--dark);
    }

    .legend {
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      margin-bottom: 18px;
    }

    .legend-item {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 11px;
      color: #737b85;
    }

    .legend-dot {
      width: 13px;
      height: 13px;
      border-radius: 4px;
      border: 1px solid #d5dae0;
      background: white;
    }

    .legend-dot.recent-dot {
      background: var(--recent);
    }

    .legend-dot.history-dot {
      background: var(--history);
    }

    .legend-dot.selected-dot {
      background: white;
      border: 2px solid var(--dark);
    }

    .rotation-check {
      border-radius: 14px;
      padding: 16px;
      background: var(--soft);
      border-left: 4px solid transparent;
    }

    .rotation-check.warning {
      background: var(--warning-bg);
      border-left-color: var(--warning-line);
    }

    .rotation-check.ok {
      background: var(--ok-bg);
      border-left-color: var(--ok-line);
    }

    .check-title {
      font-weight: 800;
      margin-bottom: 6px;
    }

    .check-message {
      font-size: 13px;
      color: #666f79;
      line-height: 1.55;
    }

    .current-site {
      font-size: 22px;
      font-weight: 800;
      margin: 16px 0 5px;
    }

    .current-note {
      color: #7a828c;
      font-size: 12px;
      line-height: 1.5;
    }

    /* =========================
       STEP 2
       ========================= */

    .session-site {
      padding: 14px 15px;
      border-radius: 14px;
      background: var(--soft);
      margin-bottom: 14px;
    }

    .session-site-label {
      font-size: 11px;
      color: #818a94;
      font-weight: 700;
      margin-bottom: 4px;
    }

    .session-site-value {
      font-size: 18px;
      font-weight: 800;
    }

    .sensor-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }

    .sensor-card {
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 14px;
      min-height: 96px;
    }

    .sensor-label {
      font-size: 11px;
      color: #858d97;
      font-weight: 800;
      margin-bottom: 8px;
    }

    .sensor-value {
      font-size: 17px;
      font-weight: 800;
      margin-bottom: 4px;
    }

    .sensor-sub {
      font-size: 10px;
      color: #9299a2;
      line-height: 1.4;
    }

    .demo-note {
      margin-top: 14px;
      padding: 12px 13px;
      border-radius: 12px;
      background: #f7f7f8;
      color: #858d97;
      font-size: 11px;
      line-height: 1.6;
    }

    .button-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
      margin-top: 14px;
    }

    .button-row .action-button {
      margin-top: 0;
    }

    /* =========================
       STEP 3
       ========================= */

    .result-banner {
      padding: 18px;
      border-radius: 15px;
      background: var(--soft);
      margin-bottom: 14px;
    }

    .result-kicker {
      font-size: 11px;
      color: #818a94;
      font-weight: 800;
      margin-bottom: 5px;
    }

    .result-title {
      font-size: 23px;
      font-weight: 800;
    }

    .result-table {
      display: flex;
      flex-direction: column;
      border: 1px solid var(--line);
      border-radius: 14px;
      overflow: hidden;
      margin-bottom: 14px;
    }

    .result-row {
      display: grid;
      grid-template-columns: 105px 1fr;
      gap: 10px;
      padding: 12px 14px;
      border-bottom: 1px solid #edf0f2;
      font-size: 13px;
    }

    .result-row:last-child {
      border-bottom: none;
    }

    .result-label {
      color: #7b848e;
      font-weight: 700;
    }

    .result-value {
      font-weight: 800;
    }

    .confirmation {
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 16px;
      margin-top: 14px;
    }

    .confirm-site {
      font-size: 19px;
      font-weight: 800;
      margin: 8px 0 14px;
    }

    .previous-action {
      width: 100%;
      margin: 0 0 18px;
      background: white;
      color: var(--dark);
      border-color: #cfd5db;
      font-weight: 700;
    }

    .confirm-buttons {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }

    .confirm-button {
      background: var(--dark);
      color: white;
      border-color: var(--dark);
      font-weight: 800;
    }

    .correct-button {
      background: white;
      color: var(--dark);
      border-color: #cfd5db;
      font-weight: 700;
    }

    .recorded-box {
      display: none;
      padding: 16px;
      border-radius: 14px;
      background: var(--ok-bg);
      border-left: 4px solid var(--ok-line);
      margin-top: 14px;
    }

    .recorded-box.visible {
      display: block;
    }

    /* =========================
       History
       ========================= */

    .history-empty {
      font-size: 13px;
      color: #858d97;
      padding: 13px;
      background: var(--soft);
      border-radius: 12px;
    }

    .history-list {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }

    .history-item {
      display: grid;
      grid-template-columns: 32px 1fr auto;
      align-items: center;
      gap: 10px;
      padding: 11px 12px;
      background: #f6f7f8;
      border-radius: 12px;
    }

    .history-index {
      font-size: 11px;
      font-weight: 800;
      color: #89919b;
    }

    .history-site {
      font-size: 13px;
      font-weight: 700;
    }

    .history-meta {
      margin-top: 3px;
      font-size: 10px;
      color: #9198a0;
      line-height: 1.35;
    }

    .history-status {
      font-size: 9px;
      color: #727a83;
      font-weight: 800;
      text-align: right;
    }

    .prototype-note {
      margin-top: 10px;
      font-size: 11px;
      color: #9aa1a9;
      line-height: 1.6;
    }

    @media (max-width: 600px) {
      .container {
        width: 92%;
        padding-top: 22px;
      }

      .brand {
        font-size: 29px;
      }

      .card {
        padding: 19px;
      }

      .stepper {
        padding: 16px 10px;
      }

      .stepper-row {
        grid-template-columns: 1fr 26px 1fr 26px 1fr;
      }

      .step-title {
        font-size: 11px;
      }

      .sensor-grid {
        grid-template-columns: 1fr 1fr;
      }

      .profile-rule-grid {
        grid-template-columns: 1fr 1fr 1fr;
        gap: 6px;
      }

      .profile-rule-item {
        padding: 10px 7px;
      }

      .profile-rule-value {
        font-size: 11px;
      }

      .result-row {
        grid-template-columns: 86px 1fr;
      }
    }
  </style>
</head>


<body>

<div class="container">

  <!-- Header -->
  <div class="header">
    <div class="brand">KOKCHI</div>
    <div class="subtitle">자가주사 수행 보조 시스템 · Functional Embedded Prototype</div>

    <div class="profile-row">
      <span class="profile-label">현재 사용 모드</span>
      <span class="profile-chip">데모 프로필</span>
    </div>
  </div>


  <!-- Stepper -->
  <div class="stepper">
    <div class="stepper-row">

      <div class="step-node active" id="stepNode1">
        <div class="step-circle" id="stepCircle1">1</div>
        <div class="step-title">위치 선택</div>
      </div>

      <div class="step-line" id="stepLine1"></div>

      <div class="step-node" id="stepNode2">
        <div class="step-circle" id="stepCircle2">2</div>
        <div class="step-title">주사 수행</div>
      </div>

      <div class="step-line" id="stepLine2"></div>

      <div class="step-node" id="stepNode3">
        <div class="step-circle" id="stepCircle3">3</div>
        <div class="step-title">결과 / 기록</div>
      </div>

    </div>
  </div>


  <!-- =====================================================
       STEP 1
       ===================================================== -->

  <div class="step-panel visible" id="step1Panel">

    <div class="card">
      <div class="section-number">STEP 01</div>
      <div class="card-title">주사 위치 로테이션</div>
      <div class="card-desc">
        데모 프로필에서 사용할 주사 부위와 상대적 세부 위치를 선택합니다.
        선택한 위치는 최근 확인 이력과 비교되며, 세션 완료 후 사용자가 확인한 경우에만 기록됩니다.
      </div>

      <div class="profile-rule-grid">
        <div class="profile-rule-item">
          <span class="profile-rule-label">허용 부위</span>
          <span class="profile-rule-value" id="allowedRegionSummary">4개</span>
        </div>

        <div class="profile-rule-item">
          <span class="profile-rule-label">최근 비교</span>
          <span class="profile-rule-value" id="recentWindowSummary">3회</span>
        </div>

        <div class="profile-rule-item">
          <span class="profile-rule-label">기록 기준</span>
          <span class="profile-rule-value">세션 확인 후</span>
        </div>
      </div>

      <div class="rotation-principle">
        <strong>Rotation Assistance 기준</strong><br>
        같은 주사 부위라도 세부 위치를 구분해 이력을 관리합니다.
        최근 사용한 동일 세부 위치는 경고하며, 시스템이 특정 위치를 자동 처방하지는 않습니다.
      </div>

      <div class="map-title-row">
        <div class="map-region-name">주사 부위 선택</div>
        <div class="map-guide">Allowed regions · Demo Profile</div>
      </div>

      <div class="region-buttons">
        <button data-region-key="abdomen" onclick="selectRegion(this, 'abdomen')">복부</button>
        <button data-region-key="leftThigh" onclick="selectRegion(this, 'leftThigh')">왼쪽 허벅지</button>
        <button data-region-key="rightThigh" onclick="selectRegion(this, 'rightThigh')">오른쪽 허벅지</button>
        <button data-region-key="upperArm" onclick="selectRegion(this, 'upperArm')">위팔</button>
      </div>

      <div class="map-title-row">
        <div class="map-region-name" id="mapRegionName">부위를 먼저 선택해주세요</div>
        <div class="map-guide">상대적 세부 위치</div>
      </div>

      <div class="rotation-map" id="rotationMap">
        <div class="site-cell" data-index="1" onclick="selectSubRegion(1)">
          <div class="site-number">1</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="2" onclick="selectSubRegion(2)">
          <div class="site-number">2</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="3" onclick="selectSubRegion(3)">
          <div class="site-number">3</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="4" onclick="selectSubRegion(4)">
          <div class="site-number">4</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="5" onclick="selectSubRegion(5)">
          <div class="site-number">5</div>
          <div class="cell-badges"></div>
        </div>

        <div class="site-cell" data-index="6" onclick="selectSubRegion(6)">
          <div class="site-number">6</div>
          <div class="cell-badges"></div>
        </div>
      </div>

      <div class="relative-zone-note">
        ※ 1–6은 데모 프로필의 상대적 위치 구역입니다.
        실제 피부의 cm 단위 안전거리나 자동 위치 판정을 의미하지 않습니다.
      </div>

      <div class="legend">
        <div class="legend-item">
          <span class="legend-dot"></span>
          사용 가능
        </div>

        <div class="legend-item">
          <span class="legend-dot recent-dot"></span>
          가장 최근
        </div>

        <div class="legend-item">
          <span class="legend-dot history-dot"></span>
          최근 이력
        </div>

        <div class="legend-item">
          <span class="legend-dot selected-dot"></span>
          현재 선택
        </div>
      </div>

      <div class="rotation-check" id="rotationCheck">
        <div class="check-title">최근 사용 위치 확인</div>
        <div class="check-message" id="rotationMessage">
          위치를 선택하면 최근 확인된 이력과 비교합니다.
        </div>
      </div>

      <div class="current-site" id="currentSite">선택된 위치 없음</div>

      <div class="current-note">
        현재 선택은 아직 사용 이력에 저장되지 않습니다.
      </div>

      <button class="action-button" id="startSessionButton" onclick="startInjectionSession()" disabled>
        이 위치로 시작
      </button>
    </div>


    <div class="card">
      <div class="section-number">RECENT HISTORY</div>
      <div class="card-title">최근 주사 위치 기록</div>
      <div class="card-desc">
        확인된 세션만 표시됩니다. 최근 기록은 다음 위치 선택 시 Rotation Check에 사용됩니다.
      </div>

      <div id="historyContainerStep1">
        <div class="history-empty">아직 확인된 기록이 없습니다.</div>
      </div>
    </div>

  </div>


  <!-- =====================================================
       STEP 2
       ===================================================== -->

  <div class="step-panel" id="step2Panel">

    <div class="card">
      <div class="section-number">STEP 02</div>
      <div class="card-title">주사 수행</div>
      <div class="card-desc">
        실제 통합 단계에서는 ESP32가 센서 입력을 읽고 현재 수행 상태를 판단합니다.
        이 Web v3에서는 최종 화면 구조를 먼저 완성하고, 이후 실제 센서값과 State Machine을 연결합니다.
      </div>

      <div class="session-site">
        <div class="session-site-label">현재 선택 위치</div>
        <div class="session-site-value" id="sessionSite">-</div>
      </div>

      <div class="state-box">
        <div class="state-label">현재 단계</div>
        <div class="state-value" id="sessionState">READY</div>
      </div>

      <div class="sensor-grid">

        <div class="sensor-card">
          <div class="sensor-label">PAD 접촉</div>
          <div class="sensor-value" id="padValue">대기</div>
          <div class="sensor-sub">FSR-PAD · GPIO32</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">PEN 접촉</div>
          <div class="sensor-value" id="penValue">대기</div>
          <div class="sensor-sub">FSR-PEN · GPIO34</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">펜 자세</div>
          <div class="sensor-value" id="orientationValue">대기</div>
          <div class="sensor-sub">MPU6050 · Roll / Pitch</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">플런저</div>
          <div class="sensor-value" id="plungerValue">대기</div>
          <div class="sensor-sub">Tact Button · GPIO27</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">유지 시간</div>
          <div class="sensor-value" id="holdValue">0.0 s</div>
          <div class="sensor-sub">State Machine timer</div>
        </div>

        <div class="sensor-card">
          <div class="sensor-label">Rotation</div>
          <div class="sensor-value" id="rotationValue">확인 완료</div>
          <div class="sensor-sub">최근 확인 이력 비교</div>
        </div>

      </div>

      <div class="demo-note">
        ※ 현재 버튼은 Web v3의 단계 전환과 결과 확인 흐름을 검증하기 위한 임시 기능입니다.
        최종 통합에서는 실제 ESP32 State Machine의 정상 완료 이벤트가 STEP 3 전환을 수행합니다.
      </div>

      <div class="button-row">
        <button class="action-button secondary" onclick="backToSiteSelection()">
          위치 다시 선택
        </button>

        <button class="action-button" onclick="completeDemoInjection()">
          시연용 세션 완료
        </button>
      </div>

    </div>

  </div>


  <!-- =====================================================
       STEP 3
       ===================================================== -->

  <div class="step-panel" id="step3Panel">

    <div class="card">
      <div class="section-number">STEP 03</div>
      <div class="card-title">결과 및 기록</div>

      <div class="result-banner">
        <div class="result-kicker">SESSION RESULT</div>
        <div class="result-title" id="resultTitle">세션 완료</div>
      </div>

      <div class="result-table">

        <div class="result-row">
          <div class="result-label">위치</div>
          <div class="result-value" id="resultSite">-</div>
        </div>

        <div class="result-row">
          <div class="result-label">Rotation</div>
          <div class="result-value" id="resultRotation">-</div>
        </div>

        <div class="result-row">
          <div class="result-label">접촉</div>
          <div class="result-value">센서 통합 후 표시</div>
        </div>

        <div class="result-row">
          <div class="result-label">펜 자세</div>
          <div class="result-value">센서 통합 후 표시</div>
        </div>

        <div class="result-row">
          <div class="result-label">Hold</div>
          <div class="result-value">센서 통합 후 표시</div>
        </div>

      </div>

      <div class="confirmation" id="confirmation">
        <div class="check-title">실제 수행 위치 확인</div>

        <div class="check-message">
          실제 수행한 위치가 아래 선택과 일치합니까?
          확인한 경우에만 주사 위치 기록에 저장됩니다.
        </div>

        <div class="confirm-site" id="confirmSite">-</div>

        <button
          class="previous-action"
          id="backToInjectionButton"
          onclick="backToInjectionSession()">
          ← 주사 수행으로 돌아가기
        </button>

        <div class="confirm-buttons">
          <button class="correct-button" onclick="correctSite()">
            위치 수정
          </button>

          <button class="confirm-button" onclick="confirmSite()">
            이 위치로 기록
          </button>
        </div>
      </div>

      <div class="recorded-box" id="recordedBox">
        <div class="check-title">기록 완료</div>
        <div class="check-message">
          확인된 위치가 주사 위치 기록에 저장되었습니다.
        </div>
      </div>

      <button class="action-button" id="newSessionButton" onclick="startNewSession()" style="display:none;">
        새 세션 시작
      </button>
    </div>


    <div class="card">
      <div class="section-number">CONFIRMED HISTORY</div>
      <div class="card-title">주사 위치 기록</div>
      <div class="card-desc">
        사용자가 세션 완료 후 확인한 위치만 저장합니다.
      </div>

      <div id="historyContainerStep3">
        <div class="history-empty">아직 확인된 기록이 없습니다.</div>
      </div>

      <div class="prototype-note">
        ※ 현재 Web v3의 이력은 페이지가 열려 있는 동안의 프로토타입 메모리에 저장됩니다.
        ESP32 재부팅 후에도 유지되는 비휘발성 저장은 최종 통합 단계에서 별도로 연결할 수 있습니다.
      </div>
    </div>

  </div>

</div>


<script>

// =====================================================
// Configuration
// =====================================================

const DEMO_PROFILE = {
  name: "데모 프로필",
  allowedRegions: [
    "abdomen",
    "leftThigh",
    "rightThigh",
    "upperArm"
  ],
  recentWindow: 3,
  maxHistory: 10
};

const REGION_CONFIG = {
  abdomen: {
    name: "복부",
    code: "A"
  },
  leftThigh: {
    name: "왼쪽 허벅지",
    code: "LT"
  },
  rightThigh: {
    name: "오른쪽 허벅지",
    code: "RT"
  },
  upperArm: {
    name: "위팔",
    code: "UA"
  }
};

const RECENT_WINDOW = DEMO_PROFILE.recentWindow;
const MAX_HISTORY = DEMO_PROFILE.maxHistory;


// =====================================================
// State variables
// =====================================================

let currentStep = 1;

let selectedRegionKey = "";
let selectedSubRegion = null;
let currentSessionSite = "";

let sessionHadRotationWarning = false;

// Confirmed sessions only
let historyList = [];


// =====================================================
// Device / Protocol Profile
// =====================================================

function applyProfile() {

  document
    .querySelectorAll(".region-buttons button")
    .forEach(button => {

      const key =
        button.dataset.regionKey;

      const allowed =
        DEMO_PROFILE.allowedRegions.includes(key);

      button.style.display =
        allowed ? "block" : "none";
    });

  document.getElementById("allowedRegionSummary").innerText =
    DEMO_PROFILE.allowedRegions.length + "개";

  document.getElementById("recentWindowSummary").innerText =
    DEMO_PROFILE.recentWindow + "회";
}


// =====================================================
// Step navigation
// =====================================================

function showStep(step) {

  currentStep = step;

  document.getElementById("step1Panel")
    .classList.toggle("visible", step === 1);

  document.getElementById("step2Panel")
    .classList.toggle("visible", step === 2);

  document.getElementById("step3Panel")
    .classList.toggle("visible", step === 3);

  updateStepper();
  window.scrollTo({ top: 0, behavior: "smooth" });
}


function updateStepper() {

  for (let i = 1; i <= 3; i++) {

    const node =
      document.getElementById("stepNode" + i);

    const circle =
      document.getElementById("stepCircle" + i);

    node.classList.remove("active", "done");

    if (i < currentStep) {
      node.classList.add("done");
      circle.innerText = "✓";
    } else if (i === currentStep) {
      node.classList.add("active");
      circle.innerText = String(i);
    } else {
      circle.innerText = String(i);
    }
  }

  document.getElementById("stepLine1")
    .classList.toggle("done", currentStep >= 2);

  document.getElementById("stepLine2")
    .classList.toggle("done", currentStep >= 3);
}


// =====================================================
// Region / site selection
// =====================================================

function selectRegion(button, regionKey) {

  document
    .querySelectorAll(".region-buttons button")
    .forEach(b => b.classList.remove("selected"));

  button.classList.add("selected");

  selectedRegionKey = regionKey;
  selectedSubRegion = null;
  currentSessionSite = "";

  const region =
    REGION_CONFIG[selectedRegionKey];

  document.getElementById("mapRegionName").innerText =
    region.name + " · 세부 위치";

  document.getElementById("currentSite").innerText =
    region.name + " / 세부 위치 선택";

  document.getElementById("startSessionButton").disabled =
    true;

  clearRotationMessage();

  renderRotationMap();
}


function selectSubRegion(index) {

  if (selectedRegionKey === "") {
    alert("먼저 주사 부위를 선택해주세요.");
    return;
  }

  selectedSubRegion = index;

  const region =
    REGION_CONFIG[selectedRegionKey];

  currentSessionSite =
    region.name + " / " + region.code + index;

  document.getElementById("currentSite").innerText =
    currentSessionSite;

  document.getElementById("startSessionButton").disabled =
    false;

  runRotationCheck();
  renderRotationMap();
}


function clearRotationMessage() {

  const box =
    document.getElementById("rotationCheck");

  box.classList.remove("warning", "ok");

  document.getElementById("rotationMessage").innerText =
    "세부 위치를 선택하면 최근 확인된 이력과 비교합니다.";
}


// =====================================================
// Rotation logic
// =====================================================

function isCurrentSiteRecent() {

  const recentHistory =
    historyList.slice(0, RECENT_WINDOW);

  return recentHistory.some(
    item => item.site === currentSessionSite
  );
}


function runRotationCheck() {

  const box =
    document.getElementById("rotationCheck");

  const message =
    document.getElementById("rotationMessage");

  const usedRecently =
    isCurrentSiteRecent();

  sessionHadRotationWarning =
    usedRecently;

  box.classList.remove("warning", "ok");

  if (usedRecently) {

    box.classList.add("warning");

    message.innerHTML =
      "<strong>최근 사용한 동일 세부 위치입니다.</strong><br>" +
      "현재 선택은 최근 " +
      RECENT_WINDOW +
      "회의 확인 이력에 포함됩니다. " +
      "같은 주사 부위 안에서도 다른 세부 위치를 고려할 수 있습니다.";

  } else {

    box.classList.add("ok");

    message.innerHTML =
      "<strong>최근 동일 세부 위치 기록이 없습니다.</strong><br>" +
      "현재 선택은 최근 " +
      RECENT_WINDOW +
      "회의 확인 이력과 동일하지 않습니다.";
  }
}


// =====================================================
// Rotation map rendering
// =====================================================

function renderRotationMap() {

  document
    .querySelectorAll(".site-cell")
    .forEach(cell => {

      cell.classList.remove(
        "selected",
        "recent",
        "history"
      );

      const badgeContainer =
        cell.querySelector(".cell-badges");

      badgeContainer.innerHTML = "";

      if (selectedRegionKey === "") {
        return;
      }

      const index =
        parseInt(cell.dataset.index);

      const region =
        REGION_CONFIG[selectedRegionKey];

      const siteName =
        region.name + " / " + region.code + index;

      const recentHistory =
        historyList.slice(0, RECENT_WINDOW);

      const historyOrder =
        recentHistory.findIndex(
          item => item.site === siteName
        );

      if (historyOrder === 0) {

        cell.classList.add("recent");

        addCellBadge(
          badgeContainer,
          "RECENT"
        );

      } else if (historyOrder > 0) {

        cell.classList.add("history");

        addCellBadge(
          badgeContainer,
          "HISTORY"
        );
      }

      if (selectedSubRegion === index) {

        cell.classList.add("selected");

        addCellBadge(
          badgeContainer,
          "SELECTED",
          true
        );
      }
    });
}


function addCellBadge(container, text, selected = false) {

  const badge =
    document.createElement("span");

  badge.className =
    "cell-badge" +
    (selected ? " selected-badge" : "");

  badge.innerText = text;

  container.appendChild(badge);
}


// =====================================================
// STEP 1 -> STEP 2
// =====================================================

function startInjectionSession() {

  if (currentSessionSite === "") {
    return;
  }

  // Freeze the warning result at the moment session starts
  sessionHadRotationWarning =
    isCurrentSiteRecent();

  document.getElementById("sessionSite").innerText =
    currentSessionSite;

  document.getElementById("sessionState").innerText =
    "READY";

  document.getElementById("rotationValue").innerText =
    sessionHadRotationWarning
      ? "최근 위치 경고"
      : "최근 중복 없음";

  // Sensor placeholders until physical integration
  document.getElementById("padValue").innerText =
    "대기";

  document.getElementById("penValue").innerText =
    "대기";

  document.getElementById("orientationValue").innerText =
    "대기";

  document.getElementById("plungerValue").innerText =
    "대기";

  document.getElementById("holdValue").innerText =
    "0.0 s";

  showStep(2);
}


function backToSiteSelection() {
  showStep(1);
}


// =====================================================
// STEP 2 -> STEP 3
// =====================================================

function completeDemoInjection() {

  if (currentSessionSite === "") {
    return;
  }

  document.getElementById("resultSite").innerText =
    currentSessionSite;

  document.getElementById("resultRotation").innerText =
    sessionHadRotationWarning
      ? "최근 위치 경고 확인"
      : "최근 동일 위치 없음";

  document.getElementById("confirmSite").innerText =
    currentSessionSite;

  document.getElementById("confirmation").style.display =
    "block";

  document.getElementById("backToInjectionButton").style.display =
    "block";

  document.getElementById("recordedBox")
    .classList.remove("visible");

  document.getElementById("newSessionButton").style.display =
    "none";

  showStep(3);
}


// =====================================================
// STEP 3 -> STEP 2
// =====================================================

function backToInjectionSession() {

  if (currentSessionSite === "") {
    return;
  }

  // 기록 확정 전까지만 STEP 2로 돌아갈 수 있다.
  showStep(2);
}


// =====================================================
// Confirm / Correct
// =====================================================

function confirmSite() {

  if (currentSessionSite === "") {
    return;
  }

  const newRecord = {
    site: currentSessionSite,
    regionKey: selectedRegionKey,
    subRegion: selectedSubRegion,
    result: "COMPLETE",
    rotationWarning: sessionHadRotationWarning
  };

  historyList.unshift(newRecord);

  if (historyList.length > MAX_HISTORY) {
    historyList.pop();
  }

  renderHistory();

  document.getElementById("confirmation").style.display =
    "none";

  document.getElementById("backToInjectionButton").style.display =
    "none";

  document.getElementById("recordedBox")
    .classList.add("visible");

  document.getElementById("newSessionButton").style.display =
    "block";
}


function correctSite() {

  document.getElementById("confirmation").style.display =
    "none";

  document.getElementById("recordedBox")
    .classList.remove("visible");

  document.getElementById("newSessionButton").style.display =
    "none";

  showStep(1);

  document.getElementById("rotationMessage").innerHTML =
    "<strong>위치 수정 중입니다.</strong><br>" +
    "실제 수행한 위치에 맞게 주사 부위와 세부 위치를 다시 선택해주세요.";

  renderRotationMap();
}


// =====================================================
// New session
// =====================================================

function startNewSession() {

  selectedRegionKey = "";
  selectedSubRegion = null;
  currentSessionSite = "";
  sessionHadRotationWarning = false;

  document
    .querySelectorAll(".region-buttons button")
    .forEach(b => b.classList.remove("selected"));

  document.getElementById("mapRegionName").innerText =
    "부위를 먼저 선택해주세요";

  document.getElementById("currentSite").innerText =
    "선택된 위치 없음";

  document.getElementById("startSessionButton").disabled =
    true;

  clearRotationMessage();
  renderRotationMap();

  document.getElementById("confirmation").style.display =
    "block";

  document.getElementById("backToInjectionButton").style.display =
    "block";

  document.getElementById("recordedBox")
    .classList.remove("visible");

  document.getElementById("newSessionButton").style.display =
    "none";

  showStep(1);
}


// =====================================================
// History
// =====================================================

function renderHistory() {

  renderHistoryInto(
    "historyContainerStep1"
  );

  renderHistoryInto(
    "historyContainerStep3"
  );

  renderRotationMap();
}


function renderHistoryInto(containerId) {

  const container =
    document.getElementById(containerId);

  if (historyList.length === 0) {

    container.innerHTML =
      '<div class="history-empty">' +
      '아직 확인된 기록이 없습니다.' +
      '</div>';

    return;
  }

  let html =
    '<div class="history-list">';

  historyList.forEach((item, index) => {

    html +=
      '<div class="history-item">' +

        '<div class="history-index">' +
          String(index + 1).padStart(2, "0") +
        '</div>' +

        '<div>' +
          '<div class="history-site">' +
            item.site +
          '</div>' +

          '<div class="history-meta">' +
            '데모 프로필 · 완료' +
            (item.rotationWarning
              ? ' · 최근 위치 경고 발생'
              : '') +
          '</div>' +
        '</div>' +

        '<div class="history-status">' +
          (index === 0
            ? "가장 최근"
            : "확인 기록") +
        '</div>' +

      '</div>';
  });

  html += '</div>';

  container.innerHTML = html;
}


// =====================================================
// Initial rendering
// =====================================================

applyProfile();
updateStepper();
renderHistory();
renderRotationMap();

</script>

</body>
</html>
)rawliteral";


// =====================================================
// ESP32 Setup
// =====================================================

void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("Starting KOKCHI Integrated Web v3.2...");

  WiFi.mode(WIFI_AP);

  bool apStarted =
    WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    Serial.println("SoftAP started.");
  } else {
    Serial.println("SoftAP failed.");
  }

  Serial.print("SSID: ");
  Serial.println(AP_SSID);

  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {

    server.send_P(
      200,
      "text/html; charset=UTF-8",
      PAGE
    );

  });

  server.begin();

  Serial.println("WebServer started.");
}


// =====================================================
// ESP32 Loop
// =====================================================

void loop() {

  server.handleClient();

}
