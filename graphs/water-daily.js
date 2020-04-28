<!-- <script type="text/javascript"> -->


/// TODO option
///// https://www.highcharts.com/demo/column-drilldown
//   -- daily graphs, with drilldown for hourly stats
//


// TODO:
//  -- for the incremental usage, if the incoming data is not densely populated during the end of day period
//     then our "area" or "areaspline" graph will lose it's sharp edge at the top.  For my test data I fixed it
//     by faking out repeated data in the inbound data set.  To handle this data more gracefully we should test
//     data density at the end of each day and throw a bunch of extra data points at the graph to help shape the
//     spline better.
//     - we want the slight curve of the spline, 
//     - I don't yet know how to teach the spline to not be too loose across far spaced data points
//     - you can see this if the incremetal doesn't end at a sharp point at the same level as the daily total
//       but zooming in on the day in question reverts the graph to a better shape


// TODO: 
// - add units to axis
// - split axis if we have water / energy on the same graph
//   - or consider co-ordinated one-above-the-other for water + energy
//
// - only divide by pulsePerUnit when we push the datapoint, not before, for more accurate totals.
//
//

var tsChannel = 1043963;
var tsKey     = 0; // future: enable if needed and make use of it below
var nFields = 1;   // Graph "n" fields, starting with field1
var pulsePerUnit = [450, 450, 1];  // one number per field is required; turn pulses in units


// data should be reported based on TZ of the water meter, NOT of the graph viewer, so is constant
// todo - for thingspeak we could extract lat/long of the channel and use that to determine
//
// e.g. US/Eastern = UTC - 4hrs (or 5 hrs)
//      -4 * 60 * 60 * 1000
//      -14400000
// TODO: this doesn't handle Daylight Savings changes at all.
var tzOffset = -14400000;

// JSON data provided by ThingSpeak API is this.  We need to parse/process that created_at and
// turn it into a purely numerical timestamp, in the form of "%s"
//
//    {
//      "created_at": "2020-04-24T19:57:48Z",
//      "entry_id": 224517,
//      ...
//
function getChartDate(d) {
    return Date.UTC(d.substring(0,4), d.substring(5,7)-1, d.substring(8,10), d.substring(11,13), d.substring(14,16), d.substring(17,19)) + tzOffset;
}







<!-- sample data from highcharts demo --> 
Highcharts.getJSON(
    'https://api.thingspeak.com/channels/' + tsChannel + '/feed.json?days=90',   // TODO: &key= if it's needed
    //'https://api.thingspeak.com/channels/' + tsChannel + '/feed.json?results=20',   // TODO: &key= if it's needed
    function (data) {

    // no access?
    if (data == '-1') {
       $('#chart-container').append('This channel is not public.  To embed charts, the channel must be public or an API key must be appended in the source URL.');
       window.console && console.log('ThingSpeak Data Loading Error');
     }


    // Array of data set names
    var dataNames=[];

    // Array of data set point data arrays (imported data)
    var dataSets=[];

    // Second array of data sets for running total sets per day)
    var dailySets=[];


    var period = 3600000 * 24;  // one day


    // iterate through each field (field1, field2, ... )
    for (var nF=0; nF < nFields; nF++) {

       // save this data set name
       dataNames[nF] = eval( "data.channel.field" + (nF + 1) );
       dataSets[nF] = [];
       dailySets[nF] = [];

       console.log('Working for field: ' + (nF+1) + ' of ' + nFields + ' (' + dataNames[nF] + ')');

       // iterate through each data point to process
       var last_v = 0;  // keep previous value to calc difference
       var first_v = 1; // we need to throw away the first data point.

       // while we iterate through data points, keep running daily totals
       var totalTS = 0; // timestamp of total
       var totalY  = 0; // value

       for (var nD=0; nD < data.feeds.length; nD++) {

         // Descriptive path to data element to extract.  We need to eval()
         // this because it looks like:   data.feeds[100].field1
	 var fieldStr = "data.feeds[" + nD + "].field" + (nF+1)

         // X-axis: date string converted into %s
         var x = getChartDate(data.feeds[nD].created_at);

         // v: read the value to interpret to get Y-axis
         var v = eval(fieldStr);
         var y = 0.0;  // default value of 0 is common

         // if it's not a number, skip here
         if (!isNaN(parseFloat(v))) {

           var v = parseFloat(v);   // turn into float

           // first value in the set; assume zero; 
           if (first_v > 0) {
             first_v = 0;

             // totalTS = timestamp of first period after "x"
             // -> time + period length - remainder from period length
             totalTS = x + period - ( x % period )

             // push a "0" record for the period before
             dailySets[nF].push([ totalTS - period, 0 ]);

           }
           // value < last; we reset at some time ago (also true for y == 0)
           else if (v < last_v) {
             y = v / pulsePerUnit[nF];
           }
           // else value is difference
           else {
             y = (v - last_v) / pulsePerUnit[nF];
           }

           last_v = v; 

           if (nD % 200 == 0) {
             console.log('- working for datapoint: ' + (nD+1));
             console.log(x + ", " + y);
           }

           // did we roll over our target total timestamp
           if ( x > totalTS ) {
             dailySets[nF].push([ totalTS, totalY ]);
             totalY = 0;
             totalTS += period;
           }
           // add it and continue
           totalY += y;

           // push data into graph
           // option one - per time period usage
           // dataSets[nF].push([ x, y ]);
           
           // option two - running total per time period (we're defaulting to days now)
           dataSets[nF].push([ x, totalY ]);
           

         }
        
       } // end each data point

       // push final total for the time period, if we have it
       // - plus a zero for shortly into the following time period, to make the graph look nice
       //   by "stepping" it back down.
       if (totalTS > 0) {
         dailySets[nF].push([ totalTS, totalY ]);
         dailySets[nF].push([ totalTS + period/24, 0 ]);

         // and a closing == 0 datapoint one second later for dataSets to close the graph shape.
         // TODO: can we put this final line in dotted/dashed?
         // TODO: should only do this if the last data point was NOT at 0; perhaps, not close to 0
         dataSets[nF].push([ x , 0 ]);

       }


       // Really useful for debugging, but very verbose
       //console.log('This data set:\n' + JSON.stringify(dataSets[nF]));
       //console.log('Daily data set:\n' + JSON.stringify(dailySets[nF]));

     }


     // generate a "series" object suitable for our graph
     //
     var ourSeries = [];

     // push each data set in as a series
     for (var s=0; s < nFields; s++) {

       // running totals for this field
       ourSeries.push( {
           //type: 'spline',
           name: 'Daily totals: ' + dataNames[s],
           data: dailySets[s],
           step: 'right',
            threshold: null,
            tooltip: { valueDecimals: 2 },
            fillColor: {
                linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1
                },
                stops: [
                    [0, Highcharts.getOptions().colors[0]],
                    [1, Highcharts.color(Highcharts.getOptions().colors[0]).setOpacity(0).get('rgba')]
                ]
            }
       });

       // data for this field
       ourSeries.push( {
           type: 'areaspline',
           name: dataNames[s],
           data: dataSets[s],
            threshold: null,
            tooltip: { valueDecimals: 2 },
		dataLabels: { enabled: false },
            fillColor: {
                linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1
                },
                stops: [
                    [0, Highcharts.getOptions().colors[0]],
                    [1, Highcharts.color(Highcharts.getOptions().colors[0]).setOpacity(0).get('rgba')]
                ]
            }
       });

     }

     // generate the graph with our multi-series data
     //
     Highcharts.stockChart('chart-container', {
            chart: { zoomType: 'x' },
            title: { text: 'Water Meter' },
            subtitle: {
                text: document.ontouchstart === undefined ?
                    'Click and drag in the plot area to zoom in' : 'Pinch the chart to zoom in'
            },
            legend: { enabled: true },

            // this is disabled on a per-series basis to only show on our total pseudo-columns
	    plotOptions: {
               series: {
                  dataLabels: {
                     enabled: true,
                     align: 'right',
                     formatter: function () {
                       if (!isNaN(parseFloat(this.y)) && this.y > 1) { return (this.y.toFixed(2) + " L"); }
                     }        
                  }
               }
            },

		xAxis: { ordinal: false }, // required for irregularly updated meter readings!
            yAxis: { min: 0 },

            // dynamically generated series data
	    series: ourSeries
 
        });

    

});


  
<!-- </script> -->

