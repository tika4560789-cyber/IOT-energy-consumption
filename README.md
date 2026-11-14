# IOT-energy-consumption
**Overview**  

Conventional electricity meters only give you a monthly total. They don't show when energy is used or which appliance is driving the bill. This project builds a low-cost prototype that makes consumption visible in real time and keeps a historical record for later analysis.

The ESP32 reads an AC voltage sensor (ZMPT101B) and an AC current sensor (ACS712), computes RMS voltage, RMS current, power and accumulated energy, and publishes the results as JSON over MQTT. Node-RED subscribes to the stream, renders live gauges and charts, and inserts each reading into a PostgreSQL database.

**Objectives**  

Build a working smart energy meter prototype on the ESP32.
Measure voltage and current with the ZMPT101B and ACS712 sensors.
Send the computed data wirelessly via MQTT to a cloud broker (HiveMQ Cloud).
Visualise the data in real time with a Node-RED dashboard.
Store readings in PostgreSQL for historical logging.
Evaluate performance, identify limitations and propose improvements.

**System Architecture**

The system uses a publish–subscribe model. The ESP32 is the edge node (publisher). Node-RED is the gateway (subscriber) and runs two parallel paths: one to the dashboard, one to the database.

MQTT over TLSenergy_meter/data
ZMPT101BVoltage sensor
ESP32
ACS712Current sensor
HiveMQ CloudBroker
Node-RED
Dashboardgauges & charts
PostgreSQL
Component	Role
ZMPT101B	AC voltage measurement with transformer isolation
ACS712	AC current measurement using the Hall effect
ESP32	ADC sampling, RMS calculation, power/energy computation, MQTT publishing
HiveMQ Cloud	MQTT broker for message routing and delivery
Node-RED	MQTT subscription, data processing, dashboard rendering
PostgreSQL	Persistent storage for historical analysis

**Features**

Real-time measurement of voltage (V), current (A), power (W) and energy (kWh)
RMS calculation from 100 ADC samples per reading (about one 50 Hz cycle)
20-sample moving-average filter to smooth readings
Secure MQTT connection to HiveMQ Cloud on port 8883 (TLS)
Remote energy reset over MQTT (RESET_ENERGY command)
Automatic Wi-Fi and MQTT reconnection
Live Node-RED dashboard accessible from any browser
Every reading logged to PostgreSQL, with credentials kept in environment variables
Modular design: the dashboard and database logic can change without touching the firmware

**Hardware Required**

ESP32 development board	Powered over micro-USB
ZMPT101B voltage sensor module	Connected to the AC mains
ACS712 current sensor module (20A version)	Connected in series with the load
Breadboard and jumper wires	For prototyping
Test load	e.g. a lamp, fan or charger
Wiring
Sensor	Sensor pin	ESP32 pin
ZMPT101B	OUT	GPIO 34
ACS712	OUT	GPIO 35
Both	VCC / GND	Per each module's supply requirements

Safety warning: This project connects to mains AC voltage. Use proper insulation and enclosures, never touch live wiring, disconnect power before changing connections, and have an experienced person check your setup before powering it on.

ADC note: The ESP32 ADC accepts a maximum of about 3.3 V. If your ACS712 module is powered at 5 V, its output can exceed that at higher currents, so consider a voltage divider on the output line.

**Software Requirements**

Arduino IDE with the ESP32 board package installed
Arduino libraries: PubSubClient, ArduinoJson
WiFi and WiFiClientSecure (included with the ESP32 core)
Node-RED: node-red-dashboard, node-red-contrib-postgresql
PostgreSQL
HiveMQ Cloud cluster



**Results**

The prototype demonstrated a complete end-to-end data flow:

✅ Sensing: both sensors captured AC voltage and current.  
✅ Processing: the ESP32 computed RMS voltage, RMS current, power and cumulative energy.  
✅ Transmission: readings were reliably published to HiveMQ Cloud via MQTT.  
✅ Visualisation: the Node-RED dashboard updated in real time.  
✅ Storage: every reading was inserted into PostgreSQL.

**Key findings**

MQTT works well: low latency and reliable delivery suit IoT energy monitoring.
Modular design helps: because Node-RED is decoupled from the ESP32, data processing can change without reflashing the firmware.
Remote access is practical: the dashboard can be viewed from any browser-enabled device.
The database adds real value: it turns a simple monitor into a platform for historical analysis and reporting.

**Limitations**

Sensor accuracy: low-cost sensors (ACS712, ZMPT101B) introduce small deviations. The system is good for trend tracking, not precision metering.
Power calculation: power is computed as V × I, so it does not account for power factor. Readings for inductive or non-linear loads may differ from true real power.
Network dependency: if Wi-Fi or the HiveMQ broker is unavailable, readings are not stored. Local buffering would be needed for mission-critical use.
Certificate validation: the current firmware uses TLS but skips server certificate verification (setInsecure()).
