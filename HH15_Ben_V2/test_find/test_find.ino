/*
 *  Authors : Papa Niamadio & Guillaume Pilot
 *  GridPocket confidential
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

const char* ssid     = "HighGrid";
const char* password = "sophiagrid";

const char* accessToken   = "d68a3552-0e1f-42fa-9e11-5c852648609f";

const String url = "/users/current/sites/ESP10/variables/flux/series";

int tram_length = 200;
int secondsToSleep = 1000000;

const char* host = "api.powervas.com";//"192.168.2.30";
const int httpPort = 443;

unsigned long startMillis;
const long maxDelayInMillis = 60000;

int teleinfoVCC = 4;
int BatVoltage = A0;
//alpha = Rb /(Ra+Rb) avec Ra=1MOhms et Rb=82kOhms
float alpha = 0.077;
char ETX = 0x03;

String json;

void goToSleep(int secondes){
  //Serial.print("Sleep ");
  //Serial.println(secondes);
  delay(1000);
  ESP.deepSleep(secondsToSleep * secondes);//Sleep for x seconds
  //delay(secondsToSleep*1000);
}

void checkTimeAndGoToSleep(){
  long currentMillis = millis();
  if(currentMillis - startMillis >= maxDelayInMillis) {
    Serial.println(F("Go back to sleep"));
    goToSleep(60);
  }else{
    //Serial.println("No time to sleep");
  }
}

void wifiConnection(){
  Serial.print(F("Connecting to "));
  Serial.print(ssid);
  Serial.println(F("..."));

  //If know SSID and just disconnect
  if(WiFi.SSID()==ssid && !WiFi.isConnected()){
    Serial.print(F("Reconnect to "));
    Serial.println(WiFi.SSID());
    WiFi.reconnect();
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if(i>5 && i!=-1){
        Serial.println(F("Re-begin"));
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        i=-1;
      }else if(i!=-1){
        i++;
      }
      delay(500);
      checkTimeAndGoToSleep();
      Serial.print(F("."));
    }
    Serial.println(WiFi.isConnected());
  }else if(WiFi.SSID()!=ssid){
    Serial.println(F("Init"));
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      checkTimeAndGoToSleep();
      Serial.print(".");    
    }
  }
  Serial.println(F("Connected"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
}

/*****************************************************************
* Wait to get the STX to start collecting info from the frame    *
*                                                                *
******************************************************************/

void waitForSTX()
{
  while(Serial.available()){
    if(((char)Serial.read() & 0x7F) == 0x02)
      {
          break;
      }
     else {checkTimeAndGoToSleep();}
  }
}


/*****************************************************************
* Verify Checksum to ensure the received frame                   *
*                                                                *
******************************************************************/

char checksumcalc(String data){ // calculate checksum. Sum all characters in the group info, take 6 LSB then + 0x20, (ERDF doc p.18)
  char s=0x00;
  for (int i=0; i < data.length(); i++){
    s += data.charAt(i);
  }
  s = s & 0x3F;
  s = s + 0x20;
  return s;
}

String checksumverify(String data){ // to verify the checksum in each group info in the frame 
  int i=0; // variable to hold position at LF
  int j=0; // variable to hold position at next CR

  while(j<data.length()){
    while((data.charAt(j)!='\r')&&(j<data.length())){
      j++;
    }

   if (j>=data.length()){
      break;
      }
    
    char calcchecksum = checksumcalc(data.substring(i+1,j-2));
    
    if (calcchecksum != data.charAt(j-1)){
      data.setCharAt(j-2,'#'); // delete the part with checksum error
    }
    i=j+1;
    while((data.charAt(i)!='\n')&&(i<data.length())){ // find the new LF
      i++;
    }
   if (i>=data.length()){
      break;
      }
    for (int k=j+1;k<i;k++){data.setCharAt(k,' ');}
//    data.replace(data.substring(j+1,i),""); // delete the unreadable part between CR to new LF
    j=i+1;
  }

  return data;
}


/*****************************************************************
* Read data from teleinfo input and put data into string that    *
* will be sent                                                   *
******************************************************************/
String getData(int dataLength){
  String data = "{\"value\":\"";
  String str = "";
  int i = 0;
  int strLength = 0;
  digitalWrite(teleinfoVCC, HIGH);
  delay(1000);
  waitForSTX(); // wait to receive the 1st SXT then begin to collect data
  
  while (str.length()<dataLength) {
    if(Serial.available()){
      char c = (char)Serial.read() & 0x7F;
      if(c == 0x04){ // if receive the EOT, so stop reading frame
        Serial.println("End of Transmission Detected");
        if (str.length()>=180){break;}// if the string has already large data so we keep it, otherwise recollect the string
        else {
          str = "";  
          waitForSTX(); // wait until the re-begin of the transmission 
        }
      }else if((c!=0x02) && (c!=0x03 )){ // do not take the STX and ETX
        str += c; 
      }
      
    }else if(str.length()==0){
      delay(1000);
      Serial.println(F("No tram detected"));
      checkTimeAndGoToSleep();
    }
  }
  digitalWrite(teleinfoVCC, LOW);
//  str.replace("\\r","\\\r"); // need to check again
  int LF_pos = str.indexOf('\n'); 
  int CR_pos = str.lastIndexOf('\r');
  str = str.substring(LF_pos,CR_pos+1);//remove the unreadale char at the beginning and the end of the frame
  str = checksumverify(str);
  str.replace("\n", " ");
  str.replace("\r", " ");
  str.replace("\\", "\\\\");
  str.replace("\"","\"\"");
  str.replace("\'","\'\'");

  data += str;
  data += "\"}";;
  return data;
}

void request(String data) {
  Serial.print(F("connecting to "));
  Serial.println(host);
  
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  
  if (!client.connect(host, httpPort)) {
    Serial.println(F("connection failed"));
    return;
  }
  
  Serial.print(F("Requesting URL: "));
  Serial.println(url);
  
  client.println(String("POST ") + url + " HTTP/1.1");
  client.print(F("Host: "));
  client.println(host);
  client.println(F("Content-Type: application/json"));
  client.print(F("Authorization: Bearer "));
  client.println(accessToken);
  client.print(F("Content-Length: "));
  client.println(data.length());
  client.println();
  client.println(data);
  
  delay(50);
  
  // Read all the lines of the reply from server and print them to Serial


  
 while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  if(client.connected()){
    client.stop();
  }
  
  Serial.println();
  Serial.println(F("closing connection"));
}

  void sendStatusAndBatteriesLevel()
  {
    String json="{\"value\":\"";
    //Calcul the level of the batteries
    float result = analogRead(BatVoltage);
    result = result / (1024.0 * alpha);
    
    json += " SUPPLY VCC: ";
    json += (String)result;
    json += "\"}";
    request(json);
  }


void setup() {
  pinMode(teleinfoVCC, OUTPUT);
  pinMode(BatVoltage, INPUT);
  
  startMillis = millis();
  Serial.begin(1200, SERIAL_8N1);
  delay(10);
  Serial.println(F("WakeUp!"));
}

void loop(){
  
  delay(1000);
  json = getData(tram_length);
  Serial.println(json);
  delay(1000);
  
 
  wifiConnection();
  //sendStatusAndBatteriesLevel();
  delay(1000);
  request(json);
  goToSleep(30);
}


