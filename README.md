# pb100-Weather-Station
ESP8266 web interface for Airmar PB100

Weather station that reads NMEA weather data from an Airmar PB100 senor. Note that this sensor has a poor quality temperature sensor which is prone to self heating and so an alternative is provided.
10 minute data is stored in a local rolling 1 day cache, and also (TBC) stored to flash.
Data is accessible using a web browser, served web page feartures an HTML5 web app using Bootstrap and Highcharts to display the data.

