#include <ctime>
#include <SPI.h>
#include "SmartBox.h"
#include <cstdlib>


SmartBox::SmartBox() {
  std::srand(std::time(nullptr));
  this->id = std::rand() % 65000;
  //TODO: egyiknek kell tokent adni
  hasToken = false;
  volume = 0;
  //TODO: commands
  //TODO: minVolume értéke
  //TODO: id
  btle.begin("id");
}


void SmartBox::listenBeeping() {
  Serial.begin(9600);
  while (!Serial) { }
  Serial.print("Listening... ");
  
  while (true) {
    //TODO: volume beolvasása
    if (volume >= minVolume) {
      tokenRequestMsg myRequest = {this->id, this->volume, 0};
      unsigned long startTime = millis();
      //Token kérelem Küldés, fogadás
      while (millis() - startTime < 500) {
        sendTokenRequest(myRequest);
        receiveTokenRequests();
      }
      //Token átadási folyamat
      while (millis() - startTime < 1000) {
        handOverToken();
      }
      //Kérelmek törlése
      this->requests.clear();

    } else {
      if (maxHopNumber > 5) maxHopNumber = 5;
      delay(10);
    }
  }
}


//Szerializált kérelem elküldése
void SmartBox::sendTokenRequest(tokenRequestMsg request) {
  //TODO: próba, máret ellenőrzés (3 unsigned int)
  uint8_t* serializedRequest = reinterpret_cast<uint8_t*>(&request);
  nrf_service_data buf;
  buf.value = serializedRequest;
  btle.advertise(0x16, &buf, sizeof(buf));

  Serial.print("Send request: ");
  Serial.print(request.senderId); Serial.print(request.volume); Serial.print(request.hopNumber);
}


//Szerializált kérelem fogadása
void SmartBox::receiveTokenRequests() {
  if (btle.listen()) {
    Serial.print("Got payload: ");
    for (uint8_t i = 0; i < (btle.buffer.pl_size)-6; i++) { 
      Serial.print(btle.buffer.payload[i],HEX); Serial.print(" ");
    }
    
    //TODO: próba, máret ellenőrzés (3 unsigned int)
    uint8_t* payload = btle.buffer.payload;
    tokenRequestMsg* request = reinterpret_cast<tokenRequestMsg*>(&payload);
    bool duplication = false;
    for (int i = 0; i < this->requests.size(); i++) {
      if (this->requests[i].senderId == request->senderId)
        duplication = true;
    }
    //Továbbküldés
    if (!duplication) {
      this->requests.push_back(*request);
      if (request->hopNumber < this->maxHopNumber) {
        tokenRequestMsg reSentRequest = {request->senderId, request->volume, request->hopNumber + 1};
        sendTokenRequest(reSentRequest);
      }
    }
  }
}


//Token átadási protokoll
void SmartBox::handOverToken() {
  //Tokent nem birtokló szereplők (!)
  if (!hasToken) {
    waitForToken();
  }

  //Innentől: tokent birtokló szereplő (!)
  //Új token tulajdonos kiválasztása a kérelmek alapján
  tokenRequestMsg* newOwner = nullptr;
  for (int i = 0; i < this->requests.size(); i++) {
    if (this->requests[i].volume > volume) {
      newOwner = &(this->requests[i]);
    }
  }

  if (newOwner == nullptr) {
    return;
  }
  //Announce: 1. lépés
  announceMsg announce = { this->id, newOwner->senderId, 0, msgState::hearMe };
  unsigned long timeoutStart = millis();
  bool success = false;
  while (millis() - timeoutStart < 500) {
    success = sendAnnounce(announce) && receiveAnnounce(announce);
  }

  if (!success) maxHopNumber += 1;
}


//Tokent nem birtokló szereplők
void SmartBox::waitForToken() {
  unsigned long timeoutStart = millis();
  bool success = false;
  while (millis() - timeoutStart < 500) {
    answerAnnounce();
  }

  if (!success) maxHopNumber += 1;
  else {
    hasToken = true;
    sendCommand();
  }
}


bool SmartBox::sendAnnounce(announceMsg announce) {
  bool success = false;
  //TODO: próba, success, méret ellenőrzés (3 unsigned int + 1 msgState)
  uint8_t* serializedRequest = reinterpret_cast<uint8_t*>(&announce);
  nrf_service_data buf;
  buf.value = serializedRequest;
  btle.advertise(0x16, &buf, sizeof(buf));

  Serial.print("Send announce: ");
  Serial.print(announce.senderId); Serial.print(announce.receiverId); Serial.print(announce.hopNumber);
  
  success = true;
  return success;
}


bool SmartBox::receiveAnnounce(announceMsg prevAnnounce) {
  //TODO: méret ellenőrzés (3 unsigned int + 1 msgState)
  bool success = false;
  if (btle.listen()) {
    Serial.print("Got payload: ");
    for (uint8_t i = 0; i < (btle.buffer.pl_size)-6; i++) { 
      Serial.print(btle.buffer.payload[i],HEX); Serial.print(" ");
    }
    
    uint8_t* payload = btle.buffer.payload;
    announceMsg* newOwnerAnswer = reinterpret_cast<announceMsg*>(&payload);
    if (newOwnerAnswer->announceState == msgState::hearYou) {
      success = true;
      prevAnnounce.announceState = msgState::tokenExchange;
      this->hasToken = false;
      success = sendAnnounce(prevAnnounce);
    }
  }
  return success;
}


bool SmartBox::answerAnnounce() {
  bool success = false;
  if (btle.listen()) {
    Serial.print("Got payload: ");
    for (uint8_t i = 0; i < (btle.buffer.pl_size)-6; i++) { 
      Serial.print(btle.buffer.payload[i],HEX); Serial.print(" ");
    }
    
    uint8_t* payload = btle.buffer.payload;
    announceMsg* announce = reinterpret_cast<announceMsg*>(&payload);
    //TODO: id ellenőrzés különszedése + broadcasting
    if (announce->announceState == msgState::hearMe && (announce->receiverId == this->id) ) {
      announceMsg announceAnswer = { id, announce->senderId, 0, msgState::hearYou };
      sendAnnounce(announceAnswer);
    } else if (announce->announceState == msgState::tokenExchange) {
      success = true;
      hasToken = true;
    }
  } 
  return success;
}


//Parancsok végrehajtása/végrehajttatása
void SmartBox::sendCommand() {
  for (int i = 0; i < this->commands.size(); i++) {
    commandMsg command = commands[i];
    if (command.deviceId == this->id) {
      //Lokális végrehajtás, most nem kell
    } else {
      //Command üzenet kiküldése
      uint8_t* serializedRequest = reinterpret_cast<uint8_t*>(&command);
      nrf_service_data buf;
      buf.value = serializedRequest;
      btle.advertise(0x16, &buf, sizeof(buf));

      Serial.print("Send command: ");
      Serial.print(command.deviceId); 
    }
  }
}
