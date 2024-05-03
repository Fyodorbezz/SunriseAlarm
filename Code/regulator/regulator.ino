#define ZC_PIN 2
#define PWM_PIN 3

#define RADIO_CE 9
#define RADIO_CSN 10

#include <SPI.h>;
#include <nRF24L01.h>;
#include <RF24.h>;

#include <GyverTimers.h>

RF24 radio(RADIO_CE, RADIO_CSN);
byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"};
byte pipeNo;

int lamp;
bool on=0;
bool last_on=0;
bool flag = 0;

int upeer_rest=7250, lower_rest=3750;

void setup(){
  Serial.begin(9600);
  
  radio.begin();
  radio.setAutoAck(1);
  radio.setRetries(0, 15);
  //radio.enableAckPayload();
  radio.setPayloadSize(32);
  radio.openReadingPipe(1, address[0]);
  radio.setChannel(0x60);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.powerUp();
  radio.startListening();

  pinMode(ZC_PIN, INPUT_PULLUP);
  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, LOW);
  attachInterrupt(0, zero_crossed, RISING);
  Timer1.enableISR();
 }

 void loop(){
  if(radio.available(&pipeNo)){
    byte data[2];
    radio.read(&data, 2);
    if(data[0] == data[1]){
      Serial.println(data[0]);
      if(data[0] == 0){
        lamp = 9400;
        on = 0;
      }
      else if(data[0] == 100){
        lamp = 500;
        on = 1;
      }
      else{
        lamp = map(100 - data[0], 0, 100, lower_rest, upeer_rest);
        on = 1;
      }
    }
  }
  if(on == 0 && last_on == 1){
    Timer1.disableISR();
    digitalWrite(3, LOW);
    last_on = on;
  }
  else if(on == 1 && last_on == 0){
    Timer1.enableISR();
    Timer1.stop();
    last_on = on;
    flag = 1;
  }
  
 }

void zero_crossed(){
  static byte last_value;
  digitalWrite(PWM_PIN, 0);
  if(on){
    if(last_value != lamp){
      Timer1.setPeriod(lamp);
      last_value = lamp;
    }
    else if(flag){
      Timer1.setPeriod(200000);
      flag = 0;
    }
    else{
      Timer1.restart();
    }
  }
 }

ISR(TIMER1_A){
  if(on){
    digitalWrite(PWM_PIN, 1);
    Timer1.stop();
  }
}

