#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>

const char* ssid = "ORANGE_2160";
const char* pass = "FfAQeUSU";

const int PIN_SERVO_DOOR=14, PIN_SERVO_WIN=17, PIN_FAN_INP=12, PIN_FAN_INN=13;
const int PIN_BUZ=27, PIN_LED=5, PIN_RGB=18, PIN_DHT=23, PIN_PIR=19;
const int PIN_GAS=16, PIN_RAIN=32, PIN_BTN1=34, PIN_BTN2=35;
const int PIN_GAS_A=33;
const int RAIN_LIMIT=2500;

DHT dht(PIN_DHT, DHT11);
Servo door, win;
LiquidCrystal_I2C lcd(0x27,16,2);
Adafruit_NeoPixel rgb(4, PIN_RGB, NEO_GRB + NEO_KHZ800);
WiFiServer server(80);

WiFiClient appClient;
String lineBuf="";

bool ledOn=false, fanOn=false, doorOpen=false, winOpen=true, lastBtn2=false, whistleOn=false;
unsigned long lastLCD=0, lastSend=0, lastWinMove=0, lastDoorMove=0;
String lastEvent="Welcome!"; unsigned long eventTime=0;
void setEvent(String m){ lastEvent=m; eventTime=millis(); }

void moveServo(Servo &s, int pin, int angle){
  s.attach(pin); s.write(angle); delay(700); s.detach();
  pinMode(pin, OUTPUT); digitalWrite(pin, LOW);
}

uint32_t Wheel(byte w){ w=255-w; if(w<85) return rgb.Color(255-w*3,0,w*3); if(w<170){w-=85; return rgb.Color(0,w*3,255-w*3);} w-=170; return rgb.Color(w*3,255-w*3,0); }
void setColor(uint8_t r,uint8_t g,uint8_t b){ for(int i=0;i<4;i++) rgb.setPixelColor(i, rgb.Color(r,g,b)); rgb.show(); }
void rainbow(){ for(int f=0;f<256;f+=8){ for(int i=0;i<4;i++) rgb.setPixelColor(i, Wheel((f+i*64)&255)); rgb.show(); delay(20);} }
void chase(){ for(int s=0;s<16;s++){ for(int i=0;i<4;i++) rgb.setPixelColor(i,(i==s%4)?Wheel(s*16):rgb.Color(0,0,0)); rgb.show(); delay(60);} }
void birthday(){ int n[]={392,392,440,392,523,494,392,392,440,392,587,523}; for(int i=0;i<12;i++){ tone(PIN_BUZ,n[i]); delay(280);} noTone(PIN_BUZ); }

void sendData(){
  if(!appClient) return;
  int r=analogRead(PIN_RAIN);
  bool g=(analogRead(PIN_GAS_A)>1000), m=digitalRead(PIN_PIR);
  float t=dht.readTemperature(), h=dht.readHumidity();
  appClient.print(String(r)+","+(g?"0":"1")+","+(m?"1":"0")+","+String(t)+","+String(h));
}


void respondOK(){
  if(!appClient) return;
  appClient.print("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nok");
}


void handleChar(char c){
  Serial.print("App char: "); Serial.println(c);
  switch(c){
    case 'a': ledOn=true; setEvent("LED ON"); break;
    case 'A': ledOn=false; setEvent("LED OFF"); break;
    case 'b': if(millis()-lastWinMove>1500){ lastWinMove=millis(); winOpen=true; moveServo(win,PIN_SERVO_WIN,90); setEvent("Window OPEN");} break;
    case 'B': if(millis()-lastWinMove>1500){ lastWinMove=millis(); winOpen=false; moveServo(win,PIN_SERVO_WIN,10); setEvent("Window SHUT");} break;
    case 'c': birthday(); setEvent("Music!"); break;
    case 'C': noTone(PIN_BUZ); setEvent("Music off"); break;
    case 'd': whistleOn=true; setEvent("Whistle ON"); break;
    case 'D': whistleOn=false; setEvent("Whistle off"); break;
    case 'e': if(millis()-lastDoorMove>1500){ lastDoorMove=millis(); doorOpen=true; moveServo(door,PIN_SERVO_DOOR,90); setEvent("Door OPEN");} break;
    case 'E': if(millis()-lastDoorMove>1500){ lastDoorMove=millis(); doorOpen=false; moveServo(door,PIN_SERVO_DOOR,0); setEvent("Door SHUT");} break;
    case 'f': fanOn=true; setEvent("Fan ON"); break;
    case 'F': fanOn=false; setEvent("Fan OFF"); break;
    case 'g': setColor(255,0,0); setEvent("Red"); break;
    case 'h': setColor(200,100,0); setEvent("Orange"); break;
    case 'i': setColor(200,200,0); setEvent("Yellow"); break;
    case 'j': setColor(0,255,0); setEvent("Green"); break;
    case 'k': setColor(0,100,255); setEvent("Cyan"); break;
    case 'l': setColor(0,0,255); setEvent("Blue"); break;
    case 'm': setColor(100,0,255); setEvent("Purple"); break;
    case 'n': setColor(255,255,255); setEvent("White"); break;
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': setColor(0,0,0); setEvent("Light off"); break;
    case 'o': rainbow(); setEvent("Rainbow!"); break;
    case 'p': chase(); setEvent("Chase!"); break;
    case 'O': case 'P': setColor(0,0,0); setEvent("Light off"); break;
  }
}
void handleLine(String L){
  Serial.println("CMD: "+L);
  if(L.indexOf("/led/on")>=0){ respondOK(); ledOn=true; }
  else if(L.indexOf("/led/off")>=0){ respondOK(); ledOn=false; }
  else if(L.indexOf("/window/on")>=0){ respondOK(); if(millis()-lastWinMove>1500){ lastWinMove=millis(); winOpen=true; moveServo(win,PIN_SERVO_WIN,90);} }
  else if(L.indexOf("/window/off")>=0){ respondOK(); if(millis()-lastWinMove>1500){ lastWinMove=millis(); winOpen=false; moveServo(win,PIN_SERVO_WIN,10);} }
  else if(L.indexOf("/music/on")>=0){ respondOK(); birthday(); }
  else if(L.indexOf("/music/off")>=0){ respondOK(); noTone(PIN_BUZ); }
  else if(L.indexOf("/buz/on")>=0){ respondOK(); whistleOn=true; }
  else if(L.indexOf("/buz/off")>=0){ respondOK(); whistleOn=false; }
  else if(L.indexOf("/door/on")>=0){ respondOK(); if(millis()-lastDoorMove>1500){ lastDoorMove=millis(); doorOpen=true; moveServo(door,PIN_SERVO_DOOR,90);} }
  else if(L.indexOf("/door/off")>=0){ respondOK(); if(millis()-lastDoorMove>1500){ lastDoorMove=millis(); doorOpen=false; moveServo(door,PIN_SERVO_DOOR,0);} }
  else if(L.indexOf("/fan/on")>=0){ respondOK(); fanOn=true; }
  else if(L.indexOf("/fan/off")>=0){ respondOK(); fanOn=false; }
  else if(L.indexOf("/red/on")>=0){ respondOK(); setColor(255,0,0); }
  else if(L.indexOf("/oringe/on")>=0||L.indexOf("/orange/on")>=0){ respondOK(); setColor(200,100,0); }
  else if(L.indexOf("/yellow/on")>=0){ respondOK(); setColor(200,200,0); }
  else if(L.indexOf("/green/on")>=0){ respondOK(); setColor(0,255,0); }
  else if(L.indexOf("/cyan/on")>=0){ respondOK(); setColor(0,100,255); }
  else if(L.indexOf("/blue/on")>=0){ respondOK(); setColor(0,0,255); }
  else if(L.indexOf("/purple/on")>=0){ respondOK(); setColor(100,0,255); }
  else if(L.indexOf("/white/on")>=0){ respondOK(); setColor(255,255,255); }
  else if(L.indexOf("/red/off")>=0||L.indexOf("/oringe/off")>=0||L.indexOf("/orange/off")>=0||L.indexOf("/yellow/off")>=0||L.indexOf("/green/off")>=0||L.indexOf("/cyan/off")>=0||L.indexOf("/blue/off")>=0||L.indexOf("/purple/off")>=0||L.indexOf("/white/off")>=0||L.indexOf("/sfx1/off")>=0||L.indexOf("/sfx2/off")>=0){ respondOK(); setColor(0,0,0); }
  else if(L.indexOf("/sfx1/on")>=0){ respondOK(); rainbow(); }
  else if(L.indexOf("/sfx2/on")>=0){ respondOK(); chase(); }
  else if(L.indexOf("/rain/on")>=0) appClient.println(analogRead(PIN_RAIN));
  else if(L.indexOf("/gas/on")>=0) appClient.println(analogRead(PIN_GAS_A)>1000?"dangerous":"safety");
  else if(L.indexOf("/body/on")>=0) appClient.println(digitalRead(PIN_PIR)?"someone":"no one");
  else if(L.indexOf("/temp/on")>=0) appClient.println(dht.readTemperature());
  else if(L.indexOf("/humidity/on")>=0) appClient.println(dht.readHumidity());
}

void setup(){
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting");
  unsigned long t0=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-t0<20000){ delay(500); Serial.print("."); }
  if(WiFi.status()==WL_CONNECTED){
    Serial.println(); Serial.print("IP: "); Serial.println(WiFi.localIP());
    server.begin();
  } else {
    Serial.println();
    Serial.println("WiFi FAILED! Chabakat:");
  WiFi.disconnect();delay(200);
    int n = WiFi.scanNetworks();
    if(n==0){delay(1000);n = WiFi.scanNetworks();}
    for(int i=0;i<n;i++){
      Serial.println(String(WiFi.SSID(i)) + "  quwa:" + String(WiFi.RSSI(i)));
    }
  }
  dht.begin();
  moveServo(door,PIN_SERVO_DOOR,0);
  moveServo(win,PIN_SERVO_WIN,90);
  pinMode(PIN_PIR,INPUT); pinMode(PIN_BTN1,INPUT); pinMode(PIN_BTN2,INPUT);
  pinMode(PIN_GAS,INPUT); pinMode(PIN_LED,OUTPUT); pinMode(PIN_BUZ,OUTPUT);
  noTone(PIN_BUZ);
  pinMode(PIN_FAN_INP,OUTPUT); digitalWrite(PIN_FAN_INP,LOW);
  pinMode(PIN_FAN_INN,OUTPUT); digitalWrite(PIN_FAN_INN,LOW);
  rgb.begin(); rgb.setBrightness(60); setColor(0,0,0);
  lcd.init(); lcd.backlight();
  lcd.print("Welcome! "); lcd.setCursor(0,1); lcd.print("Smart Home");
  delay(2000);
}

void loop(){
  if(server.hasClient()){ appClient=server.available(); Serial.println("App connected!"); lineBuf=""; setEvent("App Linked!"); }
  if(appClient && !appClient.connected()){ appClient.stop(); Serial.println("App left"); }

    while(appClient.available()){
    char c=appClient.read();
    Serial.print(c);
    if(c=='\n' || c=='s'){
      if(lineBuf.length()>0){
        if(lineBuf.indexOf("/")>=0) handleLine(lineBuf);
        else handleChar(lineBuf.charAt(0));
        lineBuf="";
      }
    }
    else if(c!='\r' && lineBuf.length()<80) lineBuf+=c;
  }

  float t=dht.readTemperature(), h=dht.readHumidity();
  int rainValue=analogRead(PIN_RAIN);
  bool gasAlarm=(analogRead(PIN_GAS_A)>1000), mov=digitalRead(PIN_PIR);
  bool b1=!digitalRead(PIN_BTN1), b2=!digitalRead(PIN_BTN2);
  bool raining=(rainValue>RAIN_LIMIT);
    static bool prevGas=false, prevMov=false;
  if(gasAlarm&&!prevGas) setEvent("GAS ALARM!");
  if(mov&&!prevMov) setEvent("Motion!");
  prevGas=gasAlarm; prevMov=mov;

  if(raining && winOpen){ winOpen=false; moveServo(win,PIN_SERVO_WIN,10); setEvent("Rain!Win shut"); }
  if(fanOn){ digitalWrite(PIN_FAN_INN,LOW); digitalWrite(PIN_FAN_INP,HIGH); }
  else { digitalWrite(PIN_FAN_INP,LOW); digitalWrite(PIN_FAN_INN,LOW); }
  digitalWrite(PIN_LED, ledOn?HIGH:LOW);
  if(b1||gasAlarm||whistleOn) tone(PIN_BUZ,1000); else noTone(PIN_BUZ);
  if(b2 && !lastBtn2){ doorOpen=!doorOpen; moveServo(door,PIN_SERVO_DOOR,doorOpen?90:0); }
  lastBtn2=b2;

  if(millis()-lastLCD>1000){
    lastLCD=millis();
    Serial.println("G:"+String(gasAlarm)+" R:"+String(rainValue)+" M:"+String(mov)+" B1:"+String(b1)+" B2:"+String(b2)+" GA:"+String(analogRead(PIN_GAS_A)));
    lcd.clear(); lcd.print("T:"); lcd.print(t); lcd.print("C H:"); lcd.print(h); lcd.print("%");
        lcd.setCursor(0,1);
    if(millis()-eventTime<5000) lcd.print(lastEvent);
    else if(raining) lcd.print("Rain! ");
    else if(gasAlarm) lcd.print("Gas! ");
    else if(mov) lcd.print("Motion");
    else lcd.print(winOpen?"All OK":"Win:Shut");
  }
  if(millis()-lastSend>500){ lastSend=millis(); sendData(); }
}