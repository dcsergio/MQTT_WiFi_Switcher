#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <PubSubClient.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//define your default values here, if there are different values in config.json, they are overwritten.
#define ONE_MINUTE 60000
boolean status = false;
String stateString = "0";
unsigned long lastSentTime = 0;
int minutes = -1;
char charBuf[10];

char mqttServer[40] = "m23.cloudmqtt.com";
char mqttPort[6] = "12873";
char mqttUser[40] = "rptjayur";
char mqttPassword[40] = "VybhGdceNgzn";
char currentStateTopic[40] = "home/heater/state";
char commandTopic[40] = "home/heater/command";
char clientName[40] = "ESP";
//flag for saving data
bool shouldSaveConfig = false;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);
  Serial.begin(115200);
  delay(10000);
  readFS();
  wifiSetup(true);
  connectMQTT();
  saveConfig();
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  lastSentTime = millis();
}

void connectMQTT() {
  int port = atoi(mqttPort);
  Serial.print("MQTT port is: ");
  Serial.println(port);
  client.setServer(mqttServer, port);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT: " + String(mqttUser) + " " + String(mqttPassword));
    if (client.connect(clientName, mqttUser, mqttPassword)) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  client.subscribe(currentStateTopic, 1);
  client.subscribe(commandTopic, 1);
}

void callback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if (length > 0) {
    if (strcmp(topic, currentStateTopic) == 0) {
      if (payload[0] == '1') {
        switchOn();
      } else {
        switchOff();
      }
      if (payload[0] == 'c') {
        wifiSetup(false);
        connectMQTT();
        saveConfig();
      }
    } else if (strcmp(topic, commandTopic) == 0) {
      payload[length] = '\0'; // Make payload a string by NULL terminating it.
      minutes = atoi((char *)payload);
      Serial.println("timer set to: " + String(minutes));
    }
  }
}


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void switchOn() {
  if (stateString != "1") {
    digitalWrite(LED_BUILTIN, true);
    Serial.println("ON status");
    stateString = "1";
    publishString(stateString, currentStateTopic);
  }
}

void toggle() {
  if (stateString == "0") {
    switchOn();
  } else if (stateString == "1") {
    switchOff();
  }
}

void switchOff() {
  if (stateString != "0") {
    digitalWrite(LED_BUILTIN, false);
    Serial.println("OFF status");
    stateString = "0";
    publishString(stateString, currentStateTopic);
  }
}

void publishString(String m, char* topic) {
  m.toCharArray(charBuf, 10);
  client.publish(topic, charBuf, true);
}

void readFS() {
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqttServer, json["mqttServer"]);
          strcpy(mqttPort, json["mqttPort"]);
          strcpy(mqttUser, json["mqttUser"]);
          strcpy(mqttPassword, json["mqttPassword"]);
          strcpy(currentStateTopic, json["currentStateTopic"]);
          strcpy(commandTopic, json["commandTopic"]);
          strcpy(clientName, json["clientName"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}

void wifiSetup(bool autoConn) {
  WiFiManager wifiManager;
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqttServer, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqttPort, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqttUser, 40);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqttPassword, 40);
  WiFiManagerParameter custom_mqtt_command_topic("command_topic", "Command topic", commandTopic, 40);
  WiFiManagerParameter custom_mqtt_state_topic("state_topic", "State topic", currentStateTopic, 40);
  WiFiManagerParameter custom_mqtt_client_name("client_name", "Client unique name", clientName, 40);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_command_topic);
  wifiManager.addParameter(&custom_mqtt_state_topic);
  wifiManager.addParameter(&custom_mqtt_client_name);

  if (autoConn) {
    if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      ESP.reset();
      delay(5000);
    }
  } else {
    if (!wifiManager.startConfigPortal("OnDemandAP")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      ESP.reset();
      delay(5000);
    }
  }
  Serial.println("connected...yeey :)");
  strcpy(mqttServer, custom_mqtt_server.getValue());
  strcpy(mqttPort, custom_mqtt_port.getValue());
  strcpy(mqttUser, custom_mqtt_user.getValue());
  strcpy(mqttPassword, custom_mqtt_password.getValue());
  strcpy(currentStateTopic, custom_mqtt_state_topic.getValue());
  strcpy(commandTopic, custom_mqtt_command_topic.getValue());
  strcpy(clientName, custom_mqtt_client_name.getValue());
}

void saveConfig() {
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqttServer"] = mqttServer;
    json["mqttPort"] = mqttPort;
    json["mqttUser"] = mqttUser;
    json["mqttPassword"] = mqttPassword;
    json["currentStateTopic"] = currentStateTopic;
    json["commandTopic"] = commandTopic;
    json["clientName"] = clientName;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println();
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void loop() {
  if (!client.connected()) {
    connectMQTT();
    publishString("r", currentStateTopic);
  }
  long gap = millis() - lastSentTime;
  if (gap >= ONE_MINUTE || gap < 0) {
    Serial.println("gap: " + String(gap));
    lastSentTime = millis();
    if (minutes > 0) {
      minutes --;
      Serial.println("remaing minutes to toggle: " + String(minutes));
      Serial.println("            current state: " + stateString);
      if (minutes == 0) {
        toggle();
      }
    }
    publishString(String(minutes), commandTopic);
  }
  client.loop();
}
