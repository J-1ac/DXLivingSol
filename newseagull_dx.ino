uint16_t crc(byte buffer[110], uint8_t length){    //crc check 함수
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

void setup() {
  Serial.begin(19200); // 시리얼 통신 시작, 보드레이트 9600
}

void loop() {
  byte buffer[1000], val[110]; // 1000바이트 크기의 임시배열, 110바이트 크기의 실제 val 배열
  int mycnt=0, i=0, index=0;
  int bytesRead = Serial.readBytesUntil(170, buffer, 1000); 

  if (bytesRead > 0) { //데이터 전처리과정
    while(true){
      i++;
      if (buffer[i]==85) mycnt++;
      if (mycnt!=2) continue;
      else if(buffer[i]==170){
        val[index]=buffer[i];
        break;
      } 
      val[index]=buffer[i];
      index++; 
    }
    for(int i=0; i<110; i++){
      Serial.print(val[i]);
      Serial.print(" ");
    }
    Serial.println(); 
    Serial.print("crc: ");
    Serial.println(crc(val, 107));
  }
}