#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define STX 85
#define ETX 170
#define ACK 6
#define NAK 21
#define SEND_LENGTH 30
#define RECEIVE_LENGTH 110
#define SOFTWARE_SERIAL_TX 12 
#define SOFTWARE_SERIAL_RX 13 
#define COLD_WASH_VALVE 8
#define HOT_WASH_VALVE 7
#define PRE_WASH_VALVE 6
#define BLEACH_VALVE 5
#define DRAIN_PUMP 4
#define STEAM_HEATER 3
#define STEAM_VALVE 2


SoftwareSerial mySerial(SOFTWARE_SERIAL_RX, SOFTWARE_SERIAL_TX); // RX, TX UINT8 maucReceiveBuffer [RECEIVE_LENGTH]; 
LiquidCrystal_I2C lcd(0x27, 16, 4);
uint8_t maucReceiveBuffer[RECEIVE_LENGTH];
//Receiving DATA Buffer(fron product) - 110 bytes
//uint8_t send[SEND_LENGTH];
//Sending DATA Buffer(to product) - 30 bytes

uint8_t send[30]={85, 6, 2550/256, 2550%256, 882/256, 882%256, 800/256, 800%256, 840/256, 840%256, 825/256, 825%256, 540/256, 540%256, 960/256, 960%256, 100, 1, 11, 0, 0, 0, 0, 0, 0, 0, 0, 64, 166, 170};
uint8_t BIT_89[8]={};
uint8_t BIT_91[8]={};

int f=10.0; //배속
bool cpros=false; //현재 세탁상태

void GetBit(uint8_t val89, uint8_t val91) {
  if(maucReceiveBuffer[3]!=1) return;
  for(int i=0;i<=7;i++){
    BIT_89[i]=val89%2;
    BIT_91[i]=val91%2;
    val89/=2;
    val91/=2;
  }
  return;
}

void Printstate(){
  uint16_t cwf=send[2]*256 + send[3];
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WaterFreq:");
  lcd.setCursor(0,1);
  lcd.print(cwf);
  return;
}

void WaterFreq(int timecnt) {
  if(timecnt%5!=0) return;
  uint16_t cwf=send[2]*256 + send[3];
  int cnt = 0;
  static int exceed = 0;
    
  if(BIT_89[0]==1) cnt++;
  if(BIT_89[2]==1) cnt++;
  if(BIT_89[3]==1) cnt++;
  if(BIT_89[4]==1) cnt++;
  
  if(cwf>2370) exceed=1;

  if ( cwf > 2370){
    cwf-=cnt*1*f;
  }else if ( cwf <=2370 && exceed==0 ){
    cwf-=cnt*0.7*f;
  }else if ( cwf <=2370 && exceed==1){
    cwf-=cnt*0.9*f;
  }

  if(BIT_91[5]==1) cwf+=4*f;

  if(cwf>2550) cwf=2550; //배속으로 인한 초과를 제어
  if(cwf<2120) cwf=2120;
  
  send[2]=cwf/256; 
  send[3]=cwf%256;
  return;
}

void WashTemp(int timecnt) {
  if(timecnt%5!=0) return;
  uint16_t cwt=send[4]*256 + send[5];
  int ccnt=0, hcnt=0;
  if(BIT_89[2]==1) ccnt++;
  if(BIT_89[3]==1) ccnt++;
  if(BIT_89[4]==1) ccnt++;
  if(BIT_89[0]==1) hcnt++;

  if(cwt<860 && ccnt==0 && hcnt==0) cwt+=0.05;
  else{
    cwt+=ccnt*1.5*f;
    cwt-=ccnt*0.4*f;
  }
  if(cwt>890) cwt=890;
  if(cwt<180) cwt=180;
  send[4]=cwt/256;
  send[5]=cwt%256;
  return;
}

void IPMTemp(int timecnt){
  if (maucReceiveBuffer[3]==0){
    if(maucReceiveBuffer[98] == 10) {cpros = true; } //현재상태: 탈수
    else                            {cpros = false;} //현재상태: 그외
  }
  if(timecnt%5!=0) return;
  uint16_t ipm=send[6]*256 + send[7];
  uint16_t requestRpm=maucReceiveBuffer[13]*256 + maucReceiveBuffer[14];
  if (requestRpm > 0){        //when requestRpm>0 
    if(cpros){                //탈수인 경우
      ipm += 2*f;
    }
    else {     
      ipm += 3.5*f;
    }
  } else if (requestRpm == 0 && ipm > 85){
    ipm -= 0.2*f;
  }

  send[6]=ipm/256; 
  send[7]=ipm%256;
  return;
}

void SteamTemp(int timecnt) {
  if(timecnt%5!=0) return;
  uint16_t stetem=send[8]*256 + send[9];
  if(BIT_89[6]==1) stetem-=0.3*f;  //steam_heater ON
  if(BIT_89[1]==1) stetem+=1.5*f;  //steam_valve ON
  if(BIT_89[6]==0 && BIT_89[1]==0 && stetem <820) stetem +=0.2*f; //steam_heater/steam_valve off,Steamtemp<820

  if(stetem>840) stetem=840;
  if(stetem<260) stetem=260;

  send[8]=stetem/256;
  send[9]=stetem%256;
  return;
}

void SteamVoltage(int timecnt){
  if(timecnt%5!=0) return;
  int mode=0; uint16_t slv=send[12]*256+send[13], ssv=send[14]*256+send[15];
  if(BIT_89[6]==1) mode=1;
  if(BIT_89[1]==1) mode=2;
  switch(mode){
    case 1:
      slv-=4.5; ssv-=4.5;
      break;
    case 2:
      //수치값 정해지지 않음.
      break;
  }
  send[12]=slv/256; send[13]=slv%256;
  send[14]=ssv/256; send[15]=ssv%256;
  return;
}
        
uint16_t MAKE2BYTE(uint8_t num_1, uint8_t num_2){
  uint16_t r=0;
  r+=num_1*256;
  r+=num_2;
  return r;
}

uint16_t CRC_Maker(uint8_t buffer[], uint8_t length){    //crc check 함수
  uint8_t i=0;
  uint16_t crc=0, temp=0, quick=0;
  for(i=0; i<length; i++){
    temp=(crc>>8)^*buffer++;
    crc <<= 8;
    quick = temp ^ (temp>>4);
    crc ^= quick;
    quick <<= 5;
    crc ^= quick;
    quick <<= 7;
    crc ^= quick;
  }
  return crc;
}

uint8_t PacketError_Check(uint8_t a_u8PacketLength){         //packet check for receive
  uint16_t lrc_result = 0;
  uint16_t temp_int = 0;
  lrc_result = CRC_Maker(&(maucReceiveBuffer[0]), (uint8_t)(a_u8PacketLength - 3));

  *((unsigned char *) (&temp_int) + 1) = maucReceiveBuffer[a_u8PacketLength - 3];
  *((unsigned char *) (&temp_int)) = maucReceiveBuffer[a_u8PacketLength - 2];

  if(lrc_result == temp_int) {
    return 1;
  }
  else{
    return 0;
  }
}

uint8_t PacketError_Check_1(uint8_t a_u8PacketLength){        //packet check for send
  uint16_t lrc_result = 0;
  uint16_t temp_int = 0;
  lrc_result = CRC_Maker(&(send[0]), (uint8_t)(a_u8PacketLength - 3));

  *((unsigned char *) (&temp_int)) = send[a_u8PacketLength - 3];
  *((unsigned char *) (&temp_int)+1) = send[a_u8PacketLength - 2];

  if(lrc_result == temp_int) {
    return 1;
  }
  else{
    return 0;
  }
}

void Upadte_LED(void){
  digitalWrite(COLD_WASH_VALVE, BIT_89[4]);
  digitalWrite(HOT_WASH_VALVE, BIT_89[0]);
  digitalWrite(PRE_WASH_VALVE, BIT_89[3]);
  digitalWrite(BLEACH_VALVE, BIT_89[2]);
  digitalWrite(DRAIN_PUMP, BIT_91[5]);
  digitalWrite(STEAM_HEATER, BIT_89[6]);
  digitalWrite(STEAM_VALVE, BIT_89[1]);
  return;
}

void Update_RX_Data(void){
  send[17]=maucReceiveBuffer[13];   //update requestrpm
  send[18]=maucReceiveBuffer[14];
  Upadte_LED();
  // uint16_t unRequestRPM = maucReceiveBuffer[13]*256 + maucReceiveBuffer[14];
  // if (maucReceiveBuffer[3] == 0){
  //     mstRxDataInfo.ucCurrentProcess = maucReceiveBuffer[98];
  //   }
  //   else if (maucReceiveBuffer[3] == 1) {// ID2 : Packet mode  
  //   mstRxDataInfo.ucLoadlInfoByte.byte = maucReceiveBuffer[89];
  //   mstRxDataInfo.ucLoad2InfoByte.byte = maucReceiveBuffer[90];
  //   mstRxDataInfo.ucLoad3InfoByte.byte = maucReceiveBuffer[91];
  //   }
}

void Update_TX_Data (void) {
  static uint8_t uc100msTimer = 0;

  uc100msTimer++;
  if (uc100msTimer > 10){
  uc100msTimer = 1;
  }
  else;
  GetBit(maucReceiveBuffer[89], maucReceiveBuffer[91]);
  WaterFreq(uc100msTimer);
  WashTemp(uc100msTimer);
  // IPMTemp(uc100msTimer);  //일단은 없이 돌려보기, 표준모드에서는 변경값없음.
  SteamTemp(uc100msTimer);
  SteamVoltage(uc100msTimer);
}

void Virtualization_BigFL() {
  if(mySerial.available() > 0){
    if (mySerial.peek() != STX) {
      while(mySerial.available()) mySerial.read();
    }
    else {
      mySerial.readBytes(maucReceiveBuffer, sizeof(maucReceiveBuffer));
      if(PacketError_Check(RECEIVE_LENGTH) == 0) {
        mySerial.println("NG");
      }
      else{
        Update_RX_Data();
        Update_TX_Data();
        uint16_t unCheckSum = CRC_Maker(send, (uint8_t) (27));
        send[SEND_LENGTH - 3] = *((unsigned char *) (&unCheckSum));
        send[SEND_LENGTH - 2] = *((unsigned char *) (&unCheckSum) +1);
        
        Printstate(); //index 89 91 bit , WaterFreq, WaterTemp, SteamTemp 출력

        mySerial.write(send, SEND_LENGTH);
      }
    }
  }
}

void setup() {
  mySerial.begin(19200);
  Serial.begin(19200);
  lcd.init();
  lcd.backlight();
  pinMode(COLD_WASH_VALVE, OUTPUT);
  pinMode(HOT_WASH_VALVE, OUTPUT);
  pinMode(PRE_WASH_VALVE, OUTPUT);
  pinMode(BLEACH_VALVE, OUTPUT);
  pinMode(DRAIN_PUMP, OUTPUT);
  pinMode(STEAM_HEATER, OUTPUT);
  pinMode(STEAM_VALVE, OUTPUT);
}

void loop() {
  Virtualization_BigFL();
}
