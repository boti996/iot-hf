#include <RF24.h>
#include <BTLE.h>
#include <StandardCplusplus.h>
//#include <string>
#include <vector>


//Token kérelem - informáló üzenet
typedef struct {
	unsigned int senderId;
	unsigned int volume;
	unsigned int hopNumber;	
} tokenRequestMsg;


//Token átadó üzenet állapotai (3 lépés)
enum msgState {hearMe, hearYou, tokenExchange};


//Token átadó üzenet (msgState: hányadik lépés)
typedef struct {
	unsigned int senderId, receiverId;
	unsigned int hopNumber;
	msgState announceState;
} announceMsg;


//Parancs típus
enum commandType {switchRadio, checkDoor, notifyUser};


//Parancs küldése: commandType meghatározott, vagy null
//null --> végrehajtandó függvény átadása (!)
typedef struct {
	unsigned int deviceId;
	void* commandFunc;
	char** commandParams;
	commandType type;
} commandMsg;


//Eszközökön futó kódot ez tartalmazza
class SmartBox {
	private:
		unsigned int 	id;
		bool 			hasToken;
		unsigned int 	volume;
		
		std::vector<tokenRequestMsg> 	requests;
		std::vector<commandMsg> 			commands;

		unsigned int maxHopNumber = 1;
		unsigned int minVolume = 0;

   RF24 radio = RF24(9, 10);;
   BTLE btle = BTLE(&radio);

	public:
		//fő ciklus, hangjelzés figyelése
		void listenBeeping();
		//Token kérelmi üzenetek küldése, fogadása, újraküldése
		void sendTokenRequest(tokenRequestMsg);
		void receiveTokenRequests();
		//Token átadása, fogadása
		void handOverToken();
		void waitForToken();

		bool sendAnnounce(announceMsg);
		bool receiveAnnounce(announceMsg);
		bool answerAnnounce();

		//Parancsok kiadása, végrehajtása
		void sendCommand();
		//TODO: minden példánynál más inicializálás kell
		SmartBox();
};
