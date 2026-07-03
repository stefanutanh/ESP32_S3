#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>
#include <algorithm>

const char* ssid = "ssid";
const char* password = "password";

WebServer server(80);

const int ledPinRed = 17; 
const int ledPinGreen = 18;

int bleScanTime = 3; 
BLEScan* pBLEScan;
String bleResultsHTML = "";
String wifiResultsHTML = "";

// Struktur för att kunna sortera Bluetooth-enheter på signalstyrka
struct BleDeviceFound {
  String name;
  String address;
  int rssi;
};
std::vector<BleDeviceFound> bleList;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (advertisedDevice.haveName()) { // Spara bara enheter som har ett namn
        BleDeviceFound dev;
        dev.name = String(advertisedDevice.getName().c_str());
        dev.address = String(advertisedDevice.getAddress().toString().c_str());
        dev.rssi = advertisedDevice.getRSSI();
        bleList.push_back(dev);
      }
    }
};

// Sorteringsfunktion (Högst RSSI först)
bool compareRSSI(BleDeviceFound a, BleDeviceFound b) {
  return a.rssi > b.rssi;
}

void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name='viewport' content='width=device-width, initial-scale=1' charset='utf-8'>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; margin-top: 30px; background-color: #f4f4f9; color: #333; }";
  html += ".container { display: inline-block; margin: 10px; padding: 15px; border: 1px solid #ccc; border-radius: 10px; background-color: #fff; width: 280px; }";
  html += ".button { display: inline-block; padding: 12px 20px; font-size: 16px; cursor: pointer; text-align: center; text-decoration: none; color: #fff; border: none; border-radius: 10px; margin: 5px; }";
  html += ".on { background-color: #4CAF50; } .off { background-color: #f44336; }";
  html += ".btn-ble { background-color: #008CBA; } .btn-wifi { background-color: #8E44AD; }";
  html += ".results { text-align: left; max-width: 650px; margin: 20px auto; padding: 20px; background: white; border-radius: 10px; border: 1px solid #ccc; box-shadow: 0px 4px 6px rgba(0,0,0,0.05); }";
  html += "ul { padding-left: 20px; } li { margin-bottom: 8px; }";
  html += "</style></head>";
  html += "<body><h1>ESP32-S3 Kontrollcenter</h1>";
  
  // LED-kontroller
  html += "<div class='container'><h2>Röd Diod</h2><a href='/red/on'><button class='button on'>TÄND</button></a><a href='/red/off'><button class='button off'>SLÄCK</button></a></div>";
  html += "<div class='container'><h2>Grön Diod</h2><a href='/green/on'><button class='button on'>TÄND</button></a><a href='/green/off'><button class='button off'>SLÄCK</button></a></div>";
  
  // Skanningsknappar
  html += "<br><br>";
  html += "<a href='/scan/ble'><button class='button btn-ble'>SÖK BLUETOOTH (NÄRMAST FÖRST)</button></a>";
  html += "<a href='/scan/wifi'><button class='button btn-wifi'>SÖK WI-FI NÄTVERK</button></a>";
  
  // Visa Bluetooth-resultat
  if (bleResultsHTML != "") {
    html += "<div class='results'><h2>Hittade Bluetooth-enheter:</h2><ul>" + bleResultsHTML + "</ul></div>";
  }

  // Visa Wi-Fi-resultat
  if (wifiResultsHTML != "") {
    html += "<div class='results'><h2>Hittade Wi-Fi-nätverk:</h2><ul>" + wifiResultsHTML + "</ul></div>";
  }
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleBleScan() {
  Serial.println("Startar Bluetooth-skanning...");
  bleList.clear();
  bleResultsHTML = "";
  
  BLEScanResults* foundDevices = pBLEScan->start(bleScanTime, false);
  
  // Sortera listan så närmaste enhet hamnar överst
  std::sort(bleList.begin(), bleList.end(), compareRSSI);
  
  // Bygg HTML från den sorterade listan
  for (const auto& dev : bleList) {
    bleResultsHTML += "<li><strong>" + dev.name + "</strong> (RSSI: " + String(dev.rssi) + " dBm)<br><small>Adress: " + dev.address + "</small></li>";
  }
  
  if (bleList.empty()) {
    bleResultsHTML = "<li>Inga namngivna enheter hittades i närheten.</li>";
  }

  pBLEScan->clearResults();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleWifiScan() {
  Serial.println("Startar Wi-Fi-skanning...");
  wifiResultsHTML = "";
  
  // WiFi.scanNetworks returnerar antalet hittade nätverk
  int n = WiFi.scanNetworks();
  Serial.print("Wi-Fi skanning klar. Hittade: ");
  Serial.println(n);
  
  if (n == 0) {
    wifiResultsHTML = "<li>Inga Wi-Fi-nätverk hittades.</li>";
  } else {
    for (int i = 0; i < n; ++i) {
      String encryptionType = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Öppet" : "Låst";
      wifiResultsHTML += "<li><strong>" + WiFi.SSID(i) + "</strong> (" + String(WiFi.RSSI(i)) + " dBm) - <em>" + encryptionType + "</em></li>";
    }
  }
  
  WiFi.scanDelete(); // Rensa skanningsresultatet ur minnet
  server.sendHeader("Location", "/");
  server.send(303);
}

// LED-funktioner
void handleRedOn() { digitalWrite(ledPinRed, HIGH); server.sendHeader("Location", "/"); server.send(303); }
void handleRedOff() { digitalWrite(ledPinRed, LOW); server.sendHeader("Location", "/"); server.send(303); }
void handleGreenOn() { digitalWrite(ledPinGreen, HIGH); server.sendHeader("Location", "/"); server.send(303); }
void handleGreenOff() { digitalWrite(ledPinGreen, LOW); server.sendHeader("Location", "/"); server.send(303); }

void setup() {
  Serial.begin(115200);
  
  pinMode(ledPinRed, OUTPUT);
  pinMode(ledPinGreen, OUTPUT);

  // Initiera Bluetooth BLE
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // Starta Wi-Fi i station mode (så den både kan ansluta och skanna)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  server.on("/", handleRoot);
  server.on("/red/on", handleRedOn);
  server.on("/red/off", handleRedOff);
  server.on("/green/on", handleGreenOn);
  server.on("/green/off", handleGreenOff);
  server.on("/scan/ble", handleBleScan);
  server.on("/scan/wifi", handleWifiScan);
  
  server.begin();
}

void loop() {
  server.handleClient();
}
