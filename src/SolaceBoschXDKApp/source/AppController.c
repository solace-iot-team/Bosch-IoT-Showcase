/*

 */
/*----------------------------------------------------------------------------*/
/**
 * @ingroup APPS_LIST
 *
 * @defgroup SEND_DATA_OVER_MQTT BCW Client - Racecar/Handheld XDK
 * @{
 *
 * @brief Solace demonstration application - MQTT connectivity - telemetry and command configuration inbound to device..
 *
 * @details Demonstration application for Solace MQTT connectivity form the device. It emits telemetry data at a configurable interval.
 * Transmission of specific sensors can also be enabled/diables by configuration
 * The application attempts to read its connectivity configuration (broker, WLAN ...) from config.json file on an SD card inserted into the XDK.
 * This configuraiton will override the defaults embedded in the code.
 *
 *
 * @file
 **/
/* module includes ********************************************************** */

/* own header files */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

/* own header files */
#include "AppController.h"

/* system header files */
#include <stdio.h>

/* additional interface header files */
#include <Serval_Mqtt.h>

#include "XDK_LED.h"
#include "BCDS_BSP_Board.h"
#include "BCDS_CmdProcessor.h"
#include "BCDS_NetworkConfig.h"
#include "BCDS_Assert.h"
#include "BCDS_Accelerometer.h"
#include "XDK_Utils.h"
#include "XDK_WLAN.h"
#include "XDK_MQTT.h"
#include "XDK_Sensor.h"
#include "XDK_SNTP.h"
#include "XDK_ServalPAL.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <time.h>
#include "cJSON.h"
#include "em_device.h"
#include "core_cm3.h"
#include "timers.h"
#include "XDK_Storage.h"

/* constant definitions ***************************************************** */

#define APP_TEMPERATURE_OFFSET_CORRECTION               (-3459)/**< Macro for static temperature offset correction. Self heating, temperature correction factor */
#define APP_RESPONSE_FROM_SNTP_SERVER_TIMEOUT       UINT32_C(2000)/**< Timeout for SNTP server time sync */

// macros to calculate ticks form milliseconds or seconds
#define MILLISECONDS(x) ((portTickType) x / portTICK_RATE_MS)
#define SECONDS(x) ((portTickType) (x * 1000) / portTICK_RATE_MS)

// MQTT configuration
#define MQTT_CONNECT_TIMEOUT_IN_MS                  UINT32_C(1000)/**< Macro for MQTT connection timeout in milli-second */
#define MQTT_SUBSCRIBE_TIMEOUT_IN_MS                UINT32_C(20000)/**< Macro for MQTT subscription timeout in milli-second */
#define MQTT_PUBLISH_TIMEOUT_IN_MS                  UINT32_C(500)/**< Macro for MQTT publication timeout in milli-second */
#define APP_MQTT_DATA_BUFFER_SIZE                   UINT32_C(1024)/**< macro for data size of incoming subscribed and published messages */
#define APP_MQTT_BASE_TOPIC						"EU/XDK"

// config file buffer and location
#define WIFI_CFG_FILE_READ_BUFFER                   UINT32_C(2048)/**< Macro for wifi config file read buffer*/
#define APP_CONFIGURATION_FILE_NAME                 "/config.json"


/* local variables ********************************************************** */


/* configuration  variables*/

// SD card storage setup
static Storage_Setup_T StorageSetup =
        {
                .SDCard = true,
                .WiFiFileSystem = false,
        };/**< Storage setup parameters */


// WLAN setup
static WLAN_Setup_T WLANSetupInfo = { .IsEnterprise = false, .IsHostPgmEnabled = false, .SSID = WLAN_SSID, .Username = WLAN_PSK, .Password = WLAN_PSK,
		.IsStatic = WLAN_STATIC_IP, .IpAddr = WLAN_IP_ADDR, .GwAddr =
		WLAN_GW_ADDR, .DnsAddr = WLAN_DNS_ADDR, .Mask = WLAN_MASK, };/**< WLAN setup parameters */

// sensor setup.initialisation
static Sensor_Setup_T SensorSetup = { .CmdProcessorHandle = NULL, .Enable = {
		.Accel = true, .Mag = true, .Gyro = true, .Humidity = true,
		.Temp = true, .Pressure = true, .Light = true, .Noise = false, },
		.Config = { .Accel = { .Type = SENSOR_ACCEL_BMI160, .IsRawData = false,
				.IsInteruptEnabled = false, .Callback = NULL, }, .Gyro = {
				.Type = SENSOR_GYRO_BMI160, .IsRawData = false, }, .Mag = {
				.IsRawData = false }, .Light = { .IsInteruptEnabled = false,
				.Callback = NULL, }, .Temp = { .OffsetCorrection =
		APP_TEMPERATURE_OFFSET_CORRECTION, }, }, };/**< Sensor setup parameters */

// SNTP setup
static SNTP_Setup_T SNTPSetupInfo = { .ServerUrl = SNTP_SERVER_URL,
		.ServerPort = SNTP_SERVER_PORT, };/**< SNTP setup parameters */

// MQTT setup and connection info
static MQTT_Setup_T MqttSetupInfo = { .MqttType = MQTT_TYPE_SERVALSTACK,
		.IsSecure = APP_MQTT_SECURE_ENABLE, };/**< MQTT setup parameters */

static MQTT_Connect_T MqttConnectInfo = { .ClientId = APP_MQTT_CLIENT_ID,
		.BrokerURL = APP_MQTT_BROKER_HOST_URL, .BrokerPort =
		APP_MQTT_BROKER_HOST_PORT, .CleanSession = false, .KeepAliveInterval =
				10000, .Username = APP_MQTT_BROKER_USERNAME, .Password = APP_MQTT_BROKER_PASSWORD};/**< MQTT connect parameters */

// MQTT publish config
static MQTT_Publish_T MqttPublishInfo = { .Topic = APP_MQTT_TOPIC, .QoS = 1UL,
		.Payload = NULL, .PayloadLength = 0UL, };/**< MQTT publish parameters */

static MQTT_Publish_T MqttPublishResponse = { .Topic = APP_MQTT_TOPIC, .QoS = 0UL,
		.Payload = NULL, .PayloadLength = 0UL, };/**< MQTT publish parameters */


// MQTT subscriber setup
// declare the callback function for subscriptions
static void subscriptionCallBack(MQTT_SubscribeCBParam_T param);

static MQTT_Subscribe_T MqttDeviceCommandSubscribeInfoSingleDevice =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */
static MQTT_Subscribe_T MqttDeviceCommandSubscribeInfoRegionLocationProductionLine =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */
static MQTT_Subscribe_T MqttDeviceCommandSubscribeInfoRegionLocation =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */

static MQTT_Subscribe_T MqttDeviceCommandSubscribeInfoRegion =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */

static MQTT_Subscribe_T MqttDeviceCommandSubscribeAll =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */



static MQTT_Subscribe_T MqttDeviceConfigurationSubscribeInfoSingleDevice =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */
static MQTT_Subscribe_T MqttDeviceConfigurationSubscribeInfoRegionLocationProductionLine =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */
static MQTT_Subscribe_T MqttDeviceConfigurationSubscribeInfoRegionLocation =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */

static MQTT_Subscribe_T MqttDeviceConfigurationSubscribeInfoRegion =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */

static MQTT_Subscribe_T MqttDeviceConfigurationSubscribeAll =
        {
                .Topic = APP_MQTT_TOPIC,
                .QoS = 0UL,
                .IncomingPublishNotificationCB = subscriptionCallBack
        };/**< MQTT subscribe parameters */


static CmdProcessor_T * AppCmdProcessor;/**< Handle to store the main Command processor handle to be used by run-time event driven threads */

static xTaskHandle ResponseHandle = NULL;/**< OS thread handle for response processing task - sending status response to broker */
static xTaskHandle AppControllerHandle = NULL;/**< OS thread handle for sampling the sensors and storing their readings */
static xTaskHandle MQTTSubscribeHandle = NULL;/**< OS thread handle for setting up the MQTT subscriptions */
static xTaskHandle TelemetryHandle = NULL;/**< OS thread handle for sending telemetry messages */

/*
 * Global var storing the device id, this is read from the device memory
 * */
static char* deviceId = NULL;

/**
 * Global var publishing topic.
 */
static char* topic = NULL;


/**
 * The queue used for signaling a response shall be sent to the broker for a configuration/command
 */
xQueueHandle responseQueue;

/* global variables ********************************************************* */

// pointer to JSON structure - samples and status response message
cJSON *root;
cJSON *responseMessage;

// time handling SNTP time and offset in ticks when the SNTP trime was obtained
uint64_t sntpTime = 0UL;
TickType_t tickOffset;

// sempahore to synchronize access to the JSON object for samples
SemaphoreHandle_t jsonPayloadHandle;


// configuration file handling
static uint8_t FileReadBuffer[WIFI_CFG_FILE_READ_BUFFER] = { 0 }; /**< Buffer to store the configuration file from the SD card / WiFi storage */
static cJSON * config = NULL;
char* baseTopic = APP_MQTT_BASE_TOPIC;

// configuration variables - govern behaviour of the appliaction - activated sensors, publishing and sampling frequency etc
uint32_t samplesPerEvent = APP_MQTT_DATA_PUBLISH_PERIODICITY/APP_MQTT_DATA_SAMPLING_PERIODICITY;

uint32_t samplingFrequency = APP_MQTT_DATA_SAMPLING_PERIODICITY;
uint32_t publishFrequency = APP_MQTT_DATA_PUBLISH_PERIODICITY;
uint32_t isLight = true;
uint32_t isAccelerator = true;
uint32_t isGyro = true;
uint32_t isMagneto = true;
uint32_t isHumidity = true;
uint32_t isTemperature = true;
uint32_t isPressure = true;



/* inline functions ********************************************************* */

/**
 * Obtain current timestamp and format to ISO Date
 */
void createTimestamp(char *date){
	// add ticks to sntp time - need to convert sntp time to milliseconds
	TickType_t ticks = xTaskGetTickCount()-tickOffset;
	uint64_t millisSinceEpoch = (uint64_t)ticks + (sntpTime*1000);

	uint64_t seconds = millisSinceEpoch / 1000;
	int millis = millisSinceEpoch % 1000;
	// now format time stamp
	time_t tt = (time_t) seconds;
	struct tm * gmTime = gmtime(&tt);
	size_t sz;
	sz = snprintf(NULL, 0, "20%02d-%02d-%02dT%02d:%02d:%02d.%03iZ",
			gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
			gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, millis);
	date = (char *) malloc(sz + 1);
	snprintf(date, sz + 1, "20%02d-%02d-%02dT%02d:%02d:%02d.%03iZ",
			gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
			gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, millis);
}

/* file configuration functions ********************************************************* */

/**
 * Read wlan config from config file and update the configuration variables
 */
void setWLANConfig(void){
	WLANSetupInfo.SSID = cJSON_GetObjectItem(config,"wlanSSID")->valuestring;
	WLANSetupInfo.Username = cJSON_GetObjectItem(config,"wlanPSK")->valuestring;
	WLANSetupInfo.Password = cJSON_GetObjectItem(config,"wlanPSK")->valuestring;
}


/**
 * Read SNTP config from config file and update the configuration variables
 */
void setSNTPConfig(void){
	SNTPSetupInfo.ServerUrl = cJSON_GetObjectItem(config,"sntpURL")->valuestring;
	SNTPSetupInfo.ServerPort = cJSON_GetObjectItem(config,"sntpPort")->valueint;
}

/**
 * Read MQTT config from config file and update the configuration variables
 */
void setMQTTConfig(void){
	MqttConnectInfo.BrokerURL = cJSON_GetObjectItem(config,"brokerURL")->valuestring;
	MqttConnectInfo.BrokerPort = cJSON_GetObjectItem(config,"brokerPort")->valueint;

	MqttConnectInfo.Username = cJSON_GetObjectItem(config,"brokerUsername")->valuestring;
	MqttConnectInfo.Password = cJSON_GetObjectItem(config,"brokerPassword")->valuestring;
	MqttConnectInfo.ClientId = cJSON_GetObjectItem(config,"mqttClientId")->valuestring;
}
/**
 * Read topic config from config file and update the configuration variables
 */
void setTopicConfig(void){
	baseTopic = cJSON_GetObjectItem(config,"baseTopic")->valuestring;
}


/**
 * Read the config file and parse to JSON
 */
void readConfigFromFile(void){
	Retcode_T retcode = RETCODE_OK;
	Storage_Read_T readCredentials =
            {
                    .FileName = APP_CONFIGURATION_FILE_NAME,
                    .ReadBuffer = FileReadBuffer,
                    .BytesToRead = sizeof(FileReadBuffer),
                    .ActualBytesRead = 0UL,
                    .Offset = 0UL,
            };
    retcode = Storage_Read(STORAGE_MEDIUM_SD_CARD, &readCredentials);
    if (retcode!= RETCODE_OK){
    	printf("Can not read configuration file, aborting file based configuration");
    	return;
    }
    config = cJSON_ParseWithOpts((const char *) FileReadBuffer, 0, 1);
    setWLANConfig();
    setSNTPConfig();
    setMQTTConfig();
    setTopicConfig();
}



/* local functions ********************************************************** */

/**
 * Declare suspendTasks fucntion - suspends all relevant in-flight tasks to avoid race conditions on global variables
 */
Retcode_T suspendTasks(void);

/**
 * Declar resumeTasks - resumes all tasks suspended by suspendTasks
 */
Retcode_T resumeTasks(void);

/**
 * Construct and emit a response status message
 */
void sendResponse(void * param1) {
	BCDS_UNUSED(param1);
	char requestType[64];
	while (1) {
		int result = xQueueReceive(responseQueue, requestType,
				MILLISECONDS(10));
		if (result) {
			responseMessage = cJSON_CreateObject();
			cJSON_AddItemToObject(responseMessage, "status",
					cJSON_CreateString("SUCCESS"));
			cJSON_AddItemToObject(responseMessage, "deviceId",
					cJSON_CreateString(deviceId));
			char* date = NULL;
			// add ticks to sntp time - need to convert sntp time to milliseconds
			TickType_t ticks = xTaskGetTickCount()-tickOffset;
			uint64_t millisSinceEpoch = (uint64_t)ticks + (sntpTime*1000);

			uint64_t seconds = millisSinceEpoch / 1000;
			int millis = millisSinceEpoch % 1000;
			// now format time stamp
			time_t tt = (time_t) seconds;
			struct tm * gmTime = gmtime(&tt);
			size_t sz;
			sz = snprintf(NULL, 0, "20%02d-%02d-%02dT%02d:%02d:%02d.%03iZ",
					gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
					gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, millis);
			date = (char *) malloc(sz + 1);
			snprintf(date, sz + 1, "20%02d-%02d-%02dT%02d:%02d:%02d.%03iZ",
					gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
					gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, millis);
			cJSON_AddItemToObject(responseMessage, "timestamp",
					cJSON_CreateString(date));
			cJSON_AddItemToObject(responseMessage, "requestType",
					cJSON_CreateString(requestType));
			char *s;
			s = cJSON_PrintUnformatted(responseMessage);
			cJSON_Delete(responseMessage);
			free(date);
			int32_t length = strlen(s);
			MqttPublishResponse.Payload = s;
			MqttPublishResponse.PayloadLength = length;
			Retcode_T retcode = MQTT_PublishToTopic(&MqttPublishResponse,
			MQTT_PUBLISH_TIMEOUT_IN_MS);
		}
		vTaskDelay(MILLISECONDS(500));
	}
}

/**
 * Callback to handle events received via MQTT subscriptions
 * One handler for configuration and command events - the function handles both event types.
 */
static void subscriptionCallBack(MQTT_SubscribeCBParam_T param) {

	//printf("Received configuration\n");
	cJSON *inComingMsg = cJSON_Parse(param.Payload);
	if (!inComingMsg) {
		printf("Error before: [%s]\n", cJSON_GetErrorPtr());
		cJSON_Delete(inComingMsg);
		return;
	}
	if (strstr(param.Topic, "configuration") == NULL) {
		printf("not a configuration message\n");
	} else {

		xQueueSend(responseQueue, "CONFIGURATION", MILLISECONDS(100));
		// wait until changes shall be implemented
		cJSON * delay = cJSON_GetObjectItem(inComingMsg, "delay");
		int delayTicks = SECONDS(delay->valueint);
		vTaskDelay(delayTicks);
		suspendTasks();
		Retcode_T retcode = RETCODE_OK;
		// set the sampling and messaging frequency
		cJSON * numberOfEventsPerSecond = cJSON_GetObjectItem(inComingMsg,
				"telemetryEventFrequency");
		cJSON * numberOfSamplesPerEvent = cJSON_GetObjectItem(inComingMsg,
				"samplesPerEvent");
		samplesPerEvent = numberOfSamplesPerEvent->valueint;
		publishFrequency = 1000 / numberOfEventsPerSecond->valueint; // convert to milliseconds
		samplingFrequency = publishFrequency / samplesPerEvent;
		printf("Set publish interval to %i ms, set sample interval to %i ms\n",
				(int) publishFrequency, (int) samplingFrequency);

		// enable/disable sensor capture
		// disable all sensors
		isLight = 0;
		isAccelerator = 0;
		isGyro = 0;
		isMagneto = 0;
		isHumidity = 0;
		isTemperature = 0;
		isPressure = 0;
		cJSON *sensors = cJSON_GetObjectItem(inComingMsg, "sensors");
		int sensorCount = cJSON_GetArraySize(sensors);
		for (int i = 0; i < sensorCount; i++) {
			cJSON* sensor = cJSON_GetArrayItem(sensors, i);
			const char *actSensor = sensor->valuestring;
			printf("Activating sensor %s\n", actSensor);

			if (strcmp(actSensor, "humidity") == 0) {
				isHumidity = 1;
			}
			if (strcmp(actSensor, "light") == 0) {
				isLight = 1;
			}
			if (strcmp(actSensor, "temperature") == 0) {
				isTemperature = 1;
			}
			if (strcmp(actSensor, "accelerator") == 0) {
				isAccelerator = 1;
			}
			if (strcmp(actSensor, "gyroscope") == 0) {
				isGyro = 1;
			}
			if (strcmp(actSensor, "magnetometer") == 0) {
				isMagneto = 1;
			}
			actSensor = NULL;
		}
		resumeTasks();
	}
	if (strstr(param.Topic, "command") == NULL) {
		printf("not a command message\n");
	} else {
		printf("received command %s\n", param.Payload);
		cJSON *inComingMsg = cJSON_Parse(param.Payload);
		if (!inComingMsg) {
			printf("Error before: [%s]\n", cJSON_GetErrorPtr());
			cJSON_Delete(inComingMsg);
			return;
		}
		cJSON * command = cJSON_GetObjectItem(inComingMsg, "command");
		if (command == NULL) {
			return;
		}
		xQueueSend(responseQueue, "COMMAND", MILLISECONDS(100));
		char *rebootCommand = strstr(command->valuestring, "REBOOT");
		if (rebootCommand != NULL) {
			cJSON * delay = cJSON_GetObjectItem(inComingMsg, "delay");
			printf("REBOOT requested, restarting in %i seconds\n",
					delay->valueint);
			vTaskDelay(SECONDS(delay->valueint));
			//BSP_Board_SoftReset();
		}
	}
	cJSON_Delete(inComingMsg);
}

/**
 *
 * Publishes telemetry data
 * Serialises the samples stored in cJSON *root variable and publishes MQTT message
 */
static void publishTelemetryMessage(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);
    char *s;
    int sampleCount=-1;
	if (xSemaphoreTake(jsonPayloadHandle,
			(TickType_t ) publishFrequency) == pdTRUE) {
		s = cJSON_PrintUnformatted(root);
		sampleCount = cJSON_GetArraySize(root);
		cJSON_Delete(root);
		root = cJSON_CreateArray();
		xSemaphoreGive(jsonPayloadHandle);
	} else {
		printf("publishing blocked\n");
		return;
	}
	int32_t length = strlen(s);
	if (length <= 975 && sampleCount >0) {
		MqttPublishInfo.Payload = s;
		MqttPublishInfo.PayloadLength = length;
		MqttPublishInfo.QoS = 0UL;
		Retcode_T retcode = MQTT_PublishToTopic(&MqttPublishInfo,
		MQTT_PUBLISH_TIMEOUT_IN_MS);
		//printf("mqtt publish successful %i\n", retcode == RETCODE_OK);
		if (RETCODE_OK != retcode) {
			vTaskSuspend(AppControllerHandle);

			int reconnected = 0;
			for (int i = 1; i <= 5; i++){
				retcode = MQTT_ConnectToBroker(&MqttConnectInfo,
						MQTT_CONNECT_TIMEOUT_IN_MS);
				if (RETCODE_OK != retcode) {
					printf(
							"publishTelemetryMessage : MQTT connection to the broker failed attempt %i\n\r", i);
				} else {
					reconnected = 1;
					break;
				}
				vTaskDelay(MILLISECONDS(i*500));
			}
			if (!reconnected) {
				printf(
						"publishTelemetryMessage : MQTT re-connection to the broker failed \n\r");
				BSP_Board_SoftReset();
			}
				printf(
						"publishTelemetryMessage : resume MQTT publishing \n\r");
			LED_Toggle(LED_INBUILT_ORANGE);
			LED_Toggle(LED_INBUILT_YELLOW);
//				Retcode_T retcode = MQTT_PublishToTopic(&MqttPublishInfo,
//				MQTT_PUBLISH_TIMEOUT_IN_MS);

			resumeTasks();
		}
		MqttPublishInfo.QoS = 1UL;
	} else
		printf("payload length %i\n", (int)length);
	free(s);
}

/**
 * Task wrapper around the publisTelemetry function
 * Publishes data at the interval specified in the configuration variables.
 */
static void publishTelemetryMessageTask(void * param1){
	while (1){
	    TickType_t startTicks = xTaskGetTickCount();
		publishTelemetryMessage(param1, 0);
	    TickType_t endTicks = xTaskGetTickCount();
		vTaskDelay(publishFrequency-(endTicks-startTicks));
	}
}



/**
 * @brief Take sensor data sample and convert to JSON, add the sample to cJSON *root. Data is published by publishTelemetry function
 *	Only addss sensor values for the sensors that are currently marked as active in the configuration variables.
 * @param[in] pvParameters
 * Unused
 */
static void SampleTask(void* pvParameters) {
	BCDS_UNUSED(pvParameters);

	Retcode_T retCode = RETCODE_OK;
    Sensor_Value_T sensorValue;
    memset(&sensorValue, 0x00, sizeof(sensorValue));
    while (1){
	    TickType_t startTicks = xTaskGetTickCount();
		retCode = Sensor_GetData(&sensorValue);
		// create the timestamp

		char *date;
		// add ticks to sntp time - need to convert sntp time to milliseconds
		TickType_t ticks = xTaskGetTickCount()-tickOffset;
		uint64_t millisSinceEpoch = (uint64_t)ticks + (sntpTime*1000);

		uint64_t seconds = millisSinceEpoch / 1000;
		int millis = millisSinceEpoch % 1000;
		// now format time stamp
		time_t tt = (time_t) seconds;
		struct tm * gmTime = gmtime(&tt);
		size_t sz;
		sz = snprintf(NULL, 0, "20%02d-%02d-%02dT%02d:%02d:%02d.%03iZ",
				gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
				gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, millis);
		date = (char *) malloc(sz + 1);
		snprintf(date, sz + 1, "20%02d-%02d-%02dT%02d:%02d:%02d.%03iZ",
				gmTime->tm_year - 100, gmTime->tm_mon + 1, gmTime->tm_mday,
				gmTime->tm_hour, gmTime->tm_min, gmTime->tm_sec, millis);
		// set up the JSON payload
		if ((int)samplesPerEvent > cJSON_GetArraySize(root)  && xSemaphoreTake(jsonPayloadHandle,
				(TickType_t ) samplingFrequency) == pdTRUE) {
			cJSON *sample;
			sample = cJSON_CreateObject();
			cJSON_AddItemToObject(sample, "timestamp",
					cJSON_CreateString(date));
			cJSON_AddItemToObject(sample, "deviceId",
					cJSON_CreateString(deviceId));
			if (isHumidity) {
				cJSON_AddNumberToObject(sample, "humidity",
						(long int ) sensorValue.RH);
			}
			if (isLight) {
				cJSON_AddNumberToObject(sample, "light",
						(long int ) sensorValue.Light);
			}
			if (isTemperature) {
				cJSON_AddNumberToObject(sample, "temperature",
						(sensorValue.Temp /= 1000));
			}
			if (isAccelerator) {
				cJSON_AddNumberToObject(sample, "acceleratorX",
						sensorValue.Accel.X);
				cJSON_AddNumberToObject(sample, "acceleratorY",
						sensorValue.Accel.Y);
				cJSON_AddNumberToObject(sample, "acceleratorZ",
						sensorValue.Accel.Z);
			}
			if (isGyro) {
				cJSON_AddNumberToObject(sample, "gyroX", sensorValue.Gyro.X);
				cJSON_AddNumberToObject(sample, "gyroY", sensorValue.Gyro.Y);
				cJSON_AddNumberToObject(sample, "gyroZ", sensorValue.Gyro.Z);
			}
			if (isMagneto) {
				cJSON_AddNumberToObject(sample, "magR", sensorValue.Mag.R);
				cJSON_AddNumberToObject(sample, "magX", sensorValue.Mag.X);
				cJSON_AddNumberToObject(sample, "magY", sensorValue.Mag.Y);
				cJSON_AddNumberToObject(sample, "magZ", sensorValue.Mag.Z);
			}
			//printf("Sample %s\n", date);
			cJSON_AddItemToArray(root, sample);
			xSemaphoreGive(jsonPayloadHandle);
		} else {
			printf("Sampling blocked\n");
		}
		free(date);
	    TickType_t endTicks = xTaskGetTickCount();
		vTaskDelay(samplingFrequency-(endTicks-startTicks));
    }
}

Retcode_T startTasks(Retcode_T retcode) {
	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(SampleTask, (const char* const ) "SamplingTask",
						TASK_STACK_SIZE_APP_CONTROLLER * 2, NULL,
						TASK_PRIO_APP_CONTROLLER, &AppControllerHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
		}
	}
	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(publishTelemetryMessageTask,
						(const char* const ) "TelemetryPublisher",
						TASK_STACK_SIZE_APP_CONTROLLER, NULL,
						TASK_PRIO_APP_CONTROLLER + 1, &TelemetryHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
		}
	}
	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(sendResponse, (const char* const ) "ResponseTask",
						TASK_STACK_SIZE_APP_CONTROLLER , NULL,
						TASK_PRIO_APP_CONTROLLER-1, &ResponseHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
		}
	}


	return retcode;
}

/**
 * Suspend active tasks - used to avoid race conoditions on globale variables
 */
Retcode_T suspendTasks(){
	Retcode_T retcode = RETCODE_OK;
	vTaskSuspend(TelemetryHandle);
	vTaskSuspend(AppControllerHandle);
	vTaskSuspend(ResponseHandle);
	return retcode;
}
/**
 * Resume all suspended tasks
 */
Retcode_T resumeTasks(){
	Retcode_T retcode = RETCODE_OK;
	vTaskResume(TelemetryHandle);
	vTaskResume(AppControllerHandle);
	vTaskResume(ResponseHandle);
	return retcode;
}

/**
 * Sets up the MQTT subscriptions for commands and configuration
 * This task cancels itself after the first execution
 */
static void MQTTSubscribe(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);
	Retcode_T retcode = RETCODE_OK;
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceCommandSubscribeInfoSingleDevice, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceCommandSubscribeInfoRegionLocationProductionLine, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceCommandSubscribeInfoRegionLocation, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceCommandSubscribeInfoRegion, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceCommandSubscribeAll, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceConfigurationSubscribeInfoSingleDevice, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceConfigurationSubscribeInfoRegionLocationProductionLine, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceConfigurationSubscribeInfoRegionLocation, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceConfigurationSubscribeInfoRegion, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    if (RETCODE_OK == retcode)
    {
        retcode = MQTT_SubsribeToTopic(&MqttDeviceConfigurationSubscribeAll, MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerFireTask : MQTT subscribe failed \n\r");
        }
    }
    while (1){
    	// wait a while then delete/cancel this task. task must not simply finish execution.
    	vTaskDelay(SECONDS(60));
    	vTaskDelete(MQTTSubscribeHandle);
    }


}



/**
 * @brief To enable the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - SNTP
 * - MQTT
 * - Sensor
 *
 * @param[in] param1
 * Unused
 *
 * @param[in] param2
 * Unused
 */
static void AppControllerEnable(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);
	vTaskDelay(pdMS_TO_TICKS(5000));
	Retcode_T retcode = LED_Setup();
	retcode = WLAN_Enable();
	if (RETCODE_OK == retcode) {
		retcode = LED_On(LED_INBUILT_RED);
	}
	if (RETCODE_OK == retcode) {
		retcode = ServalPAL_Enable();
	}
	if (RETCODE_OK == retcode) {
		retcode = SNTP_Enable();
	}
	if (RETCODE_OK == retcode) {
		retcode = MQTT_Enable();
	}

	if (RETCODE_OK == retcode) {
		retcode = Sensor_Enable();
	}
	SNTP_GetTimeFromServer(&sntpTime, 10000L);
	tickOffset = xTaskGetTickCount();

	if (RETCODE_OK == retcode) {
		retcode = MQTT_ConnectToBroker(&MqttConnectInfo,
		MQTT_CONNECT_TIMEOUT_IN_MS);
		if (RETCODE_OK != retcode) {
			printf(
					"AppControllerEnable : MQTT connection to the broker failed \n\r");
		} else {
			printf ("Connected MQTT \n");
			retcode = LED_On(LED_INBUILT_ORANGE);
		}
	} else {
		printf ("Oh snap \n");
		retcode = LED_Off(LED_INBUILT_RED);
	}
	vTaskDelay(MILLISECONDS(MQTT_CONNECT_TIMEOUT_IN_MS));

	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(MQTTSubscribe, (const char* const ) "SubscribeTask",
						TASK_STACK_SIZE_APP_CONTROLLER, NULL,
						TASK_PRIO_APP_CONTROLLER, &MQTTSubscribeHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
		}
	}


	if (RETCODE_OK == retcode) {
		retcode = startTasks(retcode);
	}
	if (RETCODE_OK != retcode) {
		printf("AppControllerEnable : Failed \r\n");
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
	Utils_PrintResetCause();
}

/**
 * @brief To setup the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - SNTP
 * - MQTT
 * - Sensor
 *
 * @param[in] param1
 * Unused
 *
 * @param[in] param2
 * Unused
 */
static void AppControllerSetup(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);
	Retcode_T retcode = LED_Setup();
	if (RETCODE_OK == retcode) {

		retcode = WLAN_Setup(&WLANSetupInfo);
	}
	if (RETCODE_OK == retcode) {
		retcode = ServalPAL_Setup(AppCmdProcessor);
	}
	if (RETCODE_OK == retcode) {
		retcode = SNTP_Setup(&SNTPSetupInfo);
	}
	if (RETCODE_OK == retcode) {
		retcode = MQTT_Setup(&MqttSetupInfo);
	}
	if (RETCODE_OK == retcode) {
		SensorSetup.CmdProcessorHandle = AppCmdProcessor;
		retcode = Sensor_Setup(&SensorSetup);
	}
	if (RETCODE_OK == retcode) {
		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerEnable,
		NULL, UINT32_C(0));
	}
	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

char* formatTopic(char* baseTopic, const char* template) {
	char* destination = NULL;
	size_t sz0;
	sz0 = snprintf(NULL, 0, template, baseTopic, deviceId);
	destination = (char*) malloc(sz0 + 1);
	snprintf(destination, sz0 + 1, template, baseTopic, deviceId);
	return destination;
}

/* global functions ********************************************************* */




/** Initialise the app - read in configuration file, set up MQTT topics, subscriptions and publishing info */
void AppController_Init(void * cmdProcessorHandle, uint32_t param2) {
	BCDS_UNUSED(param2);

	Retcode_T retcode = RETCODE_OK;

	if (cmdProcessorHandle == NULL) {
		printf("AppController_Init : Command processor handle is NULL \r\n");
		retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
	} else {
		AppCmdProcessor = (CmdProcessor_T *) cmdProcessorHandle;
		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerSetup,
		NULL, UINT32_C(0));
	}




    if (RETCODE_OK == retcode)
    {
    	retcode = Storage_Setup(&StorageSetup);
    }
    if (RETCODE_OK == retcode)
    {
    	retcode = Storage_Enable();
    }
    int storageStatus = 0;
    if (RETCODE_OK == retcode)
    {
    	retcode = Storage_IsAvailable(STORAGE_MEDIUM_SD_CARD, &storageStatus);
    }

    if ((RETCODE_OK == retcode) && (storageStatus == true)){
    	readConfigFromFile();
    }


	// set up device id and topic
	unsigned int * serialStackAddress0 = 0xFE081F0;
	unsigned int * serialStackAddress1 = 0xFE081F4;
	unsigned int serialUnique0 = *serialStackAddress0;
	unsigned int serialUnique1 = *serialStackAddress1;
	size_t sz0;
	sz0 = snprintf(NULL, 0, "%08x%08x", serialUnique1, serialUnique0);
	deviceId = (char *) malloc(sz0 + 1);
	snprintf(deviceId, sz0 + 1, "%08x%08x", serialUnique1, serialUnique0);
	MqttConnectInfo.ClientId = deviceId;
	char* theTopic = formatTopic(baseTopic, "$create/iot-event/%s/%s/metrics");
	topic = theTopic;
	MqttPublishInfo.Topic = theTopic;

	char* tempTopic = formatTopic(baseTopic, "$create/iot-control/%s/device/%s/command");
	MqttDeviceCommandSubscribeInfoSingleDevice.Topic = tempTopic;

	tempTopic = formatTopic(baseTopic, "$update/iot-control/%s/device/%s/configuration");
	MqttDeviceConfigurationSubscribeInfoSingleDevice.Topic = tempTopic;
	const char* iotControlSDeviceCommandTemplate =
			"$create/iot-control/%s/device/command";
	tempTopic =formatTopic(baseTopic,
			iotControlSDeviceCommandTemplate);
	MqttDeviceCommandSubscribeInfoRegionLocationProductionLine.Topic = tempTopic;
	const char* iotControlSDeviceConfigurationTemplate =
			"$update/iot-control/%s/device/configuration";
	tempTopic =formatTopic(baseTopic,
			iotControlSDeviceConfigurationTemplate);
	MqttDeviceConfigurationSubscribeInfoRegionLocationProductionLine.Topic = tempTopic;

	char newBaseTopic[256] = "";
	strcpy(newBaseTopic, baseTopic);
	// chop last item of basetopic
	char* lastslash;
	lastslash = strrchr(newBaseTopic, '/');
	if (lastslash !=NULL) *lastslash = '\0';

	tempTopic =formatTopic(newBaseTopic, iotControlSDeviceCommandTemplate);

	MqttDeviceCommandSubscribeInfoRegionLocation.Topic = tempTopic;

	tempTopic =formatTopic(newBaseTopic, iotControlSDeviceConfigurationTemplate);

	MqttDeviceConfigurationSubscribeInfoRegionLocation.Topic = tempTopic;

	lastslash = strrchr(newBaseTopic, '/');
	if (lastslash !=NULL) *lastslash = '\0';
	tempTopic =formatTopic(newBaseTopic, iotControlSDeviceCommandTemplate);
	MqttDeviceCommandSubscribeInfoRegion.Topic = tempTopic;
	tempTopic =formatTopic(newBaseTopic, iotControlSDeviceConfigurationTemplate);
	MqttDeviceConfigurationSubscribeInfoRegion.Topic = tempTopic;

	MqttDeviceCommandSubscribeAll.Topic = "$create/iot-control/device/command";

	MqttDeviceConfigurationSubscribeAll.Topic = "$update/iot-control/device/configuration";

	char* statusTopic = formatTopic(baseTopic, "$update/iot-control/%s/device/%s/status");
	MqttPublishResponse.Topic = statusTopic;


	responseQueue = xQueueCreate(3,64);
	if (responseQueue == NULL){
		printf("queue not created");
		retcode = RETCODE_FAILURE;
	}

	root = cJSON_CreateArray();
	jsonPayloadHandle = xSemaphoreCreateMutex();

	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

/**@} */
/** ************************************************************************* */
