#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h>

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
#define REMAIN_TIME 9
#define RADIUS 27
#define HEIGHT 60
#define PI 3.14
#define COLDWATER_TEMP 890  //찬물 온도
#define HOTWATER_TEMP 200   //뜨거운물 온도
#define COOLDOWN 100        // 식음 열용량
#define DRAIN_500MS 467     //배수정도

SoftwareSerial mySerial(SOFTWARE_SERIAL_RX, SOFTWARE_SERIAL_TX);
LiquidCrystal_I2C lcd(0x27, 20, 4);
LiquidCrystal_I2C wf_flowlcd(0x21, 20, 4);
uint8_t maucReceiveBuffer[RECEIVE_LENGTH];
uint8_t send[30]={85, 6, 2550/256, 2550%256, 890/256, 890%256, 800/256, 800%256, 840/256, 840%256, 825/256, 825%256, 540/256, 540%256, 960/256, 960%256, 100, 1, 11, 0, 0, 0, 0, 0, 0, 0, 0, 64, 166, 170};
uint8_t BIT_89[8]={};
uint8_t BIT_91[8]={};
uint8_t wflcd0[8] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};
uint8_t wflcd1[8] = {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10};
uint8_t wflcd2[8] = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18};
uint8_t wflcd3[8] = {0x1c,0x1c,0x1c,0x1c,0x1c,0x1c,0x1c,0x1c};
uint8_t wflcd4[8] = {0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e};
uint8_t wflcd5[8] = {0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f};
uint8_t celsius[8] = {0x3,0x3,0x0,0x0,0x0,0x0,0x0,0x0};
bool cpros=false; //현재 세탁상태
float water_volume=0; //물의 양
float temperature=890; //물온도 



int rm(int remain, int val){ // val = cwf, cwt 등, remain 값에 따라 1을 더하거나 빼줌
  if(remain>10){
    remain-=10;
    val+=1;
    }
  else if(remain<-10){
    remain+=10;
    val-=1;
  }
  return val;
}

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

void print_wf_flowlcd(uint16_t wf, uint16_t max, uint16_t min){
  uint16_t flowcnt = (max-min)/20;
  int fullflow = (wf-min)/flowcnt, remainflow = (wf-min)%flowcnt/5;
  wf_flowlcd.clear();
  for(int i=0; i<fullflow; i++){
    for(int j=0; j<=3; j++){
      wf_flowlcd.setCursor(i, j);
      wf_flowlcd.write(byte(5));
    }
  }
  for(int j=0; j<=3; j++){
    wf_flowlcd.setCursor(fullflow, j);
    wf_flowlcd.write(byte(remainflow));
  }
  return;
}

void Printstate(int timecnt){
  if(timecnt%2!=0) return;
  if(maucReceiveBuffer[3]!=1) return;
  uint16_t cwf=send[2]*256+send[3], cwt=maucReceiveBuffer[85], rpm=send[17]*256+send[18];
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WaterFreq: ");
  lcd.print(cwf);

  lcd.setCursor(0, 1);
  lcd.print("Waterwater_volume: ");
  lcd.print(water_volume/1000);
  lcd.print(" L");

  lcd.setCursor(0, 2);
  lcd.print("WashTemp: ");
  lcd.print(cwt);
  lcd.write(byte(1));
  lcd.print("C");

  lcd.setCursor(0, 3);
  lcd.print("RequestRPM: ");
  lcd.print(rpm);
  
  print_wf_flowlcd(2550-cwf, 450, 0);

  return;
}

void WaterVolumeandTemp(int timecnt) {
  if(timecnt%5!=0) return;
  int cnt=0, ccnt=0, hcnt=0;
  float static calorie=0, water_500ms=312.0;    //열용량, 들어오는 물의 양 ml
  /*물 부피*/
  if(BIT_89[0]==1) cnt++;
  if(BIT_89[2]==1) cnt++;
  if(BIT_89[3]==1) cnt++;
  if(BIT_89[4]==1) cnt++;

  water_volume+=water_500ms * float(cnt);
  if(BIT_91[5]==1) water_volume-=DRAIN_500MS;
  if(water_volume<0){
    water_volume=0;
  }
  
  /*온도*/
  if(BIT_89[2]==1) ccnt++;
  if(BIT_89[3]==1) ccnt++;
  if(BIT_89[4]==1) ccnt++;
  if(BIT_89[0]==1) hcnt++;
  calorie += water_500ms*cnt*(COLDWATER_TEMP*ccnt+HOTWATER_TEMP*hcnt)+COOLDOWN*water_volume/1000;
  if(BIT_91[5]==1) calorie-=water_500ms*4*(send[4]*256 + send[5]);
  if(calorie<0) calorie=0;
    
  temperature=calorie/water_volume;
  if(water_volume==0.0) temperature=890; //890 13도 0 105도
  Serial.print("calorie : ");
  Serial.println(calorie);  
}

void WaterTemp(int timecnt) {
  if(timecnt%5!=0) return;
  float cwt = temperature;

  if(cwt>890) cwt=890;
  if(cwt<180) cwt=180;
  send[4]=int(cwt)/256;
  send[5]=int(cwt)%256;
}

void WaterFreq(int timecnt) {
  if(timecnt%5!=0) return;
  float cwf=0;
  float temp=0;
  float min=pow(RADIUS,2)*PI*HEIGHT;

  for(float i=2550;i>=2100;i--){
    temp=abs(water_volume/HEIGHT-PI/180*pow(RADIUS,2)*acos((i-2325)/225)-RADIUS*(RADIUS-(i-2325)/9)*sin(acos((i-2325)/225)));
    if(temp<min){
      min=temp;
    }
    else{
      cwf=i+1;
      break;
    }
  }
    
  if(cwf>2550) cwf=2550; //배속으로 인한 초과를 제어
  if(cwf<2100) cwf=2100;
  
  send[2]=uint16_t(cwf)/256; 
  send[3]=uint16_t(cwf)%256;
  
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
  static int remain=0;
  ipm=rm(remain, ipm);
  remain %=10;

  if (requestRpm > 0){        //when requestRpm>0 
    if(cpros){                //탈수인 경우
      ipm += 2;
    }
    else {     
      ipm += 3.5;
    }
  } else if (requestRpm == 0 && ipm > 85){
    ipm -= 0.2;
  }

  send[6]=ipm/256; 
  send[7]=ipm%256;
  remain+=ipm*10-(send[6]*256 + send[7])*10;
  return;
}

void SteamTemp(int timecnt) {
  if(timecnt%5!=0) return;
  uint16_t stetem=send[8]*256 + send[9];
  static int remain=0;
  stetem = rm(remain, stetem);
  remain %=10;

  if(BIT_89[6]==1) stetem-=0.3;  //steam_heater ON
  if(BIT_89[1]==1) stetem+=1.5;  //steam_valve ON
  if(BIT_89[6]==0 && BIT_89[1]==0 && stetem <820) stetem +=0.2; //steam_heater/steam_valve off,Steamtemp<820

  if(stetem>840) stetem=840;
  if(stetem<260) stetem=260;

  send[8]=stetem/256;
  send[9]=stetem%256;
  remain+=stetem*10-(send[8]*256 + send[9])*10;
  return;
}

void SteamVoltage(int timecnt){
  if(timecnt%5!=0) return;
  int mode=0; uint16_t slv=send[12]*256+send[13], ssv=send[14]*256+send[15];
  if(BIT_89[6]==1){
    slv-=4.5; ssv-=4.5;
  }
  if(BIT_89[1]==1){
    slv+=4.5; ssv+=4.5;
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

void Update_LED(void){
  digitalWrite(COLD_WASH_VALVE, BIT_89[4]);
  digitalWrite(HOT_WASH_VALVE, BIT_89[0]);
  digitalWrite(PRE_WASH_VALVE, BIT_89[3]);
  digitalWrite(BLEACH_VALVE, BIT_89[2]);
  digitalWrite(DRAIN_PUMP, BIT_91[5]);
  digitalWrite(STEAM_HEATER, BIT_89[6]);
  digitalWrite(STEAM_VALVE, BIT_89[1]);
  if(maucReceiveBuffer[3]==0) digitalWrite(REMAIN_TIME, maucReceiveBuffer[96]*256+maucReceiveBuffer[97]);

  return;
}

void Update_RX_Data(void){
  send[17]=maucReceiveBuffer[13];   //update requestrpm
  send[18]=maucReceiveBuffer[14];
  Update_LED();
}

void Update_TX_Data (void) {
  static uint8_t uc100msTimer = 0;
  uc100msTimer++;
  if (uc100msTimer > 10){
  uc100msTimer = 1;
  }
  else;
  GetBit(maucReceiveBuffer[89], maucReceiveBuffer[91]);
  WaterVolumeandTemp(uc100msTimer);
  WaterFreq(uc100msTimer);
  WaterTemp(uc100msTimer);
  IPMTemp(uc100msTimer);  
  SteamTemp(uc100msTimer);
  SteamVoltage(uc100msTimer);
  Printstate(uc100msTimer); //lcd 출력
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
  wf_flowlcd.init();
  wf_flowlcd.backlight();
  wf_flowlcd.createChar(0, wflcd0);
  wf_flowlcd.createChar(1, wflcd1);
  wf_flowlcd.createChar(2, wflcd2);
  wf_flowlcd.createChar(3, wflcd3);
  wf_flowlcd.createChar(4, wflcd4);
  wf_flowlcd.createChar(5, wflcd5);
  lcd.createChar(1, celsius);
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