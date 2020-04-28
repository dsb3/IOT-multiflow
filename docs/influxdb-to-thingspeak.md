# Export data from InfluxDB to ThingSpeak

## Overview

My data natively gets transmitted via MQTT to a local broker, where it's 
ingested into Home Assistant, and from there also stored in InfluxDB.

The graphs/ directory has some in-development HighCharts apps to display
usage patterns more elegantly than I can do in Grafana.  So, instead of
adding a dependency for ThingSpeak data being sent directly from the ESP
device (which I've seen to have a much higher error rate than MQTT), I have
the option to simply exporting data from InfluxDB into a CSV format that
TS can accept.


## Script

````bash

CONTAINERID=(whatever)
SINCE=2020-04-xx
OUTFILE=conv.csv

docker exec -ti $CONTAINERID influx -precision rfc3339 -database ha \
       -execute "select max(value) from pulses WHERE entity_id = 'garage_waterflow_pulses' and time > '$SINCE' group by time(1s) fill(none)" -format csv | \
       cut -d, -f2,3 | sed -e '1icreated_at,field1' -e 's/T/ /; s/Z/ UTC/; s/\r$//; /20/!d' > $OUTFILE

````

Now, upload that CSV file into ThingSpeak to the correct channel, loading for
field1 only at the time being.

Notes: 

* the "/20/" is a simplistic look for datestamps.  Any year 20xx matches, and any
blank lines or header bumpf don't.



## Alternative historical data

If you have Home Assitant "utility meter" historical data, this can be converted
to the appropriate format like this:

````InfluxQL

select round(max(value)*450) from L WHERE entity_id = 'garage_water_daily' and time < '2020-04-27' group by time(1s) fill(none)

````


### Notes on graphs

The graph accuracy in rending the sharp edges at the end of each daily period
is dependent on having a tight population of data readings.  If the data entries
are too few then when the graph zooms out, it will look very sloppy and approximate.

This can be fixed by (future) edits to the highchart code, or (now) just duplicating
the datapoints around EOD to help keep the spline shape as we want.


