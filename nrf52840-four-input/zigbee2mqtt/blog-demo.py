import concurrent.futures
import paho.mqtt.client as mqtt
import json
import tinytuya

MQTT_HOST           = '192.168.X.X'
MQTT_PORT           = 1883

ttdevs={}
ttdevs['bike_stand_floods'] = ("<TT_LOCAL_ID>", "192.168.X.X", "<TT_LOCAL_KEY>")

thread_pool = concurrent.futures.ThreadPoolExecutor(max_workers=32)


#----------------------------------------------------------------------------------------------
# mqtt on_connect
#

def on_connect (self, userdata, flags, rc, trash):
  print ("Connected with result code " + str(rc))
  # Minew nRF52840 module
  result = self.subscribe ("zigbee2mqtt/0x<ZIGBEE_DEVICE_MAC>", 0)


#----------------------------------------------------------------------------------------------
# mqtt on_message
#

def on_message (self, userdata, msg):
  print (msg.topic + " " + str (msg.payload))

  # Minew nRF52840 module
  if msg.topic == "zigbee2mqtt/0x<ZIGBEE_DEVICE_MAC>":
    if msg.payload:
      parsed = json.loads (msg.payload);

      if 'action' in parsed:

        if (parsed['action'] == 'on'):
          thread_pool.submit (tuya_floods, 'bike_stand_floods', True)
        elif (parsed['action'] == 'off'):
          thread_pool.submit (tuya_floods, 'bike_stand_floods', False)


#----------------------------------------------------------------------------------------------
# turn tuya floods on or off
#

def tuya_floods (name, on):
  print ("sending " + ("on" if (on) else "off") + " request to " + name + "...")

  d = tinytuya.BulbDevice(ttdevs[name][0], ttdevs[name][1], ttdevs[name][2]) 
  d.set_version(3.3)
  d.set_socketPersistent(False)

  if on:
    d.turn_on()
    d.set_white()
  else:
    d.turn_off()

  print (("on" if (on) else "off") + " request to " + name + " done.")

  
#----------------------------------------------------------------------------------------------
# main
#

if __name__ == "__main__":

  client = mqtt.Client (client_id="", userdata=None, protocol=mqtt.MQTTv5)
  client.on_connect = on_connect
  client.on_message = on_message
  client.connect (host=MQTT_HOST, port=MQTT_PORT)

  client.loop_forever ()
