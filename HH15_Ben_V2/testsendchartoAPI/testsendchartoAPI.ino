/*
 *  Authors : Papa Niamadio & Guillaume Pilot
 *  GridPocket confidential
 *
 */

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

const char* ssid     = "HighGrid";
const char* password = "sophiagrid";

//const char* ssid     = "nicegrid";
//const char* password = "rop4ew7ul8wy";

//const char* ssid     = "acauditrdc";
//const char* password = "1paulmontel2015";

const char* accessToken   = "d68a3552-0e1f-42fa-9e11-5c852648609f";
//const String url = "/users/current/sites/gridpocket_1/variables/buro_club";  //Uncomment
//const String url = "/users/current/sites/templiers/variables/compteur_GP";
const char* url = "/users/current/sites/ESP10/variables/flux/series";
int tram_length = 1700;
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

  WiFi.begin(ssid, password);
  delay(500);
  while(WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(ssid, password);
    delay(200);
    Serial.print(".");
    checkTimeAndGoToSleep();
  }
     
  Serial.println("Connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/*****************************************************************
* Check if the received characters aren't corrupted              *
*                                                                *
******************************************************************/
char checkBitParity(char charRead)
{
  //Every msg starts normally with STX and ends with ETX
  char STX = 0x02;
  char ETX = 0x03;
  //Every line of a msg starts with LF and ends with CR
  char LF = 0x0A;
  char CR = 0x0D;
  
  int i= 0;
  int errorRate = 0;
  
  //The righ
   char charProcess = charRead & 0x7F;
   //Variable that will contain the parity of the character calculated 
   char parityChar = 0;
   //Read the value of each bit of the 7 bits of the characters,
   //and add to the sum
   for(i=0;i<7;i++)
   {
     parityChar += bitRead(charProcess,i);
   }
   //The parity must be 0 or 1, so we calculate the modulo 2 
   parityChar = parityChar % 2;
   //The parity received is the eight's bit
   char parity = bitRead(charRead,7);
   //Comparison of the 2 bit parity
   //If they are the same, check if this character is not a special character
   if(parityChar == parity && charProcess != STX && charProcess != LF && charProcess != CR && charProcess != ETX)
   {
     charRead = charProcess;
   }
   else
   {
     //If it's a special character, we replace it by a space
     charRead = ' ';
     errorRate = 1;
   }
  
  return charRead;
}

//Check if ETX is available in the stream or not
boolean lookForETX()
{
  int maxTry = 10000;
  boolean state = false;
  int i = 0;
  for(i=0 ; i<maxTry ; i++)
  {
    if(Serial.available())
    {
      if(((char)Serial.read() & 0x7F) == ETX)
      {
        //if(((char)Serial.read() & 0x7F) == 0x7F) // double check if real ETX
        //{
          //Put stateEtxSearch on true, and ends the loop
          state = true;
          return state;
          break;
        //}
      }
    }
  }
  return state;
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
  while (str.length()<dataLength) {
    if(Serial.available()){
      char c = (char)Serial.read() & 0x7F;
      //if(c!=0x02 && c!=0x03 && c!=0x07){
      if(c>=0x20){
        str += c; //checkBitParity(c);
      }
    }else if(str.length()==0){
      delay(1000);
      Serial.println("No tram detected");
      checkTimeAndGoToSleep();
    }
  }
  digitalWrite(teleinfoVCC, LOW);
  str.replace("\n", " ");
  str.replace("\r", " ");
  str.replace("\\", " ");
  
  data += str;
  data += "\"}";
  return data;
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

/*
void checkTimeToSleep()
{
  //String stringRead = "MESURES1 BT 4 SUP36 @DATE 15/07/15 17:59:40 7EA_s 72Wh @"; //////Comment
  String stringRead = ""; ///////////Uncomment
  char ETX = 0x003;
  //char STX = 0x02;
  
  //variable that will be put at "true" if ETX or STX is found, if not it stays at false
  boolean stateEtxSearch = false;
  //The number of characters to read before saying that ETX or STX is missing
  int i = 0;
  char charRead;
  //The maximum number of characters that can be sent by the wifi module
  int addr = 0;
  
  //Wait the ETX character for a fixed number of tries
  //If found the "stateEtxSearch" status become true and the others 
  //characters are collected, else, the teleinformation signal is missing
  //or the baud rate isn't the right one
  stateEtxSearch = lookForETX();
  if(stateEtxSearch > 0)
  {
    Serial.print("The value of EtxSearch is: ");
    Serial.println(stateEtxSearch);
    stringRead = getData(100);    
    //Avoid some special characters
    stringRead.replace("\r"," ");
    stringRead.replace("\n"," ");
    stringRead.replace("\\","\\\\");
 
    addr = stringRead.indexOf("DATE");
    //Serial.print("indexOf Date : ");
    //Serial.println(addr);
    addr = stringRead.indexOf(" ",addr);
    //Serial.print("indexOf 1st space : ");
    //Serial.println(addr);
    addr = stringRead.indexOf(" ",addr+5);
    //Serial.print("indexOf 2nd space : ");
    //Serial.println(addr);
    
    int minutes = stringRead.substring(addr+4,addr+6).toInt();
    //Serial.print("Minutes : ");
    //Serial.println(minutes);
    
    int secondes = stringRead.substring(addr+7,addr+9).toInt();
    Serial.print("Secondes : ");
    Serial.println(secondes);
    
    stringRead = "";
    
    minutes = minutes % 10;
    //Serial.print("Unity minutes : ");
    //Serial.println(minutes);
    
     int timeToSleep = 600 - ((minutes*60) + secondes) - 6;
     //Serial.print("Time to sleep : ");
     //Serial.print(timeToSleep);
     //Serial.println(" Secondes");
     delay(1000);
     if(timeToSleep >= 4)
      goToSleep(timeToSleep); 
    }
    else {
      goToSleep(600);
    }
    
  }
*/
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
//  json = getData(tram_length);
  //Serial.println(json);
  delay(1000);
//   HCHC 000042655 \\  HCHP 000221240 ^        HCHP 000221240 ^
  json="{\"value\":\" S-g \"}";
  wifiConnection();
  //sendStatusAndBatteriesLevel();
  delay(1000);
  request(json);
  delay(1000);
  WiFi.disconnect(true);

  //checkTimeToSleep();
  goToSleep(45);
}


