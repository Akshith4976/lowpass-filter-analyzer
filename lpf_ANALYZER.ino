#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = " SSID";
const char* pass = "PASSWORDs";

ESP8266WebServer server(80);

#define ADCPIN A0
#define SMRATE 4000
#define SMPS 256

float lastdb = -60.0;
float lastfreq = 0.0;

float getdb() {

  int dly = 1000000 / SMRATE;

  int smp[SMPS];

  long sum = 0;

  for (int i = 0; i < SMPS; i++) {

    smp[i] = analogRead(ADCPIN);

    sum += smp[i];

    delayMicroseconds(dly);
  }

  float offs = (float)sum / SMPS;

  float sq = 0;

  for (int i = 0; i < SMPS; i++) {

    float v = smp[i] - offs;

    sq += v * v;
  }

  float rms = sqrt(sq / SMPS);

  if (rms < 1.0) rms = 1.0;

  float db = 20.0 * log10(rms / 511.5);

  return db;
}

void measure() {

  if (server.hasArg("freq")) {

    lastfreq = server.arg("freq").toFloat();
  }

  delay(80);

  float acc = 0;

  for (int i = 0; i < 3; i++) {

    acc += getdb();

    delay(10);
  }

  lastdb = acc / 3.0;

  String json =
  "{\"freq\":" + String(lastfreq, 1) +
  ",\"db\":" + String(lastdb, 2) + "}";

  server.send(200, "application/json", json);
}

void home() {

String page = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width, initial-scale=1.0">

<title>filter analyzer</title>

<style>

body {
  font-family: Arial;
  background: #fff;
  color: #111;
  margin: 20px;
}

.main {
  max-width: 1200px;
  margin: auto;
}

.top {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 20px;
}

.side {
  width: 220px;
  text-align: right;
}

.side h1 {
  font-size: 28px;
  margin: 0;
  font-weight: normal;
}

.side p {
  margin-top: 5px;
  color: #666;
}

.ctrl {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-bottom: 20px;
}

.ctrl div {
  display: flex;
  flex-direction: column;
}

label {
  font-size: 14px;
  margin-bottom: 4px;
}

input[type=number] {
  padding: 6px;
  width: 120px;
  font-size: 14px;
}

button {
  padding: 8px 14px;
  background: #f0f0f0;
  border: 1px solid #777;
  cursor: pointer;
  font-size: 14px;
  margin-right: 10px;
}

button:hover {
  background: #ddd;
}

canvas {
  width: 100%;
  height: 550px;
  border: 1px solid #999;
  background: #fff;
}

.read {
  margin-bottom: 15px;
  font-size: 15px;
}

</style>

</head>

<body>

<div class="main">

<div class="top">

<div>

<h2>low pass filter analyzer</h2>

<div class="ctrl">

<div>
<label>start freq</label>
<input type="number" id="stf" value="20">
</div>

<div>
<label>end freq</label>
<input type="number" id="enf" value="2000">
</div>

<div>
<label>steps</label>
<input type="number" id="stp" value="40">
</div>

<div>
<label>ref db</label>
<input type="number" id="rdb" value="0" step="0.1" oninput="draw()" onchange="draw()">
</div>

</div>

<button onclick="sweep()">start sweep</button>

<button onclick="clr()">clear graph</button>

</div>

<div class="side">

<h1 id="ctxt">---</h1>

<p>-3 db cutoff</p>

</div>

</div>

<div class="read">

frequency: <span id="fdisp">---</span> hz

|

level: <span id="ddisp">---</span> db

</div>

<canvas id="g"></canvas>

</div>

<script>

var pts = [];

var going = false;

var actx = null;

var osc = null;

var gn = null;

var cv = document.getElementById("g");

var cx = cv.getContext("2d");

function resize() {

  cv.width = cv.offsetWidth;

  cv.height = cv.offsetHeight;

  draw();
}

window.addEventListener("resize", resize);

setTimeout(resize, 100);

function draw() {

  var W = cv.width;

  var H = cv.height;

  var pl = 60;

  var pr = 20;

  var pt = 20;

  var pb = 60;

  var pw = W - pl - pr;

  var ph = H - pt - pb;

  cx.clearRect(0, 0, W, H);

  cx.fillStyle = "#fff";

  cx.fillRect(0, 0, W, H);

  var sf = parseFloat(document.getElementById("stf").value);

  var ef = parseFloat(document.getElementById("enf").value);

  var dmin = -60;

  var dmax = 3;

  function fx(f) {

    return pl + (Math.log10(f / sf) / Math.log10(ef / sf)) * pw;
  }

  function fy(d) {

    return pt + ph - ((d - dmin) / (dmax - dmin)) * ph;
  }

  cx.strokeStyle = "#ddd";

  var dbt = [-60, -50, -40, -30, -20, -10, 0, 3];

  for (var i = 0; i < dbt.length; i++) {

    var y = fy(dbt[i]);

    cx.beginPath();

    cx.moveTo(pl, y);

    cx.lineTo(pl + pw, y);

    cx.stroke();

    cx.fillStyle = "#000";

    cx.textAlign = "left";

    cx.fillText(dbt[i] + " db", 10, y + 4);
  }

  var ftk = [20, 50, 100, 200, 500, 1000, 2000, 5000];

  for (var j = 0; j < ftk.length; j++) {

    var f = ftk[j];

    if (f < sf || f > ef) continue;

    var x = fx(f);

    cx.beginPath();

    cx.moveTo(x, pt);

    cx.lineTo(x, pt + ph);

    cx.stroke();

    cx.fillStyle = "#000";

    cx.textAlign = "center";

    if (f >= 1000) {

      cx.fillText((f / 1000) + "k", x, H - 25);

    } else {

      cx.fillText(f, x, H - 25);
    }
  }

  cx.strokeStyle = "#000";

  cx.strokeRect(pl, pt, pw, ph);

  cx.fillStyle = "#000";

  cx.textAlign = "center";

  cx.fillText("frequency (hz)", pl + pw / 2, H - 5);

  cx.save();

  cx.translate(20, pt + ph / 2);

  cx.rotate(-Math.PI / 2);

  cx.fillText("level (db)", 0, 0);

  cx.restore();

  var ref = parseFloat(document.getElementById("rdb").value);

  var cut = ref - 3;

  var ry = fy(ref);

  var cy2 = fy(cut);

  cx.strokeStyle = "#0055ff";

  cx.setLineDash([4, 4]);

  cx.lineWidth = 1;

  cx.beginPath();

  cx.moveTo(pl, ry);

  cx.lineTo(pl + pw, ry);

  cx.stroke();

  cx.setLineDash([]);

  cx.fillStyle = "#0055ff";

  cx.textAlign = "right";

  cx.fillText("ref: " + ref.toFixed(1) + " db", pl + pw - 4, ry - 5);

  cx.strokeStyle = "#ff0000";

  cx.setLineDash([6, 6]);

  cx.lineWidth = 1;

  cx.beginPath();

  cx.moveTo(pl, cy2);

  cx.lineTo(pl + pw, cy2);

  cx.stroke();

  cx.setLineDash([]);

  cx.fillStyle = "#ff0000";

  cx.textAlign = "center";

  cx.fillText("-3 db", pl + 20, cy2 - 5);

  if (pts.length < 2) return;

  cx.strokeStyle = "#000";

  cx.lineWidth = 2;

  cx.beginPath();

  for (var k = 0; k < pts.length; k++) {

    var p = pts[k];

    var x = fx(p.f);

    var y = fy(p.d);

    if (k == 0) cx.moveTo(x, y);

    else cx.lineTo(x, y);
  }

  cx.stroke();

  cx.lineWidth = 1;

  for (var n = 0; n < pts.length; n++) {

    var pp = pts[n];

    cx.beginPath();

    cx.arc(fx(pp.f), fy(pp.d), 3, 0, Math.PI * 2);

    cx.fillStyle = "#000";

    cx.fill();
  }

  var found = false;

  for (var q = 1; q < pts.length; q++) {

    if (pts[q].d <= cut) {

      var f1 = pts[q - 1].f, d1 = pts[q - 1].d;

      var f2 = pts[q].f, d2 = pts[q].d;

      var t = (cut - d1) / (d2 - d1);

      var cf = f1 * Math.pow(f2 / f1, t);

      var cvx = fx(cf);

      cx.strokeStyle = "#ff0000";

      cx.lineWidth = 1;

      cx.beginPath();

      cx.moveTo(cvx, pt);

      cx.lineTo(cvx, pt + ph);

      cx.stroke();

      cx.beginPath();

      cx.arc(cvx, fy(cut), 5, 0, Math.PI * 2);

      cx.fillStyle = "#ff0000";

      cx.fill();

      document.getElementById("ctxt").textContent = cf.toFixed(0) + " hz";

      found = true;

      break;
    }
  }

  if (!found) {

    document.getElementById("ctxt").textContent = "---";
  }
}

function startaudio(f) {

  if (!actx) {

    actx = new (window.AudioContext || window.webkitAudioContext)();

    gn = actx.createGain();

    gn.gain.value = 0.5;

    gn.connect(actx.destination);
  }

  osc = actx.createOscillator();

  osc.type = "sine";

  osc.frequency.value = f;

  osc.connect(gn);

  osc.start();
}

function setf(f) {

  osc.frequency.setValueAtTime(f, actx.currentTime);
}

function stopaudio() {

  if (osc) {

    osc.stop();

    osc.disconnect();

    osc = null;
  }
}

function wait(ms) {

  return new Promise(resolve => setTimeout(resolve, ms));
}

async function sweep() {

  if (going) {

    going = false;

    stopaudio();

    return;
  }

  going = true;

  var sf = parseFloat(document.getElementById("stf").value);

  var ef = parseFloat(document.getElementById("enf").value);

  var ns = parseInt(document.getElementById("stp").value);

  var flist = [];

  for (var i = 0; i < ns; i++) {

    flist.push(sf * Math.pow(ef / sf, i / (ns - 1)));
  }

  startaudio(flist[0]);

  pts = [];

  document.getElementById("ctxt").textContent = "---";

  for (var i = 0; i < flist.length; i++) {

    if (!going) break;

    var fr = flist[i];

    setf(fr);

    document.getElementById("fdisp").textContent = fr.toFixed(1);

    await wait(80);

    try {

      var res = await fetch("/measure?freq=" + fr.toFixed(1));

      var dat = await res.json();

      document.getElementById("ddisp").textContent = dat.db.toFixed(1);

      pts.push({ f: dat.freq, d: dat.db });

      pts.sort((a, b) => a.f - b.f);

      draw();

    } catch (e) {

      console.log(e);
    }
  }

  stopaudio();

  going = false;
}

function clr() {

  pts = [];

  document.getElementById("fdisp").textContent = "---";

  document.getElementById("ddisp").textContent = "---";

  document.getElementById("ctxt").textContent = "---";

  draw();
}

draw();

</script>

</body>

</html>

)rawliteral";

  server.send(200, "text/html", page);
}

void setup() {

  Serial.begin(115200);

  delay(100);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
  }

  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, home);

  server.on("/measure", HTTP_GET, measure);

  server.begin();
}

void loop() {

  server.handleClient();

  yield();
}
