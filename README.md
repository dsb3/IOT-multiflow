# Multi-Flow Notes

## Quickstart

Copy example-config.h to private-config.h and update with your own parameters.

ThingSpeak expects two channels - one for pulses, one for temp.  At another time
I might recombine them together.  The free plan counts total updates, not field
updates, so we're currently using twice as many as bare minimum by having the
two channels but I suspect it'll make data processing easier in the future.

I've noticed a lot of ThingSpeak transmission errors.  MQTT is showing itself
to be more reliable, but you need additional "stuff" set up to read the data
values sent there to store them. 

Any meter pins need to accept interrupts.  Type them into the array.

DHT22 temp sensor is tested.  Humidity is shoehorned into the second temp
slot for the time being.

OneWire temp sensors simply use as many (up to 8) that are detected on that
one data pin.  If the sensors get detected in a different order, the results
will become erratic.


### Monitoring

Watch the serial console for progress.

Watch the thingspeak pages for updates as they get sent.

To view the MQTT messages being published, use something like this.  Note that
myqtthub requires the client-id to be set specifically to the value defined.

````
$ mosquitto_sub -h node02.myqtthub.com -i CLIENT-ID -u USERNAME -P PASSWORD -v -t 'esp/#'

````

To view interim readings for both temp and meter, you can query http://(IP addr)/
for a rudimentary web page, or http://(IP addr)/all.json for JSON format data 
suitable for automatic parsing.



## Other notes

myqtthub docs here are very good to get started

https://support.asplhosting.com/t/myqtthub-en-start-here/27


## FUTURE : alerting

At the moment the data is just sent upstream for processing.

Future alerts -- temps out of range, or water flow out of expectations -- can
be added to generate alerts directly from the device.



# Charts

Charting for temps is easy as the data is in a ready to display format.

Charting for water usage is hard as the data is in raw "pulses" format which
needs to be interpreted before it can be graphed.

The graphs/ directory has sample files.


For pulse meters, we specifically choose to measure and send raw pulse data.  It's
harder to interpret but is much more resilient to data loss.  For example, you
lose accuracy in WHEN consumption took place, but not that it happened.


# Home Assistant

I have HA configured to watch the MQTT topic(s) and ingest data accordingly.

sensor:

```yaml
...
# read raw pulse data via MQTT updates
- platform: mqtt
  name: mqtt_water_pulse
  unit_of_measurement: "pulses"
  force_update: true
  state_topic: "ha/sensor/BCDDC2aabbcc/waterflow/state"

- platform: template
  sensors:
    mqtt_water_liter:
      unit_of_measurement: "L"
      availability_template: '{{ states("sensor.mqtt_water_pulse") != "unknown" }}'
      value_template: '{{ (states("sensor.mqtt_water_pulse") | float / 450.0 ) | round(2)  }}'

...
```

binary\_sensor:

```yaml
...
- platform: trend
    sensors:
      garage_water_flowing:
      device_class: moisture
      entity_id: sensor.mqtt_water_liter
      min_gradient: 0.00001    # squelch e-18 type rounding errors
...
```

and a utility\_meter

```yaml
...
garage_water_daily:
  source: sensor.mqtt_water_liter
  cycle: daily

garage_water_monthly:
  source: sensor.mqtt_water_liter
  cycle: monthly
...
```



# InfluxDB

I have HA data sent to InfluxDB automatically.  If you don't have HA in place
you can ingest the MQTT data directly with an Influx plugin

 https://github.com/influxdata/telegraf/tree/master/plugins/inputs/mqtt\_consumer


# Development Playground

My data is being pushed to these public channels:

Pulses: https://thingspeak.com/channels/1043963
Temps:  https://thingspeak.com/channels/1043964



