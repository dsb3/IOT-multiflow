
// Development flags
#define USEMYQTTHUB    // bogus: remove this soon
#define FASTTIMERS

// Uncomment/comment to enable or disable different options for data upload
#define ENABLEMQTT
#define ENABLETSMETER
#define ENABLETSTEMP


// Use this file to store all of the private credentials 
// and connection details

const char* wifiName = "xxx";
const char* wifiPass = "yyy";


// ThingSpeak configuration
//
// If we wanted to squish this into one channel for any reason, we could
// push meter data starting at field 1; temp data from the top down starting
// with field 8.
//
const int   tsMeterChannel = 1043963;
const char* tsMeterKey     = "xxx";

const int   tsTempChannel  = 1043964;
const char* tsTempKey      = "xxx";



// MQTT - e.g. free low volume accounts available here
// the "ifdef USEMYQTTHUB" is just here to let me quickly switch between
// public mqtt broker (where I have usage limits), and my development
// internal one (where I don't).  It'll be removed at some time.
#ifdef USEMYQTTHUB
const char* mqttServer = "node02.myqtthub.com";
const int   mqttPort   = 1883;
#define mqttFixedIdent   "required for myqtthub!"
const char* mqttUser   = "yyy";
const char* mqttPass   = "zzz";
#else
const char* mqttServer = "other.example.com";
const int   mqttPort = 1883;
const char* mqttUser = "aaa";
const char* mqttPass = "bbb";
#endif




// Timers - all are based on millis()
//
const int tempReadFreq =     5000;  // refresh temp sensors every 5 seconds



#ifdef FASTTIMERS

// Very short timers for debugging  - upload stats much faster
const int tempSendFreq =    10000;  // 10s
const int meterSendFreq =   10000;  // 10s
const int boardSendFreq =   60000;  // 60s


#else

const int tempSendFreq =   300000;  // upload temp sensors: 5 minutes
const int meterSendFreq =  300000;  // upload meter readings: 5 minutes
const int boardSendFreq = 1800000;  // upload board stats: 30 minutes


#endif





// Specific configuration details
//
// PAY ATTENTION -- pin number refers to the actual GPIO pin id.
// In something like the NodeMCU, there is a "D#" number on the
// circuit board which you need to translate into it's corresponding
// GPIO # to enter in the configuration below.
//


// Array of meter pins that pulsed sensors are attached to.  Each one
// must be able to have an interrupt attached.
//
// No meters?  Use:  const int meterPins[] = {};
//
const int meterPins[] = {13, 14};



// EITHER: enable this for DHT22 type sensors.
//
// this both enables the DHT ifdef in our code, plus is used to
// differentiate in the library between different DHTxx models
#define DHTTYPE DHT22
const int tempPin = 12;


// OR: enable this for onewire.  Up to 8 temperature sensors on the
// same wire will be detected and used (in the order they're detected)
//    (todo: if you had to replace a temp sensor it's entirely possible
//     the replacement will detect in a different order.  being able to
//     handle this and force a specific order is a future looking idea)
//
// #define ONEWIRE
// const int tempPin = 12;


// OR: No temperature sensors?  Leave both of those #define statements
// commented out.
