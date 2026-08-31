#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// KOKCHI - Rotation Assistance Web Prototype v2
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

  <title>KOKCHI Rotation Assistance</title>

  <style>
    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      font-family: Arial, "Noto Sans KR", sans-serif;
      background: #f4f6f8;
      color: #20242a;
    }

    .container {
      width: min(920px, 92%);
      margin: 0 auto;
      padding: 28px 0 60px;
    }

    /* =========================
       Header
       ========================= */

    .header {
      margin-bottom: 22px;
    }

    .brand {
      font-size: 34px;
      font-weight: 800;
      letter-spacing: 2px;
    }

    .subtitle {
      margin-top: 5px;
      color: #6f7781;
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
       Common card
       ========================= */

    .card {
      background: white;
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
      font-size: 17px;
      font-weight: 800;
      margin-bottom: 7px;
    }

    .card-desc {
      font-size: 13px;
      color: #777f89;
      line-height: 1.55;
      margin-bottom: 18px;
    }

    /* =========================
       State
       ========================= */

    .state-box {
      padding: 15px 16px;
      background: #f5f6f8;
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

    /* =========================
       Region selector
       ========================= */

    .region-buttons {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 9px;
      margin-bottom: 20px;
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
      background: #20242a;
      border-color: #20242a;
      color: white;
    }

    /* =========================
       Rotation map
       ========================= */

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
      min-height: 70px;
      border: 1px solid #dce0e5;
      border-radius: 14px;
      background: white;
      display: flex;
      align-items: center;
      justify-content: center;
      font-weight: 800;
      cursor: pointer;
      transition: 0.15s;
    }

    .site-cell:hover {
      border-color: #aeb5be;
    }

    .site-cell.selected {
      border: 2px solid #20242a;
      box-shadow: inset 0 0 0 2px white;
    }

    .site-cell.recent-1 {
      background: #d9dde2;
    }

    .site-cell.recent-2 {
      background: #e6e9ed;
    }

    .site-cell.recent-3 {
      background: #f0f2f4;
    }

    .site-cell.selected::after {
      content: "SELECTED";
      position: absolute;
      bottom: 5px;
      font-size: 8px;
      letter-spacing: 0.7px;
      color: #5d646d;
    }

    /* =========================
       Legend
       ========================= */

    .legend {
      display: flex;
      flex-wrap: wrap;
      gap: 13px;
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

    .legend-dot.most-recent {
      background: #d9dde2;
    }

    .legend-dot.previous {
      background: #e6e9ed;
    }

    .legend-dot.selected-dot {
      background: white;
      border: 2px solid #20242a;
    }

    /* =========================
       Rotation check
       ========================= */

    .rotation-check {
      border-radius: 14px;
      padding: 16px;
      background: #f5f6f8;
    }

    .check-title {
      font-weight: 800;
      margin-bottom: 6px;
    }

    .check-message {
      font-size: 13px;
      color: #666f79;
      line-height: 1.5;
    }

    .warning {
      background: #f1f1f1;
      border-left: 4px solid #5c626a;
    }

    /* =========================
       Current selection
       ========================= */

    .current-site {
      font-size: 23px;
      font-weight: 800;
      margin-bottom: 5px;
    }

    .current-note {
      color: #7a828c;
      font-size: 12px;
      line-height: 1.5;
    }

    .action-button {
      width: 100%;
      margin-top: 16px;
      background: #20242a;
      color: white;
      border-color: #20242a;
      font-weight: 700;
      padding: 14px;
    }

    .action-button:disabled {
      background: #c8cdd3;
      border-color: #c8cdd3;
      cursor: default;
    }

    /* =========================
       Confirm
       ========================= */

    .confirmation {
      display: none;
      margin-top: 16px;
      border: 1px solid #dce0e5;
      border-radius: 14px;
      padding: 16px;
    }

    .confirmation.visible {
      display: block;
    }

    .confirm-site {
      font-size: 19px;
      font-weight: 800;
      margin: 8px 0 14px;
    }

    .confirm-buttons {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px;
    }

    .confirm-button {
      background: #20242a;
      color: white;
      border-color: #20242a;
    }

    /* =========================
       History
       ========================= */

    .history-empty {
      font-size: 13px;
      color: #858d97;
      padding: 13px;
      background: #f5f6f8;
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

    .history-status {
      font-size: 10px;
      color: #8c949d;
    }

    /* =========================
       Footer note
       ========================= */

    .prototype-note {
      margin-top: 10px;
      font-size: 11px;
      color: #9aa1a9;
      line-height: 1.6;
    }

    @media (max-width: 600px) {

      .container {
        width: 92%;
        padding-top: 24px;
      }

      .brand {
        font-size: 29px;
      }

      .card {
        padding: 19px;
      }

      .region-buttons {
        grid-template-columns: 1fr 1fr;
      }
    }
  </style>
</head>


<body>

<div class="container">

  <!-- =========================================
       Header
       ========================================= -->

  <div class="header">

    <div class="brand">KOKCHI</div>

    <div class="subtitle">
      Embedded Self-Injection Support Prototype
    </div>

    <div class="profile-row">
      <span class="profile-label">DEVICE / PROTOCOL PROFILE</span>
      <span class="profile-chip">Demo Profile</span>
    </div>

  </div>


  <!-- =========================================
       Current state
       ========================================= -->

  <div class="card">

    <div class="section-number">CURRENT SESSION</div>

    <div class="state-box">
      <div class="state-label">CURRENT STATE</div>
      <div class="state-value" id="state">
        IDLE
      </div>
    </div>

    <div class="current-site" id="currentSite">
      No site selected
    </div>

    <div class="current-note">
      선택한 위치는 아직 사용 이력에 저장되지 않습니다.
      실제 세션 완료 후 확인된 위치만 기록됩니다.
    </div>

  </div>


  <!-- =========================================
       Rotation Assistance
       ========================================= -->

  <div class="card">

    <div class="section-number">01</div>

    <div class="card-title">
      Rotation Assistance
    </div>

    <div class="card-desc">
      주사 영역과 세부 위치를 선택하고 최근 사용 이력을 확인합니다.
      현재 프로토타입은 실제 피부 좌표를 자동 측정하지 않으며,
      region / sub-region 수준의 이력 관리를 지원합니다.
    </div>


    <!-- Region -->

    <div class="map-title-row">
      <div class="map-region-name">
        Injection Region
      </div>
    </div>

    <div class="region-buttons">

      <button onclick="selectRegion(this, 'Abdomen')">
        Abdomen
      </button>

      <button onclick="selectRegion(this, 'Left Thigh')">
        Left Thigh
      </button>

      <button onclick="selectRegion(this, 'Right Thigh')">
        Right Thigh
      </button>

      <button onclick="selectRegion(this, 'Upper Arm')">
        Upper Arm
      </button>

    </div>


    <!-- Sub-region map -->

    <div class="map-title-row">

      <div class="map-region-name" id="mapRegionName">
        Select a region
      </div>

      <div class="map-guide">
        Relative sub-regions
      </div>

    </div>


    <div class="rotation-map" id="rotationMap">

      <div class="site-cell" data-index="1"
           onclick="selectSubRegion(1)">
        1
      </div>

      <div class="site-cell" data-index="2"
           onclick="selectSubRegion(2)">
        2
      </div>

      <div class="site-cell" data-index="3"
           onclick="selectSubRegion(3)">
        3
      </div>

      <div class="site-cell" data-index="4"
           onclick="selectSubRegion(4)">
        4
      </div>

      <div class="site-cell" data-index="5"
           onclick="selectSubRegion(5)">
        5
      </div>

      <div class="site-cell" data-index="6"
           onclick="selectSubRegion(6)">
        6
      </div>

    </div>


    <!-- Legend -->

    <div class="legend">

      <div class="legend-item">
        <span class="legend-dot"></span>
        Available
      </div>

      <div class="legend-item">
        <span class="legend-dot most-recent"></span>
        Most recent
      </div>

      <div class="legend-item">
        <span class="legend-dot previous"></span>
        Previous
      </div>

      <div class="legend-item">
        <span class="legend-dot selected-dot"></span>
        Selected
      </div>

    </div>


    <!-- Rotation Check -->

    <div class="rotation-check" id="rotationCheck">

      <div class="check-title">
        Rotation Check
      </div>

      <div class="check-message" id="rotationMessage">
        위치를 선택하면 최근 사용 이력과 비교합니다.
      </div>

    </div>


    <!-- Demo session button -->

    <button
      class="action-button"
      id="completeButton"
      onclick="completeDemoSession()"
      disabled>

      Complete Demo Session

    </button>


    <!-- Confirmation -->

    <div class="confirmation" id="confirmation">

      <div class="check-title">
        Confirm Injection Site
      </div>

      <div class="check-message">
        실제 수행한 위치가 아래 선택과 일치하는지 확인해주세요.
      </div>

      <div class="confirm-site" id="confirmSite">
        -
      </div>

      <div class="confirm-buttons">

        <button
          class="confirm-button"
          onclick="confirmSite()">
          Confirm
        </button>

        <button onclick="correctSite()">
          Correct Site
        </button>

      </div>

    </div>

    <div class="prototype-note">
      ※ Complete Demo Session은 Rotation UI 검증을 위한 임시 버튼입니다.
      최종 버전에서는 ESP32 State Machine의 정상 완료 이벤트로 대체합니다.
    </div>

  </div>


  <!-- =========================================
       Confirmed history
       ========================================= -->

  <div class="card">

    <div class="section-number">02</div>

    <div class="card-title">
      Confirmed Site History
    </div>

    <div class="card-desc">
      실제 세션 완료 후 확인된 위치만 최근 이력에 저장합니다.
    </div>

    <div id="historyContainer">

      <div class="history-empty">
        No confirmed history
      </div>

    </div>

  </div>

</div>


<script>

// =====================================================
// State variables
// =====================================================

let selectedRegion = "";
let selectedSubRegion = null;
let currentSessionSite = "";

// Confirmed history only
let historyList = [];

// Most recent N confirmed sessions used for warning
const RECENT_WINDOW = 3;


// =====================================================
// Region selection
// =====================================================

function selectRegion(button, region) {

  document
    .querySelectorAll(".region-buttons button")
    .forEach(b => b.classList.remove("selected"));

  button.classList.add("selected");

  selectedRegion = region;
  selectedSubRegion = null;
  currentSessionSite = "";

  document.getElementById("mapRegionName").innerText =
    region;

  document.getElementById("state").innerText =
    "SITE SELECTION";

  document.getElementById("currentSite").innerText =
    region + " / Select sub-region";

  document.getElementById("completeButton").disabled =
    true;

  document.getElementById("confirmation")
    .classList.remove("visible");

  resetSelectionStyle();
  applyHistoryStyle();

  document.getElementById("rotationCheck")
    .classList.remove("warning");

  document.getElementById("rotationMessage").innerText =
    "세부 위치를 선택하면 최근 사용 이력과 비교합니다.";
}


// =====================================================
// Sub-region selection
// =====================================================

function selectSubRegion(index) {

  if (selectedRegion === "") {

    alert("먼저 Injection Region을 선택해주세요.");
    return;
  }

  selectedSubRegion = index;

  currentSessionSite =
    selectedRegion + " / " + getRegionCode() + index;

  resetSelectionStyle();
  applyHistoryStyle();

  const cell = document.querySelector(
    '.site-cell[data-index="' + index + '"]'
  );

  cell.classList.add("selected");

  document.getElementById("currentSite").innerText =
    currentSessionSite;

  document.getElementById("state").innerText =
    "ROTATION CHECK";

  document.getElementById("completeButton").disabled =
    false;

  runRotationCheck();
}


// =====================================================
// Region code
// =====================================================

function getRegionCode() {

  if (selectedRegion === "Abdomen") {
    return "A";
  }

  if (selectedRegion === "Left Thigh") {
    return "LT";
  }

  if (selectedRegion === "Right Thigh") {
    return "RT";
  }

  if (selectedRegion === "Upper Arm") {
    return "UA";
  }

  return "S";
}


// =====================================================
// Rotation Check
// =====================================================

function runRotationCheck() {

  const checkBox =
    document.getElementById("rotationCheck");

  const message =
    document.getElementById("rotationMessage");

  const recentHistory =
    historyList.slice(0, RECENT_WINDOW);

  const usedRecently =
    recentHistory.includes(currentSessionSite);

  if (usedRecently) {

    checkBox.classList.add("warning");

    message.innerHTML =
      "<strong>Recent site detected.</strong><br>" +
      "이 세부 영역은 최근 사용 이력에 포함되어 있습니다. " +
      "선택한 프로토콜의 rotation 기준을 확인하고 " +
      "다른 위치를 고려할 수 있습니다.";

  } else {

    checkBox.classList.remove("warning");

    message.innerHTML =
      "<strong>No recent match.</strong><br>" +
      "현재 선택은 최근 " +
      RECENT_WINDOW +
      "개의 확인된 이력과 동일하지 않습니다.";

  }
}


// =====================================================
// Demo session completion
// =====================================================

function completeDemoSession() {

  if (currentSessionSite === "") {
    return;
  }

  document.getElementById("state").innerText =
    "SESSION COMPLETE";

  document.getElementById("confirmSite").innerText =
    currentSessionSite;

  document.getElementById("confirmation")
    .classList.add("visible");
}


// =====================================================
// Confirm / Correct
// =====================================================

function confirmSite() {

  if (currentSessionSite === "") {
    return;
  }

  // Commit only after confirmation
  historyList.unshift(currentSessionSite);

  // Keep only recent 10 sessions in prototype
  if (historyList.length > 10) {
    historyList.pop();
  }

  document.getElementById("state").innerText =
    "RESULT";

  document.getElementById("confirmation")
    .classList.remove("visible");

  renderHistory();

  resetSelectionStyle();
  applyHistoryStyle();

  // Current session reset
  selectedSubRegion = null;
  currentSessionSite = "";

  document.getElementById("currentSite").innerText =
    "Session recorded";

  document.getElementById("completeButton").disabled =
    true;
}


function correctSite() {

  document.getElementById("state").innerText =
    "SITE CORRECTION";

  document.getElementById("confirmation")
    .classList.remove("visible");

  document.getElementById("rotationMessage").innerText =
    "실제 수행한 위치에 맞게 다시 선택해주세요.";
}


// =====================================================
// Visual history map
// =====================================================

function resetSelectionStyle() {

  document
    .querySelectorAll(".site-cell")
    .forEach(cell => {

      cell.classList.remove(
        "selected",
        "recent-1",
        "recent-2",
        "recent-3"
      );

    });
}


function applyHistoryStyle() {

  if (selectedRegion === "") {
    return;
  }

  const regionCode = getRegionCode();

  // Apply style to the 3 most recent positions
  const recent = historyList.slice(0, 3);

  recent.forEach((site, order) => {

    const prefix =
      selectedRegion + " / " + regionCode;

    if (site.startsWith(prefix)) {

      const indexText =
        site.substring(prefix.length);

      const index =
        parseInt(indexText);

      const cell = document.querySelector(
        '.site-cell[data-index="' + index + '"]'
      );

      if (cell) {

        cell.classList.add(
          "recent-" + (order + 1)
        );

      }
    }

  });
}


// =====================================================
// History list
// =====================================================

function renderHistory() {

  const container =
    document.getElementById("historyContainer");

  if (historyList.length === 0) {

    container.innerHTML =
      '<div class="history-empty">' +
      'No confirmed history' +
      '</div>';

    return;
  }

  let html =
    '<div class="history-list">';

  historyList.forEach((site, index) => {

    html +=
      '<div class="history-item">' +

        '<div class="history-index">' +
          String(index + 1).padStart(2, "0") +
        '</div>' +

        '<div class="history-site">' +
          site +
        '</div>' +

        '<div class="history-status">' +
          (index === 0 ? "MOST RECENT" : "CONFIRMED") +
        '</div>' +

      '</div>';

  });

  html += '</div>';

  container.innerHTML = html;
}

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
  Serial.println("Starting KOKCHI Rotation Web v2...");

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