/***************************************
    * 
    * 
    Hydroponics Control System with Web Server
    By Daniel Payne
    Sources:
    1.  https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/
    2.  https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
    3.  https://maker.pro/arduino/tutorial/how-to-clean-up-noisy-sensor-data-with-a-moving-average-filter
    *Web Server: 
    *   http://192.168.0.107/
    *
    *
*****************************************/

#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "AsyncTCP.h"
#include "SPIFFS.h"
#include "time.h"
#include "Preferences.h"

//Replace with your info for this to work
const char* ssid     = "xxxxxxxxxx";
const char* password = "xxxxxxxxxx";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -25200;
const int   daylightOffset_sec = 0;

//For Web Server. These values will be displayed on the web server.
const char* PARAM1 = "light_start";
const char* PARAM2 = "light_start2";
const char* PARAM3 = "light_stop";
const char* PARAM4 = "light_stop2";
const char* PARAM5 = "water_start";
const char* PARAM6 = "water_start2";
const char* PARAM7 = "water_stop";
const char* PARAM8 = "water_stop2";
const char* PARAM9 = "ph_new_value";

//States 
bool light_state = false;
bool water_state = false;
bool fan_state=false;
bool ph_state=false;
bool ph_pump_state=false;

//Toggles - These things turn on/off based on time.
int light_toggle1=0;
int water_toggle1=0;
int fan_toggle1=0;

//Turn on water when lights turn on
bool water_first_shot=true;

//PH Values
float ph_current_val;
float ph_desired_val;

//Light Timer Variables
int light_start_h=9;
int light_start_m=0;
int light_stop_h=24;
int light_stop_m=0;
int water_start_m=1;
int water_stop_m=1;

//Water Timer
unsigned long previous_millis;
unsigned long current_millis;

//Main Loop Timer
unsigned long main_previous_millis;
unsigned long main_current_millis;
unsigned long main2_current_millis;
unsigned long main2_previous_millis;

//PH Timer
unsigned long ph_previous_millis;
unsigned long ph_current_millis;

//Main Timer for Arduino
char h [80];
char m [80];
char sec [80];

//Pins for 120vac relays and 12vdc transistors
const byte relay1 = 22;
const byte relay2 = 23;
const byte relay3 = 18;
const byte relay4 = 19;

//Pin for PH - Analog In
const byte analog_pin = 32;

//PH Calibration Values
float calibration_value=13.95+7;
float sum=0;
int ph_index=0;
float readings[5];
int window_size = 10;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Preferences for loading data
Preferences preferences;

//Updates Web Server with current value of variables
String processor(const String& var)
{
  Serial.println(var);
  if(var == "CURRENT_TIME"){
    String current_time = "";
    current_time+=h;
    current_time+=":";
    current_time+=m;
      return current_time;
  }
  if(var == "LIGHT_STATE"){
    if (light_state){
      return "ON";
    }
    else {
      return "OFF";
    }
  }
  if(var == "LIGHT_START"){
      String lstart = "";
      if (light_start_h <10){
        lstart+="0";
      }
      lstart+=light_start_h;
      lstart+=":";
      if (light_start_m <10){
        lstart+="0";
      }
      lstart+=light_start_m;
      return lstart;
  }
  if(var == "LIGHT_STOP"){
      String lstop = "";
      if (light_stop_h < 10){
        lstop+="0";
      }
      lstop+=light_stop_h;
      lstop+=":";
      if (light_stop_m < 10){
        lstop+="0";
      }
      lstop+=light_stop_m;
      return lstop;
  }
  if(var == "LIGHT_BUTTON"){
    if (light_state){
      return "TURN OFF";
    }
    else{
      return "TURN ON";
    }
      
  }
  if(var == "WATER_STATE"){
    if (water_state){
      return "ON";
    }
    else {
      return "OFF";
    }
  }
  if(var == "WATER_START"){
      String wstart = "";
      wstart+=water_start_m;
      return wstart;
  }
  if(var == "WATER_STOP"){
      String wstop = "";
      wstop+=water_stop_m;
      return wstop;
  }
  if(var == "WATER_BUTTON"){
    if (water_state){
      return "TURN OFF";
    }
    else{
      return "TURN ON";
    }
      
  }
  if(var == "PH_STATE"){
    if (ph_state){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  if(var == "PH_CURRENT"){
      String ph_current="";
      ph_current.concat(ph_current_val);
      return ph_current;
  }
  if(var == "PH_DESIRED"){
      String ph_value_string="";
      ph_value_string.concat(ph_desired_val);
      return ph_value_string;
  }
  if(var == "PH_BUTTON"){
    if (ph_state){
      return "TURN OFF";
    }
    else{
      return "TURN ON";
    }
  }

  if(var == "FAN_STATE"){
    if (fan_state){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  if(var == "FAN_BUTTON"){
    if (fan_state){
      return "TURN OFF";
    }
    else{
      return "TURN ON";
    }
  }
  
  return String();
}

void setup()
{
  Serial.begin(115200);

  //Setup input/output pins
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);
  pinMode(analog_pin, INPUT);
  
  //Relays off 
  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);

  // Initialize SPIFFS
  while(!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
  }

  //Initiate Preferences - Get Values from Memory
  //If they don't exist, we use default values instead
  preferences.begin("hydroponics_v3", false);
  light_start_h = preferences.getInt("light_start_h", 9);
  light_start_m = preferences.getInt("light_start_m", 0);
  light_stop_h = preferences.getInt("light_stop_h", 24);
  light_stop_m = preferences.getInt("light_stop_m", 0);
  water_start_m = preferences.getInt("water_start_m", 1);
  water_stop_m = preferences.getInt("water_stop_m", 60);
  ph_desired_val = preferences.getLong("ph_desired_val", 7);

  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Set local time once
  setLocalTime();
  
  //Handle HTTP Get Requests
  getFunction();
  
  // Start server
  server.begin();

  //Get timers ready
  main_previous_millis = millis();
  main2_previous_millis = millis();
  
  
}

void loop()
{
  main_current_millis = millis();
  main2_current_millis=main_current_millis;

  //Every 5 seconds
  if (main2_current_millis-main2_previous_millis > 5000){
      main2_previous_millis+=5000;
      phCheck();
    }
    
  //Every 1 seconds
  if (main_current_millis - main_previous_millis>1000){
    main_previous_millis += 1000;
    lightCheck(light_start_h,light_start_m,light_stop_h,light_stop_m); 
    waterCheck(water_start_m,water_stop_m);      
    fanCheck();
        
    if (ph_state){
      phCorrection();
    }
    
    if (light_state){
      digitalWrite(relay1, HIGH);
    }
    else{
      digitalWrite(relay1, LOW);
    }
    if (water_state){
      digitalWrite(relay2, HIGH);
    }
    else{
      digitalWrite(relay2, LOW);
    }
    if (ph_state && ph_pump_state){
      digitalWrite(relay3, HIGH);
    }
    else{
      digitalWrite(relay3, LOW);
    }
    if (fan_state){
      digitalWrite(relay4, HIGH);
    }
    else{
      digitalWrite(relay4, LOW);
    }

    Serial.print("light_state: ");
    Serial.println(light_state);
    Serial.print("water_state: ");
    Serial.println(water_state);  
    Serial.print("ph_pump_state: ");
    Serial.println(ph_pump_state);    
  }
}

void lightCheck(int start_hour,int start_min,int stop_hour, int stop_min)
{
  int current_hour = atoi(h);
  if (start_hour >= stop_hour){
    if (stop_hour > current_hour || current_hour >= start_hour){
      if (light_toggle1 ==0){
        light_state=true;
        light_toggle1=1;
        Serial.println("Turning Lights On");
        water_first_shot=true;
      }
    }
    else{
      if (light_toggle1==1){
        light_toggle1=0;
        light_state=false;
      }
    }    
  }
  else {
    if (stop_hour > current_hour && current_hour >= start_hour){    
      if (light_toggle1 ==0){
        light_state=true;
        light_toggle1=1;
        Serial.println("Turning Lights On");
        water_first_shot=true;
      }     
    }
    else{
      if (light_toggle1==1){
        light_toggle1=0;
        light_state=false;
      } 
    }
  }   
}

void waterCheck(int water_on, int water_off)
{
  int threshold_on = water_on*60*1000;
  int threshold_off = water_off*60*1000;

  current_millis = millis();
  
  if (water_first_shot && light_state){
    water_state=true;
    water_first_shot=false;
    previous_millis=current_millis;
  }  
  
  if (water_state){
    if (current_millis-previous_millis > threshold_on){
      if (water_toggle1==0){
        water_state=false;
        water_toggle1=1;
        previous_millis=current_millis;
      } 
    }    
  }
  else{
    if (current_millis-previous_millis > threshold_off){
      if (water_toggle1==1){
        water_state=true;
        water_toggle1=0;
        previous_millis=current_millis;
      } 
    }
  }
}

void phCheck()
{
  int resolution = analogRead(analog_pin); 
  float voltage = resolution * (3.3 / 4095);
  float ph_initial_val = voltage * -5.7 + calibration_value;
  
  //Moving average formula from https://maker.pro/arduino/tutorial/how-to-clean-up-noisy-sensor-data-with-a-moving-average-filter
  sum=sum-readings[ph_index];
  readings[ph_index]=ph_initial_val;
  sum=sum+ph_initial_val;
  ph_index=(ph_index+1)%window_size;
  ph_current_val = sum/window_size;
  Serial.print("real: ");
  Serial.println(ph_initial_val); 
  Serial.print("average: ");
  Serial.println(ph_current_val);  
  
  
}

void phCorrection()
{
  int time_on = 60*60*1000;
  int time_off = 300*60*1000;

  ph_current_millis = millis();
  
  if (ph_current_val-0.5 >= ph_desired_val){
    ph_pump_state=true;
  }
  else if(ph_desired_val >= ph_current_val-0.5){
    ph_state=false;
    ph_pump_state=false;  
  }

  if (ph_pump_state){
    if (ph_current_millis-ph_previous_millis > time_on){
        ph_pump_state=false;
        ph_previous_millis=ph_current_millis;
      } 
  }
  else{
    if (ph_current_millis-ph_previous_millis > time_off){
        ph_pump_state=true;    
        ph_previous_millis=ph_current_millis; 
    }
  }
}

void fanCheck()
{ 
  if (light_state && fan_toggle1==0){
      fan_toggle1=1;
      fan_state=true;
  }
  else if (!light_state && fan_toggle1==1){
    fan_toggle1=0;
    fan_state=false;  
  }
}

void setLocalTime()
  {
  String s = "";
  struct tm timeinfo;
  
  while (!getLocalTime(&timeinfo)){}
  
  strftime (h, 80, "%H", &timeinfo);
  strftime (m, 80, "%M", &timeinfo);
  strftime (sec, 80, "%S", &timeinfo);

}

void getFunction()
{
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });

  server.on("/light_on", HTTP_GET, [](AsyncWebServerRequest *request){   
         light_state=!light_state;
         if (light_state){
            Serial.println("Lights on");
         }
         else {
            Serial.println("Lights off");
         }
                 
         request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });
  
  server.on("/light_start", HTTP_GET, [](AsyncWebServerRequest *request){   
    if ((request->hasParam(PARAM1))&&(request->hasParam(PARAM2))) {   
      light_start_h = (request->getParam(PARAM1)->value()).toInt();
      light_start_m = (request->getParam(PARAM2)->value()).toInt();
      preferences.putInt("light_start_h", light_start_h);
      preferences.putInt("light_start_m", light_start_m);
    }
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });

  server.on("/light_stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if ((request->hasParam(PARAM3))&&(request->hasParam(PARAM4))) {    
      light_stop_h = (request->getParam(PARAM3)->value()).toInt();
      light_stop_m = (request->getParam(PARAM4)->value()).toInt();
      preferences.putInt("light_stop_h", light_stop_h);
      preferences.putInt("light_stop_m", light_stop_m);
    }   
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });
   
  server.on("/water_on", HTTP_GET, [](AsyncWebServerRequest *request){
    water_state = !water_state;  
    previous_millis=millis();
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });

  server.on("/water_start", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam(PARAM6)) {      
      //water_start_h = (request->getParam(PARAM5)->value()).toInt();
      water_start_m = (request->getParam(PARAM6)->value()).toInt();
      preferences.putInt("water_start_m", water_start_m);
    }    
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });

  server.on("/water_stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam(PARAM8)) {     
      //water_stop_h = (request->getParam(PARAM7)->value()).toInt();
      water_stop_m = (request->getParam(PARAM8)->value()).toInt();
      preferences.putInt("water_stop_m", water_stop_m);
    }   
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });

  server.on("/ph_on", HTTP_GET, [](AsyncWebServerRequest *request){
    ph_state = !ph_state;
    ph_previous_millis=millis();   
    if (!ph_state){
      ph_pump_state=false;
    }
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });

  server.on("/ph_new_value", HTTP_GET, [](AsyncWebServerRequest *request){ 
    if (request->hasParam(PARAM9)) {
      ph_desired_val = atol((request->getParam(PARAM9)->value()).c_str());
      preferences.putLong("ph_desired_val", ph_desired_val);
    }
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });

  server.on("/fan_on", HTTP_GET, [](AsyncWebServerRequest *request){ 
      fan_state = !fan_state; 
      request->send(SPIFFS, "/webpage.html", String(), false, processor);
    });
}
