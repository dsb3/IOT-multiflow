<!-- <script type="text/javascript"> -->

var tsChannel = 1043964;
var tsKey     = 0;
var nFields = 2;   // Graph "n" fields, starting with field1



// JSON data provided by ThingSpeak API is this.  We need to parse/process that created_at and
// turn it into a purely numerical timestamp, in the form of "%s"
//
//    {
//      "created_at": "2020-04-24T19:57:48Z",
//      "entry_id": 224517,
//      ...
//
function getChartDate(d) {
    return Date.UTC(d.substring(0,4), d.substring(5,7)-1, d.substring(8,10), d.substring(11,13), d.substring(14,16), d.substring(17,19));
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

    // Array of data set point data arrays
    var dataSets=[];


    // iterate through each field (field1, field2, ... )
    for (var nF=0; nF < nFields; nF++) {

       // save this data set name
       dataNames[nF] = eval( "data.channel.field" + (nF + 1) );
       dataSets[nF] = [];

       console.log('Working for field: ' + (nF+1) + ' of ' + nFields + ' (' + dataNames[nF] + ')');

       // iterate through each data point
       for (var nD=0; nD < data.feeds.length; nD++) {

         // Descriptive path to data element to extract.  We need to eval()
         // this because it looks like:   data.feeds[100].field1
	 var fieldStr = "data.feeds[" + nD + "].field" + (nF+1)

         // X: date
         var x = getChartDate(data.feeds[nD].created_at);

         // Y: value
         var y = eval(fieldStr);

         if (nD % 500 == 0) {
           console.log('- working for datapoint: ' + (nD+1));
           console.log(x + ", " + y);
         }

         // check y value is valid (not undef), and append to dataSet matching nF
         if (!isNaN(parseFloat(y))) {
           dataSets[nF].push([ x, parseFloat(y) ]);
         }
        
       } // end each data point

       // console.log('This data set:\n' + JSON.stringify(dataSets[nF]));

     }


     // generate a "series" object suitable for our graph
     //
     var ourSeries = [];

     // push each data set in as a series
     for (var s=0; s < nFields; s++) {

       ourSeries.push( {
           type: 'area',
           name: dataNames[s],
           data: dataSets[s]
              });
     }

     // generate the graph with our multi-series data
     //
     Highcharts.chart('chart-container', {
            chart: { zoomType: 'x' },
            title: { text: 'Temperature Graphs' },
            subtitle: {
                text: document.ontouchstart === undefined ?
                    'Click and drag in the plot area to zoom in' : 'Pinch the chart to zoom in'
            },
            xAxis: { type: 'datetime' },
            yAxis: {
                title: { text: 'Temperature' }
            },
            legend: { enabled: true },
            plotOptions: {
                area: {
                    fillColor: {
                        linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1 },
                        stops: [
                            [0, Highcharts.getOptions().colors[0]],
                            [1, Highcharts.color(Highcharts.getOptions().colors[0]).setOpacity(0).get('rgba')]
                        ]
                    },
                    marker: { radius: 2 },
                    lineWidth: 1,
                    states: {
                        hover: { lineWidth: 1 }
                    },
                    threshold: null
                }
            },

            // dynamically generated series data
	    series: ourSeries
 
        });

    

});


  
<!-- </script> -->

