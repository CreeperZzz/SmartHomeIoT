#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <FS.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <string.h>

// #define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// #define CHARACTERISTIC1_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// #define CHARACTERISTIC2_UUID  "688091db-1736-4179-b7ce-e42a724a6a68"
#define LIGHT_PORT 32
#define SERVO_PORT 21
#define LIGHT_SENSOR_PORT 37
#define MOTION_SENSOR_PORT 2
#define DEBUG 1
#define MOTION_STATUS_DELAY 5000;

Servo myservo;

int light_sensor_enabled = 0;

int prev_light_status;
int light_status;
int servo_status;

int is_sleeping;
int motion_status;

// int sleep_status;
// int startsleep;
// int was_wake =1;

unsigned long light_timer;
unsigned long motion_timer;

char ssid[] = "UCInet Mobile Access";    // your network SSID (name) 
char pass[] = ""; // your network password (use for WPA, or use as key for WEP)

char homessid[] = "NO WIFI";
char homepass[] = "bb110113";

char httpFail[] = "Http Failed";

const char kHostname[] = "54.219.68.25";
const uint16_t kPort = 8080;
const int kNetworkTimeout = 5*1000;
const int kNetworkDelay = 1000;

char value[100];

void setServo(int status){
  if(status){
    myservo.write(100);
    servo_status = 1;
  }else{
    myservo.write(0);
    servo_status = 0;
  }  
}

void setLight(int status){
  if(status){
    digitalWrite(LIGHT_PORT, 1);
    setServo(1);
    light_status = 1;
  }else{
    digitalWrite(LIGHT_PORT, 0);
    setServo(0);
    light_status = 0;
  } 
}

int get_motion(){
  return digitalRead(MOTION_SENSOR_PORT);
}

int lightSensorController(){
  if(millis() > light_timer){
      int analogValue = analogRead(LIGHT_SENSOR_PORT);
      Serial.println(analogValue);
      if(analogValue < 3000){
        Serial.println("Light On");
        // setLight(1);
        return 1;
      }else{
        Serial.println("[Light Sensor] Light Off");
        // setLight(0);
        return 0;
      }
      light_timer = millis()+5000;
  }
  return light_status;
}


char* sendHttpRequest(char kPath[]){
  int err = 0;
  WiFiClient c;
  HttpClient http(c);

  err = http.get(kHostname,kPort, kPath);

  if (err == 0)
  {
    if(DEBUG){
      Serial.println();
      Serial.println("startedRequest ok");
    }
    err = http.responseStatusCode();
    if (err >= 0)
    {
      if(DEBUG){
        Serial.print("Got status code: ");
        Serial.println(err);
      }
      // Usually you'd check that the response code is 200 or a
      // similar "success" code (200-299) before carrying on,
      // but we'll print out whatever response we get

      err = http.skipResponseHeaders();
      if (err >= 0)
      {
        int bodyLen = http.contentLength();
        char str[bodyLen];
        if(DEBUG){
          Serial.print("Content length is: ");
          Serial.println(bodyLen);
          Serial.println();
          Serial.println("Body returned follows:");
        }
      
        // Now we've got to the body, so we can print it out
        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out & haven't reached the end of the body
        while ( (http.connected() || http.available()) &&
              ((millis() - timeoutStart) < kNetworkTimeout) )
        {
            if (http.available())
            {
                c = http.read();
                // Print out this character
                if(DEBUG){Serial.print(c);}
                strncat(str,&c,1);
              
                // We read something, reset the timeout counter
                timeoutStart = millis();
            }
            else
            {
                // We haven't got any data, so let's pause to allow some to
                // arrive
                delay(kNetworkDelay);
            }
        }
        http.stop();
        memset(value,0,100);
        strncpy(value,str,bodyLen);
        return value;
      }
      else
      {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    }
    else
    {    
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  }
  else
  {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }
  http.stop();
  return httpFail;
}

int getLightStatus(){
  char path[] = "/info/light";
  sendHttpRequest(path);
  Serial.println(value);
  return atoi(value);
}

void setLightStatus(int s){
  if(s){
    char p[] = "/light/api?state=on";
    sendHttpRequest(p);
  }else{
    char p[] = "/light/api?state=off";
    sendHttpRequest(p);
  }
}

/**
 * Update local Light Status from aws
 */
void updateLightStatus(){
  prev_light_status = light_status;
  light_status = getLightStatus();
  
  if(DEBUG){
    Serial.println("light status:" +light_status);
    Serial.println("Prev light status:" + prev_light_status);
  }
  if(light_status){
    setLight(1);
  }else{
    setLight(0);
  }
}

void setSleepStatus(int s){
  if(s){
    char p[] = "/sleep/api?state=start";
    sendHttpRequest(p);
  }else{
    char p[] = "/sleep/api?state=end";
    sendHttpRequest(p);
  }
}

char* getCurrentTime(){
  char p[] = "/info/current_time";
  char* v = sendHttpRequest(p);
  Serial.println(v);
  return v;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  myservo.attach(SERVO_PORT);

  pinMode(LIGHT_PORT, OUTPUT);
  pinMode(MOTION_SENSOR_PORT, INPUT);

  WiFi.begin(homessid , homepass);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());

}

void loop() {

  delay(500);

  if(light_sensor_enabled){
    if(lightSensorController()){
      setLightStatus(1);
    }else{
      setLightStatus(0);
    }
  }

  updateLightStatus();

  /**
   * detect Light Status change
   */
  if(prev_light_status != light_status){
    if(DEBUG){Serial.println("light status changed");}
    if(light_status && is_sleeping){
      Serial.println("wake");
      is_sleeping = 0;
      setSleepStatus(0);
    }else{
      if(1){
        Serial.println("sleep");
        is_sleeping = 1;
        setSleepStatus(1);
      }
    }
  }

  getCurrentTime();

  if(is_sleeping){
    if(get_motion()){
      if(!motion_status){
        if(DEBUG){Serial.println("[sleep]Motion detect start");}
        motion_status = 1;
        motion_timer = millis() + MOTION_STATUS_DELAY;
      }else{
        if(millis()>motion_timer){
          if(DEBUG){Serial.println("[Motion] Wake up");}
          is_sleeping = 0;
          setLightStatus(1);
          setSleepStatus(0);
        }
      }
    }else{
      if(motion_status && millis()<motion_timer){
        if(DEBUG){Serial.println("Back Sleep");}
        motion_status = 0;
      }
    }
  }

  /**
  if(startsleep){
    sleep_status = 1;
    startsleep = 0;
  }

  if(sleep_status){
    if(get_motion() && !motion_status){
      motion_status = 1;
      motion_timer = millis() + 5000;
    }
    
    if(millis() < motion_timer){
      if(!get_motion()){
        motion_status = 0;
        Serial.println("Still sleep");
      }
      Serial.println("motion detected");
    }else{
      if(motion_status){
        sleep_status = 0;
        motion_status = 0;
        Serial.println("wake up");
        was_wake = 1;
        setLight(1);
      } 
    }
  }
  */

  // Serial.println(digitalRead(2));
}

// if (err == 0)
//     {
//       Serial.println();
//       Serial.println("startedRequest ok");

//       err = http.responseStatusCode();
//       if (err >= 0)
//       {
//         Serial.print("Got status code: ");
//         Serial.println(err);

//         // Usually you'd check that the response code is 200 or a
//         // similar "success" code (200-299) before carrying on,
//         // but we'll print out whatever response we get

//         err = http.skipResponseHeaders();
//         if (err >= 0)
//         {
//           int bodyLen = http.contentLength();
//           Serial.print("Content length is: ");
//           Serial.println(bodyLen);
//           Serial.println();
//           Serial.println("Body returned follows:");
        
//           // Now we've got to the body, so we can print it out
//           unsigned long timeoutStart = millis();
//           char c;
//           // Whilst we haven't timed out & haven't reached the end of the body
//           while ( (http.connected() || http.available()) &&
//                 ((millis() - timeoutStart) < kNetworkTimeout) )
//           {
//               if (http.available())
//               {
//                   c = http.read();
//                   // Print out this character
//                   Serial.print(c);
                
//                   bodyLen--;
//                   // We read something, reset the timeout counter
//                   timeoutStart = millis();
//               }
//               else
//               {
//                   // We haven't got any data, so let's pause to allow some to
//                   // arrive
//                   delay(kNetworkDelay);
//               }
//           }
//         }
//         else
//         {
//           Serial.print("Failed to skip response headers: ");
//           Serial.println(err);
//         }
//       }
//       else
//       {    
//         Serial.print("Getting response failed: ");
//         Serial.println(err);
//       }
//     }
//     else
//     {
//       Serial.print("Connect failed: ");
//       Serial.println(err);
//     }