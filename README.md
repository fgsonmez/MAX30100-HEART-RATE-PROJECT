# MAX30100-HEART-RATE-PROJECT


## ESKISEHIR TECHNICAL UNIVERSITY - EEM449 EMBEDDED SYSTEM DESIGN FINAL PROJECT ##


Please watch the presentation video for a better review.


The project I have done contains information about I2C sensor readings, TCP and UDP
servers on ek-tm4c1294xl launchpad. The data are obtained from the MAX30100 sensor
and transmitted to the TCP server. At the same time, control of the system with 3 different
commands can be provided via TCP server. Semaphore and mailbox structures were
created for the transmission of data in the project.



I2C Sensor reading

First of all, I connected the SCL vs SDA legs correctly so that our sensor can communicate
with the launchpad. Then I wrote the necessary codes with the device ID of the sensor to be
able to communicate with the sensor. All that remains is to calibrate the sensor and extract
the correct data. In order to decide in which mode the system will operate, we only provide
HR measurement by making the capability of the 0x06 addressed mode. Later, we tackle
issues such as Sampling Rate, Led Pulse, Led Current and filter with the help of datasheet.



At the end of the project, a sensor that can measure heartbeat per minute with the
command, and a launchpad structure that can receive data from the ntp server and send data
to the server was established. With the command system, it has been provided to be easy to
control over the server.
