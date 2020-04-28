// Embed simple web server content inline to avoid SPIFFS overhead
// - Only suitable for very small files; literal R (raw string) is C++11 only
//
// This is deprecated to almost zero use.  All data should be queried directly
// via the all.json template.
//

const char* index_html = R"(

<!DOCTYPE html>
<!-- 
  Web front end for diags.  Integration should use /all.json
-->
<html>
<head>
  <title>Multi-flow Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
<style>
html {
  /*font-family: Arial;*/
  display: inline-block;
  margin: 0px auto;
  text-align: center;
}
h1 {
  color: #0F3376;
  padding: 2vh;
}
.sensor-labels {
  vertical-align: middle;
  padding-bottom: 15px;
}
</style>


</head>
<body>
  <h1>Multi-flow Web Server</h1>

  <p>
    <span class="sensor-labels">Meter 1:</span>
    <span>%METER1%</span>
    <sup>pulses</sup>
  </p>
  <!-- for all meters, use the JSON output -->

  <p>
    <span class="sensor-labels">Temperature 1:</span>
    <span>%TEMP1%</span>
    <sup>F</sup>
  </p>
  <!-- for all temps, use the JSON output -->

  <p>
    <span class="sensor-labels">Uptime: </span>
    <span>%UPTIME%</span>
  </p>
</body>


</html>

)";
