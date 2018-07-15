/*
* NodeMCU sketch for ClimateBox project
*
* Maxim Burov
* Innopolis University, 2018 
*/

#include <Arduino.h>
#include <time.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <SPI.h>
#include <SD.h>
#define FS_NO_GLOBALS
#include <FS.h>

#include <DHT.h>

#define HTTP_OK 201
#define SDCARD_PIN D8
#define DHT_PIN D4

// Data for connecting to wi-fi and server
const char* ssid     = "Keenetic-4928";
const char* password = "PpJu9xPA";

String dataHost = "http://192.168.1.44:8000/api/readouts/";
String regHost  = "http://192.168.1.44:8000/api/devices/";

int sleeptime = 5000;
int timezone = 3;

// DHT sensor object
DHT dht(DHT_PIN, DHT22);

boolean wifiConnect() {

  // Time delay to wait wi-fi connection
  int waitTime = 15000;
  int waited = 0;

  Serial.println("Connecting to wi-fi network");

  // Start trying to connect to wi-fi
  WiFi.begin(ssid, password);
  yield();

  while (WiFi.status() != WL_CONNECTED and waited <= waitTime) {
    delay(500);
    waited += 500;
    Serial.print(".");
  }

  Serial.println(WiFi.status());
  // Return status of wi-fi network
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("WiFi network is not available");
    return false;
  }
}

void tryToRegister() {

  Serial.println("Check registratoin");

  SPIFFS.begin();
  if(!SPIFFS.exists("register.txt")) {
    Serial.println("Device is not registered");

    HTTPClient http;
    //String key = getKey();
    String macAdd = WiFi.macAddress();

    http.begin(regHost); 
    http.addHeader("Content-Type", "application/json");
    //String regData = "{\"key\": \"" + key + "\", \"MAC\": \"" + macAdd + "\", \"charge\": \"90\"}"; the correct one for production
    String regData = "{\"key\": \"JASFGH\", \"MAC\": \"" + macAdd + "\", \"charge\": \"90\"}";
    Serial.println(regData);
    int httpCode = http.POST(regData);
    Serial.println(httpCode);
    String responseBody = http.getString();
    Serial.println(responseBody);
    if(httpCode == HTTP_OK) {
      sleeptime = deserializeBody(responseBody);
    }

    http.end();

    //setTime();

  } else {
    Serial.println("Device is registered");
  }

  SPIFFS.end();
}

int deserializeBody(String body) {
  String id = "";
  String sleepTime = "";
  int len = body.length();
  boolean idChars = true;
  for(int i = 0; i < len; i++) {
    if(body[i] != ',') {
      if(body[i] != '\"') {
        if(idChars) id += body[i]; else sleepTime += body[i];
      } 
    } else {
      idChars = !idChars;
    }
  }
  setID(id);
  return sleepTime.toInt();
}

void setID(String id) {
  Serial.println("Setting device ID");
  SPIFFS.begin();
  fs::File regFile = SPIFFS.open("register.txt", "w");
  regFile.print(id);
  regFile.close();
  SPIFFS.end();
}

String getID() {
  Serial.println("Getting device ID");
  String id;
  SPIFFS.begin();
  fs::File regFile = SPIFFS.open("register.txt", "r");
  id = readlnSPIFFS(regFile);
  regFile.close();
  SPIFFS.end();
  return id;
}

/*void setTime() {
  Serial.println("Setting time");
  SPIFFS.begin();
  if(!SPIFFS.exists("time.txt")) {
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("\nWaiting for time");
    while (!time(nullptr)) {
      Serial.print(".");
      delay(1000);
      time_t now = time(nullptr);
      Serial.println(ctime(&now));
    }
  } else {  
  fs::File regFile = SPIFFS.open("register.txt", "w");
  regFile.print(id);
  regFile.close();
  }
  SPIFFS.end();
}

String getTime() {
  Serial.println("Getting device ID");
  String id;
  SPIFFS.begin();
  fs::File regFile = SPIFFS.open("register.txt", "r");
  id = readlnSPIFFS(regFile);
  regFile.close();
  SPIFFS.end();
  return id;
}*/

String getKey() {
  Serial.println("Trying to read key from SD");
  if(SD.begin(SDCARD_PIN)) {
    File keyFile = SD.open("key.txt", FILE_WRITE);
    String key = readlnSD(keyFile);
    keyFile.close();
    return key;
  } 
  return "";
}

boolean sendDataToServer(String data) {

  HTTPClient http;
  int waitTime = 5000;
  int waited = 0;

  Serial.println("[HTTP] Connecting to server...");

  if(http.begin(dataHost)) {
    Serial.println("Connected.");
  } else {
    Serial.println("Not connected.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  int returnCode = http.POST(data);

  while (returnCode != HTTP_OK and waited < waitTime) {
    delay(1000);
    waited += 1000;
    http.begin(dataHost);
    returnCode = http.POST(data);
  }

  Serial.println(returnCode);

  if (returnCode == HTTP_OK) {
    sleeptime = http.getString().toInt();
    Serial.println("Data sent");
    return true;
  }
  else {
    Serial.println("Data was not sent");
    return false;
  }
}

boolean sendOldDataToServer() {
  Serial.println("Sending old data to server");

  String dataBulk = "[";

  // Read from SD card
  if(SD.begin(SDCARD_PIN)) {
    File savedData = SD.open("data.txt", FILE_WRITE);
    while(savedData.available()) {
      dataBulk = dataBulk + readlnSD(savedData) + ", ";
    }
    savedData.close();
  }

  // Read from InMemory
  if(SPIFFS.begin()) {
    fs::File savedData = SPIFFS.open("data.txt", "r");
    if(savedData) {
      while(savedData.available()) {
        dataBulk = dataBulk + readlnSPIFFS(savedData);
        if(savedData.available()) {
          dataBulk = dataBulk + ',';
        }
      }
    }
    savedData.close();
  }
  dataBulk = dataBulk + "]";
  Serial.println("The old data is: ");
  Serial.println(dataBulk);

  return sendDataToServer(dataBulk);
}

char saveDataToSDCard(String data) {
  Serial.println("Trying to save data at SD");
  if (SD.begin(SDCARD_PIN)) {
    Serial.println("SD Card Initialized");
    // Try to open the file
    File savedData = SD.open("data.txt", FILE_WRITE);

    if (savedData) {
      // Write data and return 2 (generateData convention)
      savedData.print(data);
      savedData.close();
      return '2';
    } else {
      // Return 4 if we can't open the file
      return '4';
    }
  } else {
    // Return 3 if we can't open the SD Card
    return '3';
  }
}

void saveDataToInternalMemory(String data) {
  Serial.println("Saving data to InMemory");
  SPIFFS.begin();
  fs::File savedData = SPIFFS.open("data.txt", "a");
  if(savedData) {
    savedData.print(data);
    savedData.close();
  }
  SPIFFS.end();
}

// Mark that we couldnot send data to server
void dataSaved() {
  Serial.println("Marking data was saved");
  SPIFFS.begin();
  fs::File savedData = SPIFFS.open("saved.txt", "w");
  if(savedData) {
    savedData.print("saved");
    savedData.close();
  }
  SPIFFS.end();
}

// Check if we saved data or not
boolean checkWasSaved() {
  Serial.println("Checking was data saved or not");
  boolean res;
  if(SPIFFS.begin()) {
    fs::File savedData = SPIFFS.open("saved.txt", "r");
    delay(50);
    if(savedData) {
      String status = readlnSPIFFS(savedData);  
      Serial.print("Status: ");
      Serial.println(status);
      if (!status.compareTo("saved")) {
        res = true;
      } else {
        res = false;
      }
      savedData.close();
    }
    SPIFFS.end();
    Serial.print("Was saved: ");
    Serial.println(res);
    return res;
  }
}

void formatData(){
  Serial.println("Remove old sent data");
  if(SD.begin(SDCARD_PIN)) {
    SD.remove("data.txt");
    SD.open("data.txt", FILE_WRITE).close();
  }

  SPIFFS.begin();
  fs::File savedData = SPIFFS.open("data.txt", "w");
  savedData.close();
  fs::File savedFile = SPIFFS.open("saved.txt", "w");
  savedFile.close();
  SPIFFS.end();
}

// Read line from file from InMemory
String readlnSPIFFS(fs::File file) {
  Serial.println("reading line from SPIFFS");
  String res = "";
  char curr = ' ';
  while(curr != '\0' and file.available()) {
    curr = file.read();
    res += curr;
  }
  return res; 
}

// Read line from file from SD
String readlnSD(File file) {
  String res = "";
  char curr = ' ';
  while(curr != '\0' and file.available()) {
    curr = file.read();
    res += curr;
  }
  return res;
}

String generateData(String temp, String CO2, String humid, String battery) {
  /*
   * Errors:
   *  0: No errors;
   *  1: Wi-Fi is not available
   *  2: Have no connection with server
   *  3: SD Card is broken or is not available
   *  4: Cannot write the file on the SD Card
   */
  String data = "{\"timestamp\": \"2018-07-08T04:50:23+00:00\", \"device\": \"" + getID() + "\", \"charge\": \"" + battery +"\", \"temp\": \"" + 
  temp + "\", \"humid\": \"" + humid + "\"}";
  return data;
}

String getTemp() {
  Serial.println("Getting temp");
  float tempC = dht.readTemperature();
  return String(tempC);
}
  
String getHumid() {

  float humid = dht.readHumidity();
  return (String)humid;
}

String getBatteryLevel() {

  return "100";
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Start working");
  Serial.println(millis());

  // Variables to keep necessary data
  String temp = "";
  String CO2 = "";
  String humid = "";
  String battery = "";

  // Variables to keep state of board
  boolean wifiConnected = false;
  boolean serverConnected = false;
  boolean dataWasSaved = checkWasSaved();
  char sdStatus;

  // Initialize connection to wi-fi network
  wifiConnected = wifiConnect();
  yield();

  // Check, is it first time launch of box and register it on server
  // and get in response its ID and sleep time
  tryToRegister();

  // Launch the sensors and get values from them
  dht.begin();
  temp = getTemp();
  humid = getHumid();
  battery = getBatteryLevel();

  // Connect with server and send data
  String data = generateData(temp, CO2, humid, battery);
  Serial.println(data);
  if (wifiConnected) {
    if(dataWasSaved) {
      Serial.println("Is going to send old data.");
      serverConnected = sendOldDataToServer();
      if(serverConnected) {
        sendDataToServer(data);
        formatData();
      }
    } else {
      serverConnected = sendDataToServer(data);
    }
  }

  if (!wifiConnected or !serverConnected) {
    sdStatus = saveDataToSDCard(data);
    if(sdStatus > 2) {
      saveDataToInternalMemory(data);
    }
    dataSaved();
  }
  for(int i = 0; i < 10; i++) {
    Serial.println(analogRead(A0));
    delay(100);
  }
  Serial.println("Ended");
  ESP.deepSleep(5000000);

}

void loop() {
}
