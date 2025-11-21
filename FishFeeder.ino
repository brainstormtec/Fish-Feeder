#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"

#include <EEPROM.h>
#define EEPROM_SIZE 2 //byte


#define UNIT_FEED_TIME  300 //in Millisecond
#define DEFAULT_POWER_MODE   false

const char *service_name = "Brainstorm";
const char *pop = "Technologies";

static byte gpio_0 = 0;
static byte gpio_feeder = 13;

bool feeder_state = false;
byte feed_quntity;

static Device *my_device = NULL;

void sysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32S2
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
      WiFiProv.printQR(service_name, pop, "softap");
#else
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
      WiFiProv.printQR(service_name, pop, "ble");
#endif
      break;
    case ARDUINO_EVENT_PROV_INIT:         WiFiProv.disableAutoStop(10000); break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS: WiFiProv.endProvision(); break;
    default:                              ;
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx) {
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(param_name, "Power") == 0) {
    Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
    feeder_state = val.val.b;
    (feeder_state == false) ? digitalWrite(gpio_feeder, LOW) : digitalWrite(gpio_feeder, HIGH);
    param->updateAndReport(val);
  } else if (strcmp(param_name, "Quantity") == 0) {
    Serial.printf("\nReceived value = %d for %s - %s\n", val.val.i, device_name, param_name);
    feed_quntity = val.val.i;
    EEPROM.writeByte(0, feed_quntity); EEPROM.commit();
    param->updateAndReport(val);
  }
}

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200);
  pinMode(gpio_0, INPUT);       
  pinMode(gpio_feeder, OUTPUT); 
  digitalWrite(gpio_feeder, DEFAULT_POWER_MODE);
  feed_quntity = EEPROM.readByte(0);

  String id = String((uint32_t)(ESP.getEfuseMac()>>32),HEX) ;
  String nodeName = "Fish Feeder : " + id;
  Serial.println(nodeName);
  Node my_node;
  my_node = RMaker.initNode(nodeName.c_str());


  my_device = new Device("Feed Fish Food", "custom.device.feeder", &gpio_feeder);
  if (!my_device) {
    return;
  }
  
  my_device->addNameParam();
  my_device->addPowerParam(DEFAULT_POWER_MODE);
  my_device->assignPrimaryParam(my_device->getParamByName(ESP_RMAKER_DEF_POWER_NAME));

  Param level_param("Quantity", "custom.param.level", value(feed_quntity), PROP_FLAG_READ | PROP_FLAG_WRITE);
  level_param.addBounds(value(1), value(5), value(1));
  level_param.addUIType(ESP_RMAKER_UI_SLIDER);
  my_device->addParam(level_param);

  my_device->addCb(write_callback);

  my_node.addDevice(*my_device);

  RMaker.enableOTA(OTA_USING_TOPICS);
  RMaker.enableTZService();
  RMaker.enableSchedule();
  RMaker.enableScenes();
  RMaker.start();

  WiFi.onEvent(sysProvEvent);  
#if CONFIG_IDF_TARGET_ESP32S2
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_SOFTAP, NETWORK_PROV_SCHEME_HANDLER_NONE, NETWORK_PROV_SECURITY_1, pop, service_name);
#else
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM, NETWORK_PROV_SECURITY_1, pop, service_name);
#endif
}

void loop() {

  if (digitalRead(gpio_0) == LOW) {  
    delay(100);
    int startTime = millis();
    while (digitalRead(gpio_0) == LOW) {
      delay(50);
    }
    int endTime = millis();

    if ((endTime - startTime) > 10000) {
      Serial.printf("Reset to factory.\n");
      RMakerFactoryReset(2);
    } else if ((endTime - startTime) > 3000) {
      Serial.printf("Reset Wi-Fi.\n");
      RMakerWiFiReset(2);
    } else {
      feeder_state = !feeder_state;
      if (my_device) {
        my_device->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, feeder_state);
      }
      (feeder_state == false) ? digitalWrite(gpio_feeder, LOW) : digitalWrite(gpio_feeder, HIGH);
    }
  }
  if(feeder_state)
  {
    delay(feed_quntity*UNIT_FEED_TIME);
    feeder_state = false;

      if (my_device) {
        digitalWrite(gpio_feeder, feeder_state);
        my_device->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, feeder_state);
        delay(100);
      }
  }
  delay(100);
}
