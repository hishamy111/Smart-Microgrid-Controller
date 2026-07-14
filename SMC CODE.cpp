#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <INA226_WE.h>

// ======================
// Access Point
// ======================

const char* ssid = "Smart_Microgrid";
const char* password = "12345678";

WebServer server(80);

// ======================
// INA219
// ======================

#define SOLAR_ADDR 0x40
#define WIND_ADDR  0x41

INA226_WE solarINA = INA226_WE(SOLAR_ADDR);
INA226_WE windINA  = INA226_WE(WIND_ADDR);

// ======================
// Pins
// ======================

#define RELAY_SOLAR   25
#define RELAY_WIND    26
#define RELAY_BATTERY 27
#define RELAY_CHARGE  33

#define BATTERY_PIN   35

// ======================
// Variables
// ======================

float solarVoltage = 0;
float solarCurrent = 0;
float solarPower   = 0;

float windVoltage = 0;
float windCurrent = 0;
float windPower   = 0;

float batteryVoltage = 0;
float batteryPercent = 0;

String statusText = "System Ready";

bool chargeEnabled = false;

String activeSource = "NONE";

bool autoMode = false;

unsigned long chargeTimer = 0;
bool chargeWaiting = false;
// ======================
// Battery Divider
// ======================

const float dividerRatio = 11.0;
const float ADC_REF = 3.3;
const float ADC_MAX = 4095.0;

// ======================
// Read Sensors
// ======================

void readSensors()
{
  solarVoltage = solarINA.getBusVoltage_V();

 solarCurrent = solarINA.getCurrent_mA()/1000.0;

 solarPower = solarINA.getBusPower();

  windVoltage = windINA.getBusVoltage_V();

 windCurrent = windINA.getCurrent_mA()/1000.0;

 windPower = windINA.getBusPower();

  int raw = analogRead(BATTERY_PIN);

  float adcVoltage =
      (raw * ADC_REF) / ADC_MAX;

  batteryVoltage =
      adcVoltage * dividerRatio;

  batteryPercent =
   ((batteryVoltage - 6.0) / (8.4 - 6.0)) * 100.0;

   batteryPercent = constrain(batteryPercent, 0, 100);
}

void allSourcesOff()
{
  digitalWrite(RELAY_SOLAR, HIGH);
  digitalWrite(RELAY_WIND, HIGH);
  digitalWrite(RELAY_BATTERY, HIGH);
}

void selectSolar()
{
    allSourcesOff();
    digitalWrite(RELAY_SOLAR, LOW);

    activeSource = "SOLAR";
    statusText = "Running on Solar";
}

void selectWind()
{
    allSourcesOff();
    digitalWrite(RELAY_WIND, LOW);

    activeSource = "WIND";
    statusText = "Running on Wind";
}

void selectBattery()
{
  allSourcesOff();
  digitalWrite(RELAY_BATTERY, LOW);

  activeSource = "BATTERY";
  statusText = "Running on Battery";
}

void handleBattery()
{
    autoMode = false;

    chargeWaiting = false;

    chargeOff();

    selectBattery();

    statusText = "Manual Override - Battery";

    server.sendHeader("Location","/");
    server.send(303);
}

void chargeOn()
{
    if(activeSource == "BATTERY")
    {
        statusText = "Cannot Charge From Battery";
        return;
    }

    digitalWrite(RELAY_CHARGE, LOW);

    chargeEnabled = true;

    statusText = "Charging Enabled";
}

void chargeOff()
{
  digitalWrite(RELAY_CHARGE, HIGH);

  chargeEnabled = false;
  statusText = "Charging Disabled";
}

void handleSolar()
{
  autoMode = false;

  selectSolar();

  statusText = "Manual Override - Solar";

  server.sendHeader("Location","/");
  server.send(303);
}

void handleWind()
{
  autoMode = false;

  selectWind();

  statusText = "Manual Override - Wind";

  server.sendHeader("Location","/");
  server.send(303);
}


void handleCharge()
{
  if(chargeEnabled)
    chargeOff();
  else
    chargeOn();

  server.sendHeader("Location","/");
  server.send(303);
}

void handleAuto()
{
  autoMode = !autoMode;

  if(autoMode)
    statusText = "Auto Mode Enabled";
  else
    statusText = "Manual Mode Enabled";

  server.sendHeader("Location","/");
  server.send(303);
}

// ======================
// Dashboard
// ======================

String webpage()
{
  readSensors();

  String page = R"====(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="2">

<style>

body{
    margin:0;
    padding:15px;
    background:#d9d9d9;
    font-family:Arial,Helvetica,sans-serif;
    text-align:center;
}

.container{
    max-width:420px;
    margin:auto;
}

button{

    width:100%;
    height:58px;

    margin-top:8px;
    margin-bottom:8px;

    background:#ffffff;

    color:#000000;

    border:3px solid #666666;

    border-radius:12px;

    font-size:18px;

    font-weight:bold;

}

.active{

    border:3px solid #00bb00;

}

.row{

    display:flex;

    gap:10px;

}

.row form{

    width:100%;

}

.status{

    background:#ffffff;

    border:2px solid #666666;

    border-radius:12px;

    margin-top:12px;

    padding:15px;

    font-size:18px;

    font-weight:bold;

}

</style>

</head>

<body>

<div class="container">

)====";


//===========================
// Solar Button
//===========================

page += "<form action='/solar'>";

page += "<button class='";

if(activeSource=="SOLAR")
    page += "active";

page += "'>";

page += "Solar&nbsp;&nbsp;|&nbsp;&nbsp;";

page += String(solarVoltage,1);

page += " V&nbsp;&nbsp;&nbsp;";

page += String(solarCurrent,2);

page += " A&nbsp;&nbsp;&nbsp;";

page += String(solarPower,1);

page += " W";

page += "</button>";

page += "</form>";


//===========================
// Wind Button
//===========================

page += "<form action='/wind'>";

page += "<button class='";

if(activeSource=="WIND")
    page += "active";

page += "'>";

page += "Wind&nbsp;&nbsp;|&nbsp;&nbsp;";

page += String(windVoltage,1);

page += " V&nbsp;&nbsp;&nbsp;";

page += String(windCurrent,2);

page += " A&nbsp;&nbsp;&nbsp;";

page += String(windPower,1);

page += " W";

page += "</button>";

page += "</form>";


//===========================
// Battery Button
//===========================

page += "<form action='/battery'>";

page += "<button class='";

if(activeSource=="BATTERY")
    page += "active";

page += "'>";

page += "Battery&nbsp;&nbsp;|&nbsp;&nbsp;";

page += String(batteryVoltage,1);

page += " V&nbsp;&nbsp;&nbsp;";

page += String(batteryPercent,0);

page += " %";

page += "</button>";

page += "</form>";

//===========================
// Charging + Auto Buttons
//===========================

page += "<div class='row'>";

// Charging Button
page += "<form action='/charge'>";

page += "<button class='";

if(chargeEnabled)
    page += "active";

page += "'>";

page += "Charging";

page += "</button>";

page += "</form>";

// Auto Mode Button
page += "<form action='/auto'>";

page += "<button class='";

if(autoMode)
    page += "active";

page += "'>";

page += "Auto Mode";

page += "</button>";

page += "</form>";

page += "</div>";


//===========================
// Status Box
//===========================

page += "<div class='status'>";

page += "Status<br><br>";

page += statusText;

page += "<br><br>";

page += "Source : ";

page += activeSource;

page += "<br><br>";

page += "Charging : ";

if(chargeEnabled)
    page += "ON";
else
    page += "OFF";

page += "<br><br>";

page += "Mode : ";

if(autoMode)
    page += "AUTO";
else
    page += "MANUAL";

page += "</div>";

page += R"====(

</div>

</body>
</html>

)====";

return page;
}

// ======================
// Main Page
// ======================

void handleRoot()
{
  readSensors();
  server.send(200,
  "text/html",
  webpage());
}

void autoControl()
{
  if(!autoMode)
    return;

  readSensors();

  // Turn charging off immediately if the active source collapses

if(activeSource=="SOLAR" && solarVoltage<=6)
{
    chargeWaiting=false;
    chargeOff();
}

if(activeSource=="WIND" && windVoltage<=6)
{
    chargeWaiting=false;
    chargeOff();
}

  // ---------- Select Source ----------

// =======================
// =======================
// Currently using SOLAR
// =======================

if(activeSource == "SOLAR")
{
    // Wind is much stronger
    if(windVoltage >= 19)
    {
        chargeOff();
        chargeWaiting = false;

        selectWind();
    }

    // Solar failed
    else if(solarVoltage <= 6)
    {
        chargeOff();
        chargeWaiting = false;

        if(windVoltage >= 16)
        {
            selectWind();
        }
        else if(batteryVoltage > 7)
        {
            selectBattery();
        }
        else
        {
            allSourcesOff();
            activeSource="NONE";
            statusText="No Source Available";
        }
    }
}

// =======================
// Currently using WIND
// =======================
else if(activeSource == "WIND")
{
    // Keep using Wind until it fails

    if(windVoltage <= 6)
    {
        chargeOff();

        if(solarVoltage >= 16)
        {
            selectSolar();
        }
        else if(batteryVoltage > 7)
        {
            selectBattery();
        }
        else
        {
            allSourcesOff();
            activeSource = "NONE";
            statusText = "No Source Available";
        }
    }
}

// =======================
// Currently using BATTERY
// =======================
else if(activeSource == "BATTERY")
{
    if(solarVoltage >= 16 && windVoltage >= 16)
    {
        if(solarVoltage >= windVoltage)
            selectSolar();
        else
            selectWind();
    }
    else if(solarVoltage >= 16)
    {
        selectSolar();
    }
    else if(windVoltage >= 16)
    {
        selectWind();
    }
}

// =======================
// NONE
// =======================
else
{
    if(solarVoltage >= 16)
        selectSolar();

    else if(windVoltage >= 16)
        selectWind();

    else if(batteryVoltage > 7)
        selectBattery();

    else
    {
        allSourcesOff();
        statusText = "No Source Available";
    }
}

  // ---------- Charging ----------

 // Charging only when the active source is healthy

if(activeSource=="SOLAR")
{
    if(solarVoltage > 6 && batteryVoltage < 9)
    {
        if(!chargeWaiting)
        {
            chargeWaiting=true;
            chargeTimer=millis();
        }

        if(millis()-chargeTimer>=2000)
            chargeOn();
    }
    else
    {
        chargeWaiting=false;
        chargeOff();
    }
}

else if(activeSource=="WIND")
{
    if(windVoltage > 6 && batteryVoltage < 9)
    {
        if(!chargeWaiting)
        {
            chargeWaiting=true;
            chargeTimer=millis();
        }

        if(millis()-chargeTimer>=2000)
            chargeOn();
    }
    else
    {
        chargeWaiting=false;
        chargeOff();
    }
}

else
{
    chargeWaiting=false;
    chargeOff();
}

}   

// ======================
// Setup

// ======================
// Setup
// ======================

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_SOLAR,OUTPUT);
  pinMode(RELAY_WIND,OUTPUT);
  pinMode(RELAY_BATTERY,OUTPUT);
  pinMode(RELAY_CHARGE,OUTPUT);

  digitalWrite(RELAY_SOLAR,HIGH);
  digitalWrite(RELAY_WIND,HIGH);
  digitalWrite(RELAY_BATTERY,HIGH);
  digitalWrite(RELAY_CHARGE,HIGH);

  Wire.begin();

  if(!solarINA.init())
{
  Serial.println("Solar INA226 not connected");
}

if(!windINA.init())
{
  Serial.println("Wind INA226 not connected");
}

 solarINA.setAverage(INA226_AVERAGE_16);
windINA.setAverage(INA226_AVERAGE_16);

solarINA.setConversionTime(INA226_CONV_TIME_1100);
windINA.setConversionTime(INA226_CONV_TIME_1100);

  WiFi.softAP(ssid,password);

  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);

  server.on("/solar", handleSolar);

  server.on("/wind", handleWind);

  server.on("/battery", handleBattery);

  server.on("/charge", handleCharge);

  server.on("/auto", handleAuto);

  server.begin();

  statusText = "Dashboard Online";
}

void loop()
{
  server.handleClient();

  autoControl();

  delay(200);
}