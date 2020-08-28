#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "MQ135.h"
#include <string.h>
//Wifi Manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>     

#include <FirebaseESP8266.h>
#define FIREBASE_HOST "https://air-pollution-3ae9f.firebaseio.com/"
#define FIREBASE_AUTH "0lZIUXRG5JyCDo928qxRy3AWwYNeT1qsAfzrZTaw"
FirebaseData firebaseData;

int dht_pin = 5;
DHT dht(dht_pin, DHT11);

float h, t, f;
float rzero;


//const char *ssid = "THREE O'CLOCK";
//const char *password = "3open24h";
const char *mqtt_server = "192.168.1.124";

WiFiClient espClient;
PubSubClient client(espClient);

#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

char*result="/";

unsigned long now;
unsigned long lastMeasure = 0;
unsigned long freq = 3000;
void setup_wifi()
{

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  configWifiManager();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  char buffer[128];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';

  // Convert it to integer
  char *end = nullptr;
  long value = strtol(buffer, &end, 10);

  if (end == buffer)
    ; 
  else
    Serial.println(value);
  Serial.print("payload: ");
  Serial.println(value);
  freq = value*1000;
}

void reconnect()
{

  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
 
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  configFirebase();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  now = millis();
  client.subscribe("room/frequencyout");


  if (now - lastMeasure > freq)
  {
    lastMeasure = now; 
    readTem();
    readMQ135(); 
    updataServer();
    if(rzero > 700) {
      client.publish("Warning Pollution", "canh bao khi gas vuot nguong an toan"); 
    }
  }
  
}

void readTem()
{
  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f))
  {
    Serial.println("Failed to read from DHT sensor!");
    h = 0;
    t = 0;
    return;
  }
  Serial.println("Humidity: ");
  Serial.println(h);
  Serial.println("Temperature: ");
  Serial.println(t);

  float hic = dht.computeHeatIndex(t, h, false);
  
  static char temperatureTemp[7];
  dtostrf(hic, 6, 2, temperatureTemp);

  static char humidityTemp[7];
  dtostrf(h, 6, 2, humidityTemp);

  client.publish("room/temp", temperatureTemp);
  client.publish("room/humidity", humidityTemp);

}

void readMQ135()
{
  MQ135 gasSensor = MQ135(A0); // Attach sensor to pin A0
  rzero = gasSensor.getRZero();
  Serial.println("Gas: ");
  Serial.println(rzero);
  static char gasTemp[7];
  dtostrf(rzero, 6, 2, gasTemp);
  client.publish("room/gas", gasTemp);
}

void configWifiManager() {
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("TREESS")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  WiFiManager wifiManager;
  Serial.print("Stored SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("Stored passphrase: ");
//  Serial.println(myWiFiManager->getPassword());
}

void configFirebase() {
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  if (!Firebase.beginStream(firebaseData, "/Nodes/")) {
    Serial.println("Could not beigin stream");
    Serial.println("REASON: " + firebaseData.errorReason());
    Serial.println();
  }
}
void updataServer() {
  char buf[16];
  ltoa(now,buf,10);
  char*result1="/";
  char*result2="/"; 
  char*result3="/";   
  
  strcpy( result1,buf);
  strcat(result1,"/temp");
  Firebase.setInt(firebaseData,result1, t);

  strcpy(result2,buf);
  strcat(result2,"/humidity");
  Firebase.setInt(firebaseData,result2, h);
  
  strcpy(result3,buf);
  strcat(result3,"/gas");
  Firebase.setInt(firebaseData,result3, rzero);
  
}
