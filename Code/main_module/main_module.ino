#define RADIO_CE 9
#define RADIO_CSN 10

#define TIMEOUT 60000

#define OLED_SOFT_BUFFER_64

#include <SPI.h>;
#include <nRF24L01.h>;
#include <RF24.h>;
RF24 radio(RADIO_CE, RADIO_CSN);
byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"};

#include "GyverEncoder.h"
Encoder enc(4, 3, 8, TYPE2);

#include <microDS3231.h>
MicroDS3231 rtc;

#include <GyverOLED.h>
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;

unsigned long int cur_time = millis();
unsigned long int flash_time = 0;
unsigned long int timeout = 0;
unsigned long int display_update_time = 0;


byte lamp = 0;
byte last_lamp=0;

// часы,минуты, время возгорания, активация для каждого дня недели, мигание
byte alarms [10][11]{
  {6, 0, 15, 1, 1, 1, 1, 1, 0, 0, 1},
  {8, 30, 45, 0, 0, 0, 0, 0, 1, 1, 1}
  
};
byte alarms_count = 2;
byte alarm_id = 0;

byte statee = 0;
// 0 нормальная работа
// 1 рассвет
// 2 буждение

bool display_update = 0;
byte main_menu_id = 0;
// 0 home
// 1 clock
// 2 alarm
// 4 settings
byte enc_pos = 0;
byte submenu = 0;

bool bluetooth_connected = 0;

const uint8_t sun_img[] PROGMEM = {
  0xB6, 0xD5, 0xE3, 0x80, 0xE3, 0xD5, 0xB6, 0xFF, 
};

const uint8_t pointer_img[] PROGMEM = {
  0xFF, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F, 0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFF, 
};

const uint8_t settings_img[] PROGMEM = {
  0x3F, 0x23, 0x01, 0x89, 0x31, 0x13, 0x80, 0xC8, 0xC8, 0x80, 0x13, 0x31, 0x89, 0x01, 0x23, 0x3F, 
  0xFE, 0xC4, 0x80, 0x91, 0x8C, 0xC8, 0x01, 0x13, 0x13, 0x01, 0xC8, 0x8C, 0x91, 0x80, 0xC4, 0xFE, 
};

const uint8_t alarm_img[] PROGMEM = {
  0xF7, 0x33, 0x09, 0xE4, 0xE6, 0xF3, 0xFA, 0x18, 0x18, 0xFA, 0xF3, 0xE7, 0xE4, 0x09, 0x33, 0xF7, 
  0xFF, 0xF0, 0x03, 0x1F, 0xBF, 0x3F, 0x7F, 0x7E, 0x7C, 0x79, 0x3B, 0xBF, 0x1F, 0x03, 0xF0, 0xFF, 
};

const uint8_t clock_img[] PROGMEM = {
  0x1F, 0xC7, 0xF3, 0xF9, 0xFD, 0xFC, 0xFE, 0x06, 0x06, 0xFE, 0xFC, 0xFD, 0xF9, 0xF3, 0xC7, 0x1F, 
  0xF8, 0xE3, 0xCF, 0x9F, 0xBF, 0x3F, 0x7F, 0x7E, 0x7C, 0x78, 0x39, 0xBF, 0x9F, 0xCF, 0xE3, 0xF8, 
};

const uint8_t home_img[] PROGMEM = {
  0x7F, 0x3F, 0x1F, 0x8F, 0xC7, 0xE3, 0xF3, 0xF9, 0xF9, 0xF3, 0xE3, 0xC3, 0x83, 0x03, 0x3F, 0x7F, 
  0xFE, 0xFE, 0x00, 0x7F, 0x7F, 0x7F, 0x03, 0xF3, 0xF3, 0x03, 0x7F, 0x7F, 0x7F, 0x00, 0xFE, 0xFE, 
};

void setup(){
  Serial.begin(9600);
  
  rtc.begin();  
  rtc.setTime(COMPILE_TIME);
  
  radio.begin();
  radio.setAutoAck(1);
  radio.setRetries(0, 15);
  //radio.enableAckPayload();
  radio.setPayloadSize(32);
  radio.openWritingPipe(address[0]);
  radio.openReadingPipe(1, address[1]);
  radio.setChannel(0x60);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.powerUp();
  radio.startListening();

  pinMode(7, INPUT);
  attachInterrupt(1, enc_isr, CHANGE);

  oled.init();
  oled.flipV(1);
  oled.flipH(1);
}

void loop(){
  enc.tick();
  cur_time = millis();
  
  if(statee == 0){
    check_time();
  }
  else if(statee == 1){
    sunrize();
  }
  else if(statee == 2){
    wake_up();
  }

  if(radio.available()){
    Serial.print("vflpvblf");
    byte data[2];
    radio.read(&data, 2);
    if(data[0] == data[1]){
      statee = 0;
      lamp = data[0];
    }
  }

  if(Serial.available()){
    byte rx[100];
    byte len = Serial.readBytesUntil(';', rx, 100);
    if(rx[0]=='S' && rx[1]=='A'){
      for(byte i=0;i<11;i++){
        alarms[alarms_count][i] = rx[i+2];   
      }
      alarms_count++;
    }
    if(rx[0]=='R' && rx[1]=='A'){
      for(byte i=0;i<11;i++){
        alarms[rx[2]][i] = rx[i+3];  
      }
    }
    if(rx[0]=='S' && rx[1]=='L'){
      lamp=rx[2];
    }
    if(rx[0]=='B' && rx[1]=='C'){
      bluetooth_connected=1;
    }
    if(rx[0]=='B' && rx[1]=='D'){
      bluetooth_connected=0;
    }
  }
  
  if(last_lamp!=lamp){ 
    Serial.print("LS");
    Serial.print(lamp);
    Serial.println(";");
    byte  data[2]={lamp,lamp};
    radio.stopListening();
    radio.write(&data, 2);
    radio.startListening();
    last_lamp=lamp;
  }  
  if(cur_time-display_update_time>=500){
    display_update = 1;
    display_update_time = cur_time;
  }

  check_enc();
  
  if(display_update){
    //Serial.print("edrherth");
    display_update = 0;
    oled.clear();
    oled.home();
      
    switch(main_menu_id){
      case 0:
      home_menu();
      break;
      case 1:
      clock_menu();
      break;
      case 2:
      alarm_menu();
      break;
      case 3:
      settings_menu();
      break;  
    }

    oled.drawBitmap(0, 48, home_img, 16, 16, main_menu_id != 0, BUF_ADD);
    oled.drawBitmap(37, 48, clock_img, 16, 16, main_menu_id != 1, BUF_ADD);
    oled.drawBitmap(75, 48, alarm_img, 16, 16, main_menu_id != 2, BUF_ADD);
    oled.drawBitmap(112, 48, settings_img, 16, 16, main_menu_id != 3, BUF_ADD);

    oled.update();
    display_update = 0;
  }

  //Serial.print(submenu);
  //Serial.print(",");
  //Serial.println(enc_pos);
  
}

void enc_isr(){
  enc.tick();
  check_enc();
}

void home_menu(){
  print_time();
  oled.setCursor(0,2);
  oled.print("Next alarm clock");
  byte next_alarm_id = 0;
  int min_gap=9999;
  for(byte i=0;i<alarms_count;i++){
    int tmp = alarms[i][0]*60+alarms[i][1]-(rtc.getHours()*60+rtc.getMinutes());
    if(tmp < min_gap && alarms[i][0]*60 + tmp > 0 && alarms[i][2+rtc.getDay()] == 1){
      min_gap = tmp;
      next_alarm_id = i;
    }
  }
  draw_alarm(3, next_alarm_id);
  oled.setCursor(0,4);
  if(enc_pos == 0){
    if(submenu == 0){
      oled.print(">");
    }
    else if(submenu == 1){
      oled.invertText(1);
      oled.print(">");
      oled.invertText(0);
    }
  }
  oled.print(" Lamp ");
  oled.print(lamp);
  oled.rect(63, 32, 127, 39, OLED_STROKE);
  oled.rect(63, 32, 63+lamp/1.55, 39, OLED_FILL);
  futer(1);
}

void clock_menu(){
  futer(0);
}

void alarm_menu(){
  header();
  for(int i=0;i<alarms_count;i++){
    draw_alarm(i+1, i);
  }
  oled.setCursor(60, 4);
  oled.print("+");
  oled.roundRect(58, 31, 66, 39, OLED_STROKE);
  futer(0);
}

void settings_menu(){
  header();
  oled.setCursor(0,1);
  oled.println(F("AC Dimmer: connected"));
  oled.println(F("Switch: connected"));
  if(digitalRead(A0)){
    oled.println(F("Phone: connected"));
  }
  else{ 
    oled.println(F("Phone: disconnected"));
  }
  
  futer(0);
}

void draw_alarm(byte level, byte alarm_id){
  oled.setCursor(0,level);
  oled.print(alarms[alarm_id][0]);
  oled.print(":");
  if(alarms[alarm_id][1] < 10){
    oled.print("0");
  }
  oled.print(alarms[alarm_id][1]);
  oled.drawBitmap(35, level*8, sun_img, 8, 8, 1,BUF_ADD);
  oled.setCursor(45, level);
  oled.print(alarms[alarm_id][2]);
  oled.setCursor(62, level);
  if(alarms[alarm_id][3]){
    oled.print("M");
  }
  if(alarms[alarm_id][4]){
    oled.print("Tu");
  }
  if(alarms[alarm_id][5]){
    oled.print("W");
  }
  if(alarms[alarm_id][6]){
    oled.print("Th");
  }
  if(alarms[alarm_id][7]){
    oled.print("F");
  }
  if(alarms[alarm_id][8]){
    oled.print("Su");
  }
  if(alarms[alarm_id][9]){
    oled.print("Sa");
  }
}

void header(){
  DateTime now = rtc.getTime();
  oled.setCursor(6, 0);
  if(now.hour < 10){
    oled.print("0");
  }
  oled.print(now.hour);
  oled.print(":");
  if(now.minute < 10){
    oled.print("0");
  }
  oled.print(now.minute);
  oled.print(":");
  if(now.second < 10){
    oled.print("0");
  }
  oled.print(now.second);
  oled.print(" ");
  if(now.date < 10){
    oled.print("0");
  }
  oled.print(now.date);
  oled.print(".");
  if(now.month < 10){
    oled.print("0");
  }
  oled.print(now.month);
  oled.print(".");
  oled.print(now.year);
}

void futer(byte shift){
  if(submenu == 0){
    switch(enc_pos - shift){
      case 0:
        oled.drawBitmap(0, 40, pointer_img, 16, 8, 1, BUF_ADD);
      break;
      case 1:
        oled.drawBitmap(37, 40, pointer_img, 16, 8, 1, BUF_ADD);
      break;
      case 2:
        oled.drawBitmap(75, 40, pointer_img, 16, 8, 1, BUF_ADD);
      break;
      case 3:
        oled.drawBitmap(112, 40, pointer_img, 16, 8, 1, BUF_ADD);
      break;
    }
  }
}
void print_time(){
  DateTime now = rtc.getTime();
  oled.setScale(2);
  if(now.hour < 10){
    oled.print("0");
  }
  oled.print(now.hour);
  oled.print(":");
  if(now.minute < 10){
    oled.print("0");
  }
  oled.print(now.minute);
  oled.setScale(1);
  oled.setCursor(65, 0);
  oled.print(now.date);
  oled.print(".");
  oled.print(now.month);
  oled.print(".");
  oled.print(now.year);
  oled.setCursor(65, 1);
  switch(now.day){
    case 1:
    oled.print("Monday");
    break;
    case 2:
    oled.print("Tuesday");
    break;
    case 3:
    oled.print("Wednesday");
    break;
    case 4:
    oled.print("Thersday");
    break;
    case 5:
    oled.print("Friday");
    break;
    case 6:
    oled.print("Sunday");
    break;
    case 7:
    oled.print("Saturday");
    break;
  }
}

void check_enc(){
  switch(main_menu_id){
    case 0:
    switch(submenu){ 
      case 0:
        if(enc.isRight() && enc_pos<4){
          enc_pos++;
          //display_update = 1;
        }
        if(enc.isLeft() && enc_pos>0){
          enc_pos--;
          //display_update = 1;
        }
        if(enc.isClick()){
          switch(enc_pos){
            case 0:
              submenu = 1;
            break;
            case 99:
            break;
            default:
             main_menu_id = enc_pos-1;
             enc_pos = 0;
          }
         //display_update = 1;
        }
      break;

      case 1:
        if(enc.isClick()){
          submenu = 0;
          enc_pos = 0;
          //display_update = 1;
          break;
        }
        if(enc.isRight() && lamp<100){
          lamp++;
          //display_update = 1;
        }
        if(enc.isLeft() && lamp>0){
          lamp--;
          //display_update = 1;
        }
      
      break;
    }
    break;

    case 1:
    switch(submenu){ 
      case 0:
        if(enc.isRight() && enc_pos<3){
          enc_pos++;
          //display_update = 1;
        }
        if(enc.isLeft() && enc_pos>0){
          enc_pos--;
          //display_update = 1;
        }
        if(enc.isClick()){
          switch(enc_pos){
            default:
             main_menu_id = enc_pos;
             enc_pos = 0;
          }
         //display_update = 1;
        }
      break;
    }
    break; 

    case 2:
    switch(submenu){ 
      case 0:
        if(enc.isRight() && enc_pos<3){
          enc_pos++;
          //display_update = 1;
        }
        if(enc.isLeft() && enc_pos>0){
          enc_pos--;
          //display_update = 1;
        }
        if(enc.isClick()){
          switch(enc_pos){
            default:
             main_menu_id = enc_pos;
             enc_pos = 0;
          }
         //display_update = 1;
        }
      break;
    }
    break;

    case 3:
    switch(submenu){ 
      case 0:
        if(enc.isRight() && enc_pos<3){
          enc_pos++;
          //display_update = 1;
        }
        if(enc.isLeft() && enc_pos>0){
          enc_pos--;
          //display_update = 1;
        }
        if(enc.isClick()){
          switch(enc_pos){
            default:
             main_menu_id = enc_pos;
             enc_pos = 0;
          }
         //display_update = 1;
        }
      break;
    }
    break;
  }
}

void check_time(){
  for(int i=0;i<alarms_count;i++){
    if(alarms[i][rtc.getDay() + 2] == 1){
      //Serial.print(alarms[i][0]*60+alarms[i][1]-alarms[i][2]);
      //Serial.print(",");
      //Serial.println(rtc.getHours()*60+rtc.getMinutes());
      if(alarms[i][0]*60+alarms[i][1]-alarms[i][2] == rtc.getHours()*60+rtc.getMinutes()){
        statee = 1;
        alarm_id = i;
      }
    } 
  }
}

void sunrize(){
  if(rtc.getHours()*60+rtc.getMinutes() == alarms[alarm_id][0]*60+alarms[alarm_id][1]){
    statee = 2;
    flash_time = cur_time;
    timeout = cur_time;
  }
  else{
    //Serial.print(rtc.getHours()*3600+rtc.getMinutes()*60+rtc.getSeconds());
    //Serial.print(",");
    //Serial.println(float(rtc.getHours()*3600+rtc.getMinutes()*60+rtc.getSeconds())][0]*3600+alarms[alarm_id][1]*60));
    lamp = 100*(float((rtc.getHours()*3600+rtc.getMinutes()*60+rtc.getSeconds())+(alarms[alarm_id][2]*60)-(alarms[alarm_id][0]*3600+alarms[alarm_id][1]*60))/(alarms[alarm_id][2]*60));
  }
}
void wake_up(){
  static bool flash = 0;
  if(digitalRead(7) == 1 || cur_time-timeout >= TIMEOUT){
    statee = 0;
    noTone(6);

  }
  else if(cur_time-flash_time >= 500){
    digitalWrite(5, flash);
    tone(6,1000*flash);
    if(alarms[alarm_id][10] == 1){
      lamp = 100*flash;
    }
    flash = !flash;
    flash_time = cur_time
  }
}
