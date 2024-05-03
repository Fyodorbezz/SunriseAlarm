#define RADIO_CE 9
#define RADIO_CSN 10

#include <SPI.h>;
#include <nRF24L01.h>;
#include <RF24.h>;

#include "GyverEncoder.h"

RF24 radio(RADIO_CE, RADIO_CSN);
byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"};

Encoder enc1(4, 3, 2, TYPE2);

bool on = 0;
bool change = 0;
byte level = 0;

void setup() {
  Serial.begin(9600);
  
  radio.begin();
  radio.setAutoAck(1);
  radio.setRetries(0, 15);
  //radio.enableAckPayload();
  radio.setPayloadSize(32);
  radio.openWritingPipe(address[1]);
  radio.setChannel(0x60);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.powerUp();

}

void loop() {
  enc1.tick(); 
  if(enc1.isRight() && level<100 && on){
    level++;
    change = 1;
  }
  else if(enc1.isLeft() && level>0 && on){
    level--;
    change = 1;
  }

  if(enc1.isClick()){
    on = !on;
    change = 1;
  }

  if(change == 1){
    byte data[2] = {on*level, on*level};
    Serial.println(data[0]);
    radio.write(&data, 2);
    change = 0;
  } 
}
