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
#define MICROSEC 1000000
#define MILLISEC 1000
#define TIME_TO_WAIT 10000

String dataHostTail = "/api/readouts/";
String regHostTail  = "/api/devices/";

const char* ssid = "Innopolis";
const char* password = "Innopolis";

int sleeptime = 5000000;
int timezone = 3;

// DHT sensor object
DHT dht(DHT_PIN, DHT22);

boolean wifiConnect() {

  // Time delay to wait wi-fi connection
  int waitTime = 15000;
  int waited = 0;

  Serial.println("Connecting to wi-fi network");
  Serial.println(WiFi.macAddress());

  Serial.println(ssid);
  Serial.println(password);
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
  String regHost = "http://" + readFromFile("ip.txt") + "/api/devices/";

  Serial.println("Check registration");
  Serial.println("URL is: " + regHost);

  SPIFFS.begin();
  if(!SPIFFS.exists("register.txt")) {
    Serial.println("Device is not registered");

    HTTPClient http;
    String key = readFromFile("key.txt");
    //String key = getKey();
    String macAdd = WiFi.macAddress();

    http.begin(regHost); 
    http.addHeader("Content-Type", "application/json");
    String regData = "{\"key\": \"" + key + "\", \"MAC\": \"" + macAdd + "\", \"charge\": \"90\"}"; 
    Serial.println(regData);
    int httpCode = http.POST(regData);
    Serial.println(httpCode);
    String responseBody = http.getString();
    Serial.println(responseBody);
    if(httpCode == HTTP_OK) {
      sleeptime = deserializeBody(responseBody);
    }

    http.end();

  } else {
    Serial.println("Device is registered");
  }

  SPIFFS.end();
}

void serialCommands() {
  int timeWaited = 0;
  while(timeWaited < TIME_TO_WAIT) {
      if(Serial.available() > 0) {
        String inputCommand = readlnSerial();
        Serial.println(inputCommand);
        // Format data
        if(inputCommand.equals("format")) {
          formatData();
        }
        // Remove file register.txt with device id
        if(inputCommand.equals("unregister")) {
          SPIFFS.begin();
          SPIFFS.remove("register.txt");
          SPIFFS.end();
          Serial.println("Registration removed");
        }
        // Enter the key to register
        if(inputCommand.equals("key")) {
          while(Serial.available() == 0) {
            delay(500);
          }
          String key = readlnSerial();
          writeInFile("key.txt", key, "w");    
        }
        // Enter WI-FI settings
        if(inputCommand.equals("wifi")) {
          while(Serial.available() == 0) {
            delay(500);
          }
          String ssid = readlnSerial();
          writeInFile("wifi_ssid.txt", ssid, "w");
          while(Serial.available() == 0) {
            delay(500);
          }
          String pass = readlnSerial();
          writeInFile("wifi_password.txt", pass, "w");
        }
        // Enter ip address of server
        if(inputCommand.equals("ip")) {
          while(Serial.available() == 0) {
            delay(500);
          }
          String ip = readlnSerial();
          writeInFile("ip.txt", ip, "w");
        } 
        // Continue working
        if(inputCommand.equals("next")) {
          break;
        }
        timeWaited = 0;
      } else {
        timeWaited += 500;
        delay(500);
      }
  }
}

// Converts string to const char* for using in wi-fi connection
const char* stringToChar(String data) {
  char* tmp = new char[data.length() + 1];
  data.toCharArray(tmp, data.length() + 1);
  Serial.println(data.length() + 1);
  Serial.println(data);
  return tmp;
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
  // Saving ID of the device
  writeInFile("register.txt", id, "w");
  //setID(id);
  return sleepTime.toInt();
}

void writeInFile(String fileName, String data, char* mode) {
  Serial.println("Wrtiting \"" + data + "\" in file (" + fileName + ") with mode" + mode);
  SPIFFS.begin();
  fs::File file = SPIFFS.open(fileName, mode);
  file.print(data);
  file.close();
  SPIFFS.end();
}

String readFromFile(String fileName) {
  Serial.println("Reading from: " + fileName);
  String dataFromFile;
  SPIFFS.begin();
  fs::File file = SPIFFS.open(fileName, "r");
  dataFromFile = readlnSPIFFS(file);
  file.close();
  SPIFFS.end();
  Serial.print("The result is: ");
  Serial.println(dataFromFile);
  return dataFromFile;
}

// Set time from time servers
void setTimeOnline() {
  Serial.println("Setting time");
  configTime(timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now;
  Serial.println("\nWaiting for time");
  do {
    Serial.print(".");
    delay(1000);
    now = time(nullptr);
    Serial.println(ctime(&now));
  } while (!time(nullptr));
  writeInFile("time.txt", String(now), "w");
}

// Set time if Wi-Fi is not connected(causes unaccuracy)
void setTimeOffline(int used) {
  uint32 currentTime = getTime();
  if(used == 1) {
    currentTime += sleeptime/MICROSEC;
  } else {
    currentTime += millis()/MILLISEC;
  }
  writeInFile("time.txt", (String)currentTime, "w");
}

// Read seconds value during last launch from "time.txt"
uint32 getTime() {
  Serial.println("Getting time");
  String time;
  uint32 time_int;
  time = readFromFile("time.txt");
  time_int = (uint32)time.toInt();
  return time_int;
}

String generateTimestamp(time_t now) {
  struct tm* tmnow = localtime(&now);
  String time = String(tmnow->tm_year + 1900) + "-" + String(tmnow->tm_mon + 1) + "-" + String(tmnow->tm_mday) + "T" + String(tmnow->tm_hour) + 
    ":" + String(tmnow->tm_min) + ":" + String(tmnow->tm_sec) + "+00:00";
  return time;
}

boolean sendDataToServer(String data) {

  HTTPClient http;
  int waitTime = 5000;
  int waited = 0;
  String dataHost = "http://" + readFromFile("ip.txt") + dataHostTail;

  Serial.println("[HTTP] Connecting to server...");
  Serial.println("URL is: " + dataHost);

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
    String timeSleep = http.getString();
    Serial.println(timeSleep);
    timeSleep.replace('"', ' ');
    timeSleep.trim();
    sleeptime = timeSleep.toInt();
    Serial.println(timeSleep);
    Serial.println(sleeptime);
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
      int eof = savedData.available();
      while(eof > 0) {
        dataBulk = dataBulk + readlnSPIFFS(savedData);
        eof = savedData.available();
        if(eof > 0) {
          dataBulk = dataBulk + ",";
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
  String res = "";
  char curr = ' ';
  while(curr != '}' and file.available()) {
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

String readlnSerial() {
  String res = "";
  char curr = ' ';
  while(Serial.available() > 0) {
    curr = Serial.read();
    if(curr != '\n')
      res += curr;
  }  
  return res;
}

String generateData(String temp, String CO2, String humid, String battery) {
  String timestamp = generateTimestamp(getTime());
  String id = readFromFile("register.txt");
  String data = "{\"timestamp\": \"" + timestamp + "\", \"device\": \"" + id + "\", \"charge\": \"" + battery +"\", \"temp\": \"" + 
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

  // Variables to keep necessary data
  String temp = "";
  String CO2 = "";
  String humid = "";
  String battery = "";

  // Handling the serial commands
  Serial.println("Waiting for serial command");
  serialCommands();

  ssid = stringToChar(readFromFile("wifi_ssid.txt"));
  password = stringToChar(readFromFile("wifi_password.txt"));

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

  // Setting time firstly to create data with correct values
  if(wifiConnected) {
    setTimeOnline();
  } else {
    setTimeOffline(1);
  }
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

  // Setting time secondly to save time value more accuracy
  if(wifiConnected) {
    setTimeOnline();
  } else {
    setTimeOffline(2);
  }
  Serial.println("Ended");
  Serial.println(sleeptime);
  ESP.deepSleep(sleeptime);

}

void loop() {
}

