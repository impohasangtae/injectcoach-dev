#include <WiFi.h>
#include <WebServer.h>

// ==============================
// Wi-Fi SoftAP
// ==============================
const char* AP_SSID = "InjectCoach-Test";
const char* AP_PASSWORD = "12345678";

WebServer server(80);

// ==============================
// Web Page
// ==============================
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">

  <title>KOKCHI Dashboard</title>

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

    .header {
      margin-bottom: 24px;
    }

    .brand {
      font-size: 34px;
      font-weight: 800;
      letter-spacing: 2px;
    }

    .subtitle {
      margin-top: 5px;
      color: #68717d;
      font-size: 14px;
    }

    .card {
      background: white;
      border-radius: 18px;
      padding: 22px;
      margin-bottom: 16px;
      box-shadow: 0 5px 20px rgba(0,0,0,0.06);
    }

    .card-title {
      font-size: 14px;
      color: #6d7580;
      font-weight: 700;
      margin-bottom: 12px;
    }

    .state {
      font-size: 30px;
      font-weight: 800;
    }

    .profile {
      display: inline-block;
      background: #eef1f5;
      border-radius: 12px;
      padding: 10px 15px;
      font-weight: 700;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 12px;
    }

    .sensor {
      padding: 16px;
      border: 1px solid #e5e8ec;
      border-radius: 14px;
    }

    .sensor-name {
      font-size: 13px;
      color: #737b85;
      margin-bottom: 7px;
    }

    .sensor-value {
      font-size: 20px;
      font-weight: 800;
    }

    .site-buttons {
      display: flex;
      flex-wrap: wrap;
      gap: 9px;
      margin-bottom: 15px;
    }

    button {
      border: 1px solid #d7dce2;
      background: white;
      color: #222;
      border-radius: 12px;
      padding: 11px 15px;
      font-size: 14px;
      cursor: pointer;
    }

    button.selected {
      background: #20242a;
      color: white;
      border-color: #20242a;
    }

    .subregions {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 9px;
    }

    .history {
      min-height: 42px;
      padding: 12px;
      background: #f5f6f8;
      border-radius: 12px;
      color: #525a64;
    }

    .result {
      padding: 18px;
      border-radius: 14px;
      background: #f5f6f8;
      font-weight: 800;
      text-align: center;
      font-size: 19px;
    }

    .note {
      font-size: 12px;
      color: #8a919b;
      line-height: 1.5;
      margin-top: 10px;
    }

    @media (max-width: 600px) {
      .grid {
        grid-template-columns: 1fr;
      }

      .subregions {
        grid-template-columns: repeat(2, 1fr);
      }

      .brand {
        font-size: 28px;
      }
    }
  </style>
</head>

<body>

<div class="container">

  <div class="header">
    <div class="brand">KOKCHI</div>
    <div class="subtitle">
      Embedded Self-Injection Support Prototype
    </div>
  </div>

  <div class="card">
    <div class="card-title">CURRENT STATE</div>
    <div class="state" id="state">IDLE</div>
  </div>

  <div class="card">
    <div class="card-title">DEVICE / PROTOCOL PROFILE</div>
    <div class="profile">Demo Profile</div>
    <div class="note">
      Prototype demonstration rule set
    </div>
  </div>

  <div class="card">
    <div class="card-title">INJECTION SITE</div>

    <div class="site-buttons">
      <button onclick="selectRegion(this,'Abdomen')">Abdomen</button>
      <button onclick="selectRegion(this,'Left Thigh')">Left Thigh</button>
      <button onclick="selectRegion(this,'Right Thigh')">Right Thigh</button>
      <button onclick="selectRegion(this,'Upper Arm')">Upper Arm</button>
    </div>

    <div class="card-title">SUB-REGION</div>

    <div class="subregions">
      <button onclick="selectSite(this,'A1')">A1</button>
      <button onclick="selectSite(this,'A2')">A2</button>
      <button onclick="selectSite(this,'A3')">A3</button>
      <button onclick="selectSite(this,'A4')">A4</button>
    </div>
  </div>

  <div class="card">
    <div class="card-title">REAL-TIME STATUS</div>

    <div class="grid">

      <div class="sensor">
        <div class="sensor-name">PAD Contact</div>
        <div class="sensor-value" id="pad">OFF</div>
      </div>

      <div class="sensor">
        <div class="sensor-name">PEN Contact</div>
        <div class="sensor-value" id="pen">OFF</div>
      </div>

      <div class="sensor">
        <div class="sensor-name">Plunger</div>
        <div class="sensor-value" id="plunger">UP</div>
      </div>

      <div class="sensor">
        <div class="sensor-name">Hold Time</div>
        <div class="sensor-value" id="hold">0.0 s</div>
      </div>

      <div class="sensor">
        <div class="sensor-name">Orientation</div>
        <div class="sensor-value" id="orientation">
          CALIBRATION REQUIRED
        </div>
      </div>

      <div class="sensor">
        <div class="sensor-name">Selected Site</div>
        <div class="sensor-value" id="selectedSite">-</div>
      </div>

    </div>
  </div>

  <div class="card">
    <div class="card-title">RECENT SITE HISTORY</div>
    <div class="history" id="history">
      No site history
    </div>
  </div>

  <div class="card">
    <div class="card-title">SESSION RESULT</div>
    <div class="result" id="result">
      Waiting for session
    </div>
  </div>

</div>


<script>

let selectedRegion = "";
let historyList = [];

function selectRegion(button, region) {

  const buttons =
    document.querySelectorAll(".site-buttons button");

  buttons.forEach(b => b.classList.remove("selected"));

  button.classList.add("selected");

  selectedRegion = region;

  document.getElementById("selectedSite").innerText =
    region;

  document.getElementById("state").innerText =
    "SITE SELECTED";
}


function selectSite(button, site) {

  if (selectedRegion === "") {
    alert("먼저 Injection Site를 선택해주세요.");
    return;
  }

  const buttons =
    document.querySelectorAll(".subregions button");

  buttons.forEach(b => b.classList.remove("selected"));

  button.classList.add("selected");

  const fullSite =
    selectedRegion + " / " + site;

  document.getElementById("selectedSite").innerText =
    fullSite;

  document.getElementById("state").innerText =
    "READY";

  historyList.unshift(fullSite);

  if (historyList.length > 5) {
    historyList.pop();
  }

  document.getElementById("history").innerText =
    historyList.join("  →  ");
}

</script>

</body>
</html>
)rawliteral";


// ==============================
// Setup
// ==============================
void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("Starting KOKCHI Web Dashboard...");

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


// ==============================
// Loop
// ==============================
void loop() {

  server.handleClient();

}