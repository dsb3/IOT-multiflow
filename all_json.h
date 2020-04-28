// Embed simple web server content inline to avoid SPIFFS overhead
// - Only suitable for very small files; literal R (raw string) is C++11 only
//
//
// TODO:
// - add config options; e.g. mqttServer; temp definition; fasttimers; etc
//

const char* all_json = R"({
  "macaddr": "%MACADDR%",

  "pulses": {
%ALLPULSES%
  },

  "temperatures": {
%ALLTEMPS%
  },
  
  "mqttTx": {
    "successful": "%mqttSendSuccess%",
    "failed": "%mqttSendFail%"
  },

  "thingSpeakTx": {
    "successful": "%tsSendSuccess%",
    "failed": "%tsSendFail%"
  },
  

  "uptime": "%UPTIME%",
  "millis": "%MILLIS%"
}
)";

