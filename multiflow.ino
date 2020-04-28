/******************************************************************************

  Arduino sketch to drive a multi-water meter, multi-temperature
  sensor device.

  Inputs:
  - multiple pulsed meters.
    - one meter per pin, each currently requires it to be interrupt
      driven (e.g. don't use GPIO #16 on feather huzzah since that
      specific pin doesn't support interrupts)
    - expected to be a mix of water + electric meters.
  - multiple temperature sensors.
    - either "One Wire" style such as DS18B20, where multiple sensors
      stack onto the same data pin, or for testing, a single DHT22 where
      we'll read temp + humidity in the first two sensor slots.

  Outputs:
  - on-board web server sending templated output for diags
  - data uploaded to MQTT broker
    - for this to be useful, some other system is required to receive
      and process the data, else it just gets over-written with every
      subsequent value.
  - data uploaded to thingspeak channels, one each for pulsed meters / temps

  - Temperature data is uploaded in unitless "%2.2f" floats.  Within the code
    we'll have a constant to define whether it's F, C or (in the case of the
    faked humidity sensor, "other")
  - Pulsed meter data is uploaded as raw pulses.  Logic is needed elsewhere
    to process what this means, including:
    - conversion from raw pulse to units
    - handling rollover to zero on reset


******************************************************************************/


// Include local settings, excluded from git repo (see: example-config.h for
// the file format, and expected contents)
#include "private-config.h"


// Random globals
char uptime[64];

int mqttSendSuccess = 0;
int mqttSendFail = 0;
int tsSendSuccess = 0;
int tsSendFail = 0;


// WiFi Client
// - simplifying - we don't need ESP8266WiFiMulti unless we're
//   connecting/reconnecting between multiple APs
//
#include <ESP8266WiFi.h>
WiFiClient espClient;

// save this during setup()
char macaddr[15] = "";


// Web Server
//
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// server object on tcp/http (unencrypted!)
AsyncWebServer server(80);

// static local content (to avoid SPIFFS overhead)
#include "index_html.h"
#include "all_json.h"
#include "favicon_ico.h"


// MQTT Client
//
#include <PubSubClient.h>

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // empty - this is defined, but only used for subscribing to topics
}

PubSubClient mqttClient(mqttServer, mqttPort, mqttCallback, espClient);

// generate and save during setup() - we embed our macaddr to
// differentiate devices unless our specific mqtt configuration
// requires it to be static, in which case we just do that instead.
char mqttIdent[64] = "";




// ThingSpeak client
//
#include <ThingSpeak.h>



// Temperature Sensors are one of these types:
// TODO: permit both types to be present; add a +2 offset for DHT22 to
// take slots 0, 1, and unknown count of OneWire to take slot 2 and up.

#ifdef ONEWIRE
//   "OneWire" style sensor, such as DS18B20.  Sensors contains embedded
//   serial number to allow many to stack on the same pin.
//   TODO: this code set isn't tested as I haven't freed up a DS18B20 to plug in to test with yet
#include <OneWire.h>
#include <DallasTemperature.h>

// Create global oneWire object; pass it our global Dallas Temp object
OneWire           oneWire(tempPin);
DallasTemperature TempSensors(&oneWire);

#endif


#ifdef DHTTYPE
//   DHTxx style.  These give temp+humidity, and for testing
//   we fake out the humidity sensor to be "temp #2"
//   TODO: Look at the https://github.com/RobTillaart/DHTNew library instead
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

DHT_Unified dht(tempPin, DHTTYPE);

#endif


//
// TODO: if neither is defined, abend.
//



int meterCount = 0;  // sizeof our meterPins[] array
int tempCount = 0;   // gets detected at boot time


// Pulse counter: for up to 8 meters
// using "long", not "unsigned long" due to ThingSpeak limit
// must be volatile for updates within an interrupt
volatile long pulses[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// Temperature globals; for up to 8 sensors
// only updated within loop()
float temp[8] = {0, 0, 0, 0, 0, 0, 0, 0};


// Timestamps of last activity
unsigned long lastReadTemp = 0;
unsigned long lastSentTemp = 0;
unsigned long lastSentMeter = 0;
unsigned long lastSentBoard = 0;




//=======================================================================

// https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
// Cannot take input params; therefore we have duplicated, almost identical code

void ICACHE_RAM_ATTR pulseHandler0() {
  pulses[0]++;
}

void ICACHE_RAM_ATTR pulseHandler1() {
  pulses[1]++;
}

void ICACHE_RAM_ATTR pulseHandler2() {
  pulses[2]++;
}

void ICACHE_RAM_ATTR pulseHandler3() {
  pulses[3]++;
}

// TODO: fill out for our tentative max 8 handlers




// stupid / ugly function to update our global uptime character string
void updateGlobalUptime() {

        unsigned long m = millis();

                // > 1 day
                if (m > (1000 * 60 * 60 * 24)) {
                        sprintf(uptime, "%0d day%s, %02d:%02dm",
                                (int) (m / (1000 * 60 * 60 * 24)),
                                (m >= (2 * 1000 * 60 * 60 * 24) ? "s" : ""),  // plural?
                                (int) (m / (1000 * 60 * 60) % 24),
                                (int) (m / (1000 * 60) % 60));
                }
                // > 1 hour
                else if (m > (1000 * 60 * 60)) {
                        sprintf(uptime, "%02d:%02d:%02ds",
                                (int) (m / (1000 * 60 * 60) % 24),
                                (int) (m / (1000 * 60) % 60),
                                (int) (m / 1000 % 60));
                }
                else {
                        sprintf(uptime, "%02d:%02ds",
                                (int) (m / (1000 * 60) % 60),
                                (int) (m / 1000 % 60));
                }

}


// processor function is called for our webserver output files and
// replaces tags (e.g. %STATE%) with the value returned here
//

String processor(const String& var){
	// 256 chars is large enough for 8 meter readings
	char buffer[256];
	
	if(var == "MILLIS") {
		sprintf(buffer, "%i", millis());
	}
	// TODO: millis() will rollover every ~50 days.  Use NTP here.
	else if (var == "UPTIME") {
		updateGlobalUptime();
		sprintf(buffer, uptime);
	}
	else if (var == "MACADDR") {
	sprintf(buffer, macaddr);
	}
	
	// first integer/float sensor readings - a niceness for index.html;
	// but to query all meters we need to use the JSON instead.
	else if (var == "METER1") {
		sprintf(buffer, "%i", pulses[0]);
	}
	
	// float temp readings
	else if (var == "TEMP1") {
		sprintf(buffer, "%.2f", temp[0]);
	}
	
	// all pulse sensor readings (integers)
	else if (var == "ALLPULSES") {
		strcpy(buffer, "");
		char json[64];
		
		for (int i = 0; i <= meterCount-1; i++) {
			sprintf(json, "    \"pulse%d\": \"%d\"", i, pulses[i]);
			strcat(buffer, json);
			
			// All but the last JSON line has comma, newline
			if (i != meterCount-1) { strcat(buffer, ",\n"); }
		}
	}
	// all temp sensor readings (floats)
	else if (var == "ALLTEMPS") {
		strcpy(buffer, "");
		char json[64];
		
		for (int i = 0; i <= tempCount-1; i++) {
			sprintf(json, "    \"temp%d\": \"%.2f\"", i, temp[i]);
			strcat(buffer, json);
			
			// All but the last JSON line has comma, newline
			if (i != tempCount-1) { strcat(buffer, ",\n"); }
		}
	}
	
	else if (var == "mqttSendSuccess") {
		sprintf(buffer, "%i", mqttSendSuccess);
	}
	else if (var == "mqttSendFail") {
		sprintf(buffer, "%i", mqttSendFail);
	}
	
	else if (var == "tsSendSuccess") {
		sprintf(buffer, "%i", tsSendSuccess);
	}
	else if (var == "tsSendFail") {
		sprintf(buffer, "%i", tsSendFail);
	}
	
	
	
	// Catch all for any unrecognized variable name
	else {
		sprintf(buffer, "-");
	}
	
	return buffer;
	
}

// 404 handler
void webNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}



/******************************************************************************


******************************************************************************/

void setup() {

	// Initialize the system
	Serial.begin(115200);  // Initialize serial
	
	
	// Connect to wifi
	Serial.println("Connecting to wifi ....");
	WiFi.begin(wifiName, wifiPass);
	Serial.println("");
	
	// Wait for connection to finish and print details.
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print("Connected to: ");
	Serial.println(wifiName);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	
	// Read and save mac address one time since it won't change
	char longmac[32];  // will contain colons, etc.
	sprintf(longmac, WiFi.macAddress().c_str());
	int i = 0; int j = 0; char c;
	while ((c = longmac[i++]) != '\0' && j <= 12)
		if (isalnum(c)) { macaddr[j++] = c; }
	macaddr[j] = '\0';
	
	
	Serial.print("MAC address: ");
	Serial.println(macaddr);
	
// myqtthub requires a fixed client ident to login
#ifdef mqttFixedIdent
	sprintf(mqttIdent, mqttFixedIdent);
#else
    sprintf(mqttIdent, "esp8266-%s", macaddr);
#endif
	Serial.print("MQTT Client Ident: ");
	Serial.println(mqttIdent);
	Serial.println("");
	
	
	
	// TODO: some pulsed meters may require internal pullup resistors
	// expected -- two wire, vs three wire sensors.  need hardware to test against.
	//
	// meterCount is the size of our configured meterPins[] array (divided by the
	// size of the elements).  If it's too big, we truncate.
	//
	// TODO: Sanity check meterPins[] and remove any duplicate values if they exist
	//
	// TODO: I doubt we can drive 8 interrupt driven meters off one board anyway;
	// so perhaps a limit of 4 is more reasonable.
	//
	if (sizeof(meterPins) == 0) {
		meterCount = 0;  // no pins defined
	}
	else {
		meterCount = sizeof(meterPins) / sizeof(meterPins[0]);
		
		if (meterCount > 8) {
			Serial.print("Too many meters defined (");
			Serial.print(meterCount);
			Serial.println(") - truncating to first 8");
			meterCount = 8;
		}
	}
	
	
	// meter pins is an array of pin #s all treated the same
	Serial.print("Setting INPUT_PULLUP + attachInterrupt on pulsed meter pins: ");
	Serial.println( meterCount );
	
	for (int i = 0; i < meterCount; i++) {
		Serial.print(" - ");
		Serial.print(i);
		Serial.print(" is pin ");
		Serial.println( meterPins[i] );
		pinMode(meterPins[i], INPUT_PULLUP);
	}
	
	if (meterCount >= 0)
		attachInterrupt(digitalPinToInterrupt(meterPins[0]), pulseHandler0, RISING);
	if (meterCount >= 1)
		attachInterrupt(digitalPinToInterrupt(meterPins[1]), pulseHandler1, RISING);
	
	
	
	
	// DOUBLE CHECK -- is the NodeMCU capable enough to run all of this simultaneously?
	// https://www.aliexpress.com/item/32647690484.html
	
	
	// initialize web server
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send_P(200, "text/html", index_html, processor);
	});
	
	server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send_P(200, "image/png", favicon_ico_gz, favicon_ico_gz_len);
	});
	
	server.on("/all.json", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send_P(200, "text/json", all_json, processor);
	});
	
	server.on("/healthz", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send_P(200, "text/plain", (const char*)"OK. Up %UPTIME% (%MILLIS%ms).\n", processor);
	});
	
	// otherwise, 404
	server.onNotFound(webNotFound);
	
	// Start server
	server.begin();
	
	
	
	// add mqtt client
	// (nothing needed; we connect within updateMQTT() function
	
	
	
	// initialize ThingSpeak client
	ThingSpeak.begin(espClient);
	
	
	
	// initialize temperature sensors
	
#ifdef DHTTYPE
	dht.begin();
	
	// Print temperature sensor details.
	sensor_t sensor;
	dht.temperature().getSensor(&sensor);
	
	// Try to read a value (== determine if it's present)
	sensors_event_t event;
	dht.temperature().getEvent(&event);
	if (isnan(event.temperature)) {
		Serial.println("Could not initialize DHT22 temperature sensor; disabling.");
		tempCount=0;
	}
	else {
		Serial.println(F("------------------------------------"));
		Serial.println(F("Temperature Sensor"));
		Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
		//Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
		//Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
		Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
		Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
		Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
		Serial.println(F("------------------------------------"));
		tempCount=2;
	}
	
#endif

#ifdef ONEWIRE
	TempSensors.begin();
	
	// locate devices on the bus
	Serial.print("Locating one-wire devices...");
	Serial.print("Found ");
	tempCount = TempSensors.getDeviceCount();
	Serial.print(tempCount, DEC); // DEC = decimal
	Serial.println(" sensors.");
	Serial.println("");
	
	if (tempCount == 0) {
		Serial.println("Disabling temperature monitoring - no sensors found");
	}
	
#endif
	
	// TODO: better handle the situation where temp sensors fail to register
	// at boot time.  Force a watchdog restart; check again later to see if we
	// can initialize them?  Seems harsh to require a remote reboot in case a
	// wire is flaky.
	
	
	// Sanity check values before continuing.
	if (tempCount > 8) {
		Serial.println("WARNING: Too many temp sensors detected.  Limiting to 8.");
		tempCount = 8;
	}
	
	if (meterCount > 8) {
		Serial.println("WARNING: Too many meters detected.  Limiting to 8.");
		meterCount = 8;
	}
	
	
	// Upload data immediately for water meter sensors.  Because we are
	// running the setup() function, we know that we just rebooted, so
	// if we don't send "zero" values immediately we may lose data due
	// to the easy misinterpretation of counter resets.
	//
	// e.g. counter = 300; reset; send 0; send 500;
	// versus       = 300; reset ........ send 500;
	//
	// TODO: watch for errors, instead of just fire and forget.
	if (meterCount) {
		Serial.println("Sending refreshed data for all meters == 0");
	}
	
#ifdef ENABLEMQTT
	char mqpre[64];  // TODO: I hate having to regen this every time we upload
	sprintf(mqpre, "esp/%s/pulses/val", macaddr);
	updateMQTT(mqpre, meterCount, pulses, true);
#endif
#ifdef ENABLETSMETER
	updateChannel(tsMeterChannel, tsMeterKey, meterCount, pulses);
#endif



	// TODO:
	// meterCount == 0 && tempCount == 0 ... report that there's nothing to do and error out
	//
}





/******************************************************************************


******************************************************************************/

void loop() {
	
	// Poll sensors to refresh all temperature readings
	// - Note that we do this more often than we upload results
	// - If needed, the device can be queried for real-time stats
	if (tempCount > 0 && millis() - lastReadTemp > tempReadFreq) {
	
#ifdef ONEWIRE
		Serial.print("Reading one-wire temperature sensors ... ");
		TempSensors.requestTemperatures();
		Serial.println("DONE");
		
		for(uint8_t i = 0; i < tempCount; i++) {
			Serial.print("Temperature for sensor #");
			Serial.print(i+1);
			Serial.print(" is: ");
			// TODO: select C/F based on config
			temp[i] = TempSensors.getTempFByIndex(i);
			Serial.println(temp[i]); 
		}
#endif
	
#ifdef DHTTYPE
		Serial.print("Reading DHT temperature sensors ... ");
		sensors_event_t event;
		dht.temperature().getEvent(&event);
		
		if (! isnan(event.temperature)) {
			temp[0] = (event.temperature * 1.8) + 32;
		}
		// else: how to declare the value is invalid?
		
		// repeat for humidity, faking it out as temp[1]
		dht.humidity().getEvent(&event);
		if (! isnan(event.relative_humidity)) {
			temp[1] = event.relative_humidity;
		}
		
		Serial.println("DONE");
#endif
	
		// TODO: do not overwrite if value(s) weren't valid
		lastReadTemp = millis();
	}
	
	
	// Send temperature readings on the temp schedule
	if (tempCount > 0 && millis() - lastSentTemp > tempSendFreq) {
		
		char mqpre[64];  // TODO: I hate having to regen this every time we upload
		sprintf(mqpre, "esp/%s/temps/val", macaddr);
		
		// update textual uptime with fresh timestamp
		updateGlobalUptime();
		
		// Debug
		Serial.print("Uptime: ");
		Serial.print( uptime ) ;
		
		Serial.println("  -  Sending Temperature Readings:");
		
		for (int i = 0; i < tempCount; i++) {
			Serial.print("Temp "); Serial.print(i); Serial.print(": ");
			Serial.print(temp[i]); Serial.println(" F.");
		}
		
		
		int resMQTT = 1;
		int resTS = 1;

#ifdef ENABLEMQTT
		/// send measurements to MQTT broker
		resMQTT = updateMQTT(mqpre, tempCount, temp, true);
#endif

#ifdef ENABLETSTEMP
		/// send measurements to ThingSpeak channel
		resTS = updateChannel(tsTempChannel, tsTempKey, tempCount, temp);
#endif

		// unless both were successful, reschedule the next update to happen sooner
		if (resTS && resMQTT) {
			lastSentTemp = millis();
		}
		else {
			lastSentTemp += int(tempSendFreq / 4);
		}
		
	}
	
	
	
	// TODO: examine moving average meter readings and alert if
	// growth over time indicates out of range usage
	
	// Send all pulse meter readings on the meter schedule
	if (meterCount > 0 && millis() - lastSentMeter > meterSendFreq) {
		
		char mqpre[64];  // TODO: I hate having to regen this every time we upload
		sprintf(mqpre, "esp/%s/pulses/val", macaddr);
		
		// update textual uptime with fresh timestamp
		updateGlobalUptime();
		
		// Debug
		Serial.print("Uptime: ");
		Serial.print( uptime ) ;
		
		Serial.println("  -  Sending Meter Readings:");
		
		for (int i = 0; i < meterCount; i++) {
			Serial.print("Meter ");  Serial.print(i); Serial.print(": ");
			Serial.print(pulses[i]); Serial.println(" pulses.");
		}
		
		int resMQTT = 1;
		int resTS = 1;

#ifdef ENABLEMQTT
		resMQTT = updateMQTT(mqpre, meterCount, pulses, true);
#endif

#ifdef ENABLETSMETER
		/// send measurements to ThingSpeak channel
		resTS = updateChannel(tsMeterChannel, tsMeterKey, meterCount, pulses);
#endif

		// unless both were successful, reschedule the next update to happen sooner
		if (resTS && resMQTT) {
			lastSentMeter = millis();
		}
		else {
			lastSentMeter += int(meterSendFreq / 4);
		}
		
	}
	
	
	// Send board stats on the board schedule
	if (millis() - lastSentBoard > boardSendFreq) {
		
		// update textual uptime with timestamp
		updateGlobalUptime();
		
		// Debug
		Serial.print("Uptime: ");
		Serial.print( uptime ) ;
		
		Serial.println("  -  Updated Board Stats:");
		
		// TODO: array-ify these
		Serial.print("Millis: ");
		Serial.println(millis());
		Serial.print("MAC Addr: ");
		Serial.println(macaddr);
		
		Serial.print("MQTT data updates:  ");
		Serial.print( mqttSendSuccess );
		Serial.print(" successful; ");
		Serial.print( mqttSendFail );
		Serial.println(" failed.");
		
		Serial.print("ThingSpeak data updates: ");
		Serial.print( tsSendSuccess );
		Serial.print(" successful; ");
		Serial.print( tsSendFail );
		Serial.println(" failed.");
		
		/// future  -- send measurements to MQTT.
		/// for now -- just print to Serial, or have available via webserver
		
		
		/// assume success
		lastSentBoard = millis();
		
	}
	
	
	
	
	// TODO: do we need mqttclient.loop() when we're not subscribed to anything?
	mqttClient.loop();
	
	yield();
	
}


// TODO: 
// We frequently see error "-303" cannot parse response.  No idea why yet.
// From observation, we can re-use the same object.  After writeFields() is called,
// I presume all earlier setField values are cleared to start fresh.


// function for floats - temp
int updateChannel(const int thisChannel, const char* writeKey, int count, float* floatVals) {
	
	for (int i = 0; i < count; i++) {
	  ThingSpeak.setField(i + 1, floatVals[i]);
	}
	
	int res = ThingSpeak.writeFields(thisChannel, writeKey);
	
	if (res == 200) {
		tsSendSuccess++;
		return 1;
	}
	else {
		Serial.print("ThingSpeak err: ");
		Serial.println(res);
		tsSendFail++;
		return 0;
	}
	
}


// function for ints - pulses
int updateChannel(const int thisChannel, const char* writeKey, int count, volatile long* intVals) {
	for (int i = 0; i < count; i++) {
	  ThingSpeak.setField(i + 1, intVals[i]);
	}
	
	int res = ThingSpeak.writeFields(thisChannel, writeKey);
	
	if (res == 200) {
		tsSendSuccess++;
		return 1;
	}
	else {
		Serial.print("ThingSpeak err: ");
		Serial.println(res);
		tsSendFail++;
		return 0;
	}

}



// function for floats - temps
int updateMQTT(char* prefix, int count, float* floatVals, boolean retain) {
	char topic[64] = "";
	char payload[64] = "";
	int res = mqttClient.connect(mqttIdent, mqttUser, mqttPass);
	
	if (!res) {
		// re-use strings for error message
		sprintf(topic, "connection error");
		sprintf(payload, "%i", res);
		res = 0;
	}
	else {
		for (int i = 0; i < count; i++) {
			// topic is our prefix + int
			sprintf(topic, "%s%i", prefix, i);
			sprintf(payload, "%.2f", floatVals[i]);   // topic differs in format
			if (mqttClient.publish(topic, payload, retain)) { mqttSendSuccess++; }
			else { mqttSendFail++; res = 0; }
		}
	}
	
	if (! res) {
		Serial.print("MQTT sending failed for: ");
		Serial.print(topic);
		Serial.print(" = ");
		Serial.println(payload);
	}
	
	return res;
	
}

// ints
int updateMQTT(char* prefix, int count, volatile long* intVals, boolean retain) {
	char topic[64] = "";
	char payload[64] = "";
	
	int res = mqttClient.connect(mqttIdent, mqttUser, mqttPass);
	
	if (!res) {
		// re-use strings for error message
		sprintf(topic, "connection error");
		sprintf(payload, "%i", res);
		res = 0;
	}
	else {
		for (int i = 0; i < count; i++) {
			// topic is our prefix + int
			sprintf(topic, "%s%i", prefix, i);
			sprintf(payload, "%i", intVals[i]);   // topic differs in format
			if (mqttClient.publish(topic, payload, retain)) { mqttSendSuccess++; }
			else { mqttSendFail++; res = 0; }
		}
	}
	
	if (! res) {
		Serial.print("MQTT sending failed for: ");
		Serial.print(topic);
		Serial.print(" = ");
		Serial.println(payload);
	}
	
	return res;
	
}

