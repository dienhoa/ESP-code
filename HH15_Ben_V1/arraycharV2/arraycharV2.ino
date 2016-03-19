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
int tram_length = 500;
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
    Serial.println("Go back to sleep");
    goToSleep(60);
  }else{
    //Serial.println("No time to sleep");
  }
}

void wifiConnection(){
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println("...");

  //If know SSID and just disconnect
  if(WiFi.SSID()==ssid && !WiFi.isConnected()){
    Serial.print("Reconnect to ");
    Serial.println(WiFi.SSID());
    WiFi.reconnect();
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if(i>5 && i!=-1){
        Serial.println("Re-begin");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        i=-1;
      }else if(i!=-1){
        i++;
      }
      delay(500);
      checkTimeAndGoToSleep();
      Serial.print(".");
    }
    Serial.println(WiFi.isConnected());
  }else if(WiFi.SSID()!=ssid){
    Serial.println("Init");
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      checkTimeAndGoToSleep();
      Serial.print(".");    
    }
  }
  Serial.println("Connected");
  Serial.print("IP address: ");
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


char * checksumverify(char data[] ){
  int i=0; // variable to hold position at LF
  int j=0; // variable to hold position at next CR
  while((data[i]!='\n')&&(i<strlen(data))){ // find the 1st position of LF
    i++;
  }
  j=i+1;
  for(int k=0;k<i;k++){data[k]=' ';}
  
  while(j<strlen(data)){
    while((data[j]!='\r')&&(j<strlen(data))){
      j++;
    }
    if (j>=strlen(data)){
      for(int k=i+1;k<j;k++){data[k]=' ';}
      break;
      }
//    Serial.print("value of i: ");Serial.println(i);
//    Serial.print("value of j: ");Serial.println(j);
    char checksum=0; // calculate checksum
    for (int k=i+1; k < j-2; k++){
      checksum += data[k];
    }
    checksum = checksum & 0x3F;
    checksum = checksum + 0x20;
//    Serial.print("Checksum: ");Serial.println(checksum);
//    char calcchecksum = checksumcalc(data.substring(i+1,j-2));
    
    if (checksum != data[j-1]){
      data[j-2]='#'; // # to identify the error group
    }
    i=j+1;
    while((data[i]!='\n')&&(i<strlen(data))){ // find the new LF
      i++;
    }
    if (i>=strlen(data)){
      for(int k=j+1;k<i;k++){data[k]=' ';}
      break;
      }
    for(int k=j+1;k<i;k++){data[k]=' ';} // remove the unreadable group
    j=i+1;
  }

  return data;
}


/*****************************************************************
* Read data from teleinfo input and put data into string that    *
* will be sent                                                   *
******************************************************************/
char * getData(int dataLength){
  String data = "{\"value\":\"";
  String str = "";
  int i = 0;
  int strLength = 0;
  digitalWrite(teleinfoVCC, HIGH);
  delay(1000);
  waitForSTX();
  
  while (str.length()<dataLength) 
    if(Serial.available()){
      char c = (char)Serial.read() & 0x7F;
      if(c == 0x04){ // if receive the EOT, so stop reading frame
        
        if (str.length()>=200){break;}// if the string has already large data so we keep it, otherwise recollect the string
        else {
          str = "";  
          waitForSTX(); // wait until the re-begin of the transmission 
        }
      }else if((c!=0x02) && (c!=0x03 )){ // do not take the STX and ETX
        str += c; 
      }
      
    }else if(str.length()==0){
      delay(1000);
      Serial.println("No tram detected");
      checkTimeAndGoToSleep();
    }
  
  digitalWrite(teleinfoVCC, LOW);
  
  int LF_pos = str.indexOf('\n'); 
  int CR_pos = str.lastIndexOf('\r');
  str = str.substring(LF_pos,CR_pos+1);//remove the unreadale char at the beginning and the end of the frame
  
  int str_len = str.length() + 1; 
//  char strchar[str_len];
  char strchar[str_len];
  str.toCharArray(strchar,str_len);
//  strchar = checksumverify(strchar);
  str.replace("\n", " ");
  str.replace("\r", " ");
//
//  data += str;
//  data += "\"}";;
//  int str_len = data.length() + 1; 
//  char donne[str_len];
//  data.toCharArray(donne,str_len);
  return checksumverify(strchar);
}

void request(String data) {
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  client.println(String("POST ") + url + " HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Content-Type: application/json");
  client.print("Authorization: Bearer ");
  client.println(accessToken);
  client.print("Content-Length: ");
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
  Serial.println("closing connection");
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
  Serial.println("WakeUp!");
}

void loop(){
  
  delay(1000);
  char * json;
  json = getData(tram_length);
  Serial.println(json);
  delay(1000);
  
 
  wifiConnection();
  //sendStatusAndBatteriesLevel();
  delay(1000);
  request(json);
  goToSleep(30);
}



