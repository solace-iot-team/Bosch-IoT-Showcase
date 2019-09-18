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
#include <BCDS_WlanConnect.h>
#include "BCDS_Assert.h"
#include "BCDS_Accelerometer.h"
#include "XDK_Utils.h"
#include "XDK_WLAN.h"
#include "XDK_MQTT.h"
#include "XDK_Sensor.h"
#include "XDK_SNTP.h"
#include "XDK_ServalPAL.h"
#include "FreeRTOS.h"
#ifndef NDEBUG_XDK_APP_TASK_STATE
#include "FreeRTOSConfig.h"
#endif
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
#define MQTT_CONNECT_TIMEOUT_IN_MS                  UINT32_C(5000)/**< Macro for MQTT connection timeout in milli-second */
#define MQTT_SUBSCRIBE_TIMEOUT_IN_MS                UINT32_C(20000)/**< Macro for MQTT subscription timeout in milli-second */
#define MQTT_PUBLISH_TIMEOUT_IN_MS                  UINT32_C(500)/**< Macro for MQTT publication timeout in milli-second */
#define APP_MQTT_DATA_BUFFER_SIZE                   UINT32_C(1024)/**< macro for data size of incoming subscribed and published messages */
#define APP_MQTT_BASE_TOPIC							"EU/XDK"

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

//static xTaskHandle ResponseHandle = NULL;/**< OS thread handle for response processing task - sending status response to broker */
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
uint8_t samplesPerEvent = APP_MQTT_DATA_PUBLISH_PERIODICITY/APP_MQTT_DATA_SAMPLING_PERIODICITY;

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
void setTopicConfig(void) {
	cJSON* baseTopicItem = cJSON_GetObjectItem(config,"baseTopic");
	if(NULL == baseTopicItem) {
		vTaskDelay(1000);
		printf("[FATAL ERROR] - setTopicConfig: 'baseTopic' not found in config.json. aborting...\r\n");
		Retcode_T retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_INVALID_CONFIG);
		Retcode_RaiseError(retcode);
		assert(0);
	}
	baseTopic = baseTopicItem->valuestring;
}


/**
 * Read the config file and parse to JSON
 */
void readConfigFromFile(void){

	vTaskDelay(5000); // otherwise we won't see the INFO
	printf("[INFO] - readConfigFromFile: starting ...\r\n");

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
		vTaskDelay(5000);
    	printf("[FATAL ERROR] - readConfigFromFile: Can not read configuration file, aborting!\r\n");
		assert(0);
    }
    config = cJSON_ParseWithOpts((const char *) FileReadBuffer, 0, 1);
	if (!config) {
		vTaskDelay(5000);
		printf("[FATAL ERROR] - readConfigFromFile : parsing config file, before: [%s]\r\n", cJSON_GetErrorPtr());
		assert(0);
	}

	printf("[INFO] - readConfigFromFile: the config.json:\r\n");
	char * jsonStr = cJSON_Print(config);
	printf("%s\r\n", jsonStr);
	free(jsonStr);

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
 * Declare resumeTasks - resumes all tasks suspended by suspendTasks
 */
Retcode_T resumeTasks(void);

/**
 * Construct and emit a response status message straight without the queue
 */
Retcode_T sendStatusResponseDirectly(cJSON * statusResponseInputJson) {
	Retcode_T retcode = RETCODE_OK;
#ifndef NDEBUG_XDK_APP
	printf("[INFO] - sendStatusResponseDirectly: starting ...\r\n");
#endif
	cJSON * responseMsg = statusResponseInputJson;

	cJSON_AddItemToObject(responseMsg, "deviceId", cJSON_CreateString(deviceId));

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
	cJSON_AddItemToObject(responseMsg, "timestamp", cJSON_CreateString(date));

	char *s;
	s = cJSON_PrintUnformatted(responseMsg);
	int32_t s_length = strlen(s);

	MqttPublishResponse.Payload = s;
	MqttPublishResponse.PayloadLength = s_length;
	//MqttPublishResponse.QoS = 1UL;

#ifndef NDEBUG_XDK_APP
	printf("[DEBUG] - sendStatusResponseDirectly: MqttPublishResponse.Payload:\r\n%s\r\n", MqttPublishResponse.Payload);
#endif

	(void)MQTT_PublishToTopic(&MqttPublishResponse, MQTT_PUBLISH_TIMEOUT_IN_MS);
	// no need to check and re-connect here, this is done in the main publishing task

	free(date);
	return retcode;
}

/**
 * Callback to handle events received via MQTT subscriptions
 * One handler for configuration and command events - the function handles both event types.
 */
static void subscriptionCallBack(MQTT_SubscribeCBParam_T param) {

	//printf("[INFO] - subscriptionCallBack: received command / configuration message. payload: \r\n%s\r\n", param.Payload);

	cJSON *inComingMsg = cJSON_Parse(param.Payload);
	if (!inComingMsg) {
		printf("Error before: [%s]\r\n", cJSON_GetErrorPtr());
		cJSON_Delete(inComingMsg);
		return;
	}

	cJSON * statusResponseInputJson = cJSON_CreateObject();
	//extract the exchangeId - mandatory for all commands & configuration messages
	cJSON * exchangeIdJsonHandle = cJSON_GetObjectItem(inComingMsg, "exchangeId");
	if(exchangeIdJsonHandle == NULL) {
		printf("'exchangeId' - element not found, mandatory. aborting ...\r\n");
		cJSON_Delete(inComingMsg);
		return;
	}
	cJSON_AddItemToObject(statusResponseInputJson, "exchangeId", cJSON_CreateString(exchangeIdJsonHandle->valuestring));

		////////////////// CONFIGURATION
	if (strstr(param.Topic, "configuration") == NULL) {
		printf("not a configuration message\n");
	} else {
		// 'tags' element
		cJSON * tagsJsonHandle = cJSON_GetObjectItem(inComingMsg, "tags");
		if(tagsJsonHandle == NULL) {
			printf("'tags' - element not found, mandatory for configuration messages. aborting ...\r\n");
			cJSON_Delete(inComingMsg);
			return;
		}
		//cJSON_AddItemToObject(statusResponseInputJson, "tags", tagsJsonHandle);
		cJSON_AddItemToObject(statusResponseInputJson, "tags", cJSON_Duplicate(tagsJsonHandle,true));
		//add requestType
		cJSON_AddItemToObject(statusResponseInputJson, "requestType", cJSON_CreateString("CONFIGURATION"));

#ifndef NDEBUG_XDK_APP
		char * jsonStr = cJSON_Print(statusResponseInputJson);
		printf("[DEBUG] - subscriptionCallBack: - statusResponseInputJson:\r\n%s\r\n", jsonStr);
		free(jsonStr);
#endif

		//read the sensors
		cJSON *sensors = cJSON_GetObjectItem(inComingMsg, "sensors");
		if (sensors == NULL || cJSON_GetArraySize(sensors) <1) {
			printf("'sensors' - element not found, mandatory for configuration messages. aborting ...\r\n");
			cJSON_Delete(inComingMsg);
			return;
		}

		// set the sampling and messaging frequency
		cJSON * numberOfEventsPerSecond = cJSON_GetObjectItem(inComingMsg,
				"telemetryEventFrequency");
		cJSON * numberOfSamplesPerEvent = cJSON_GetObjectItem(inComingMsg,
				"samplesPerEvent");
		if (numberOfEventsPerSecond == NULL || numberOfSamplesPerEvent == NULL){
			printf("telemetryEventFrequency or samplesPerEvent not supplied");
			cJSON_Delete(inComingMsg);
			return;
		}

		uint8_t newSamplesPerEvent = numberOfSamplesPerEvent->valueint;
		uint32_t newPublishFrequency = 1000 / numberOfEventsPerSecond->valueint; // convert to milliseconds
		uint32_t newSamplingFrequency = newPublishFrequency / newSamplesPerEvent;
		if (newPublishFrequency == 0 || newSamplingFrequency == 0){
			printf("invalid publishingFrequency or samplingFrequency calculated");
			cJSON_Delete(inComingMsg);
			return;
		}

		// wait until changes shall be implemented
		cJSON * delay = cJSON_GetObjectItem(inComingMsg, "delay");
		int delayTicks = SECONDS(delay->valueint);

		vTaskDelay(delayTicks);

		// now suspend all tasks and implement the changes
		printf("[INFO] - subscriptionCallBack: suspending all tasks ...\r\n");
		suspendTasks();

		// enable/disable sensor capture
		// disable all sensors
		isLight = 0;
		isAccelerator = 0;
		isGyro = 0;
		isMagneto = 0;
		isHumidity = 0;
		isTemperature = 0;
		isPressure = 0;
		int sensorCount = cJSON_GetArraySize(sensors);
		for (int i = 0; i < sensorCount; i++) {
			cJSON* sensor = cJSON_GetArrayItem(sensors, i);
			const char *actSensor = sensor->valuestring;
			printf("[INFO] - subscriptionCallBack: Activating sensor %s\r\n", actSensor);

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

		// change sampling frequency
		samplesPerEvent = newSamplesPerEvent;
		publishFrequency = newPublishFrequency;
		samplingFrequency = newSamplingFrequency;
		printf("Set publish interval to %i ms, set sample interval to %i ms\n",
				(int) publishFrequency, (int) samplingFrequency);

		cJSON_AddItemToObject(statusResponseInputJson, "status", cJSON_CreateString("SUCCESS"));

		//send the response directly without the queue...
		Retcode_T retcode = sendStatusResponseDirectly(statusResponseInputJson);
		if(RETCODE_OK != retcode) {
			printf("[ERROR] - subscriptionCallBack:sendStatusResponseDirectly(): failed.\r\n");
			Retcode_RaiseError(retcode);
		}
		cJSON_Delete(statusResponseInputJson);



		printf("[INFO] - subscriptionCallBack: resuming all tasks ...\r\n");
		resumeTasks();

	}
	////////////////// COMMAND
	if (strstr(param.Topic, "command") == NULL) {
		printf("not a command message\n");
	} else {
		cJSON * command = cJSON_GetObjectItem(inComingMsg, "command");
		if (command == NULL) {
			printf("'command' - element not found, mandatory for command messages. aborting ...\r\n");
			cJSON_Delete(inComingMsg);
			return;
		}

		cJSON_AddItemToObject(statusResponseInputJson, "requestType", cJSON_CreateString("COMMAND"));

		cJSON_AddItemToObject(statusResponseInputJson, "status", cJSON_CreateString("SUCCESS"));

		//send the response directly without the queue...
		Retcode_T retcode = sendStatusResponseDirectly(statusResponseInputJson);
		if(RETCODE_OK != retcode) {
			printf("[ERROR] - subscriptionCallBack:sendStatusResponseDirectly(): failed.\r\n");
			Retcode_RaiseError(retcode);
		}
		cJSON_Delete(statusResponseInputJson);

		char *rebootCommand = strstr(command->valuestring, "REBOOT");
		if (rebootCommand != NULL) {
			cJSON * delay = cJSON_GetObjectItem(inComingMsg, "delay");
			printf("REBOOT requested, restarting in %i seconds\n",
					delay->valueint);
			vTaskDelay(SECONDS(delay->valueint));
			//printf("[INFO] - reboot command - not actually doing it, testing ...\r\n");
			BSP_Board_SoftReset();
		}
	}
	cJSON_Delete(inComingMsg);
}


static Retcode_T connect2Broker(void) {

	Retcode_T retcode = RETCODE_OK;

	printf("[INFO] - connect2Broker: connecting to mqtt broker...\r\n");

	LED_Pattern(true, LED_PATTERN_ROLLING, MILLISECONDS(500));

	bool wlanConnected = false;
	bool connected = false;
	int i;
	for (i = 1; i <= 10; i++) {
		wlanConnected = !( (WLAN_DISCONNECTED == WlanConnect_GetStatus()) || (NETWORKCONFIG_IP_NOT_ACQUIRED == NetworkConfig_GetIpStatus()));
		if (!wlanConnected) break;
		retcode = MQTT_ConnectToBroker(&MqttConnectInfo, MQTT_CONNECT_TIMEOUT_IN_MS);
		if (RETCODE_OK != retcode) {
			printf("[WARNING] - connect2Broker : MQTT connection to the broker failed, attempt %i\n\r", i);
			Retcode_RaiseError(retcode);
		} else {
			connected = true;
			break;
		}
		vTaskDelay(MILLISECONDS(MQTT_CONNECT_TIMEOUT_IN_MS * i));
	}
	if (!connected) {
		printf("[ERROR] - connect2Broker : MQTT re-connection to the broker failed. \n\r");
		Retcode_RaiseError(retcode);
	} else
		printf("[INFO] - connect2Broker : connected (%i attempts).\n\r", i);

	LED_Pattern(false, LED_PATTERN_ROLLING, MILLISECONDS(500));

	LED_Toggle(LED_INBUILT_ORANGE);
	LED_Toggle(LED_INBUILT_YELLOW);

	return retcode;
}
static void callConnect2BrokerFromTelemetryPublishing(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	LED_Pattern(true, LED_PATTERN_ROLLING, MILLISECONDS(500));

	suspendTasks();

	// waits forever until we have full connectivity
	bool wlanConnected = false;
	bool brokerConnected = false;
	do {
		wlanConnected = !( (WLAN_DISCONNECTED == WlanConnect_GetStatus()) || (NETWORKCONFIG_IP_NOT_ACQUIRED == NetworkConfig_GetIpStatus()));
		if(!wlanConnected) {
			printf("[WARNING] - callConnect2BrokerFromTelemetryPublishing : no WLAN connectivity, waiting for 5 seconds ...\n\r");
			vTaskDelay(MILLISECONDS(5000));
		} else {
			printf("[INFO] - callConnect2BrokerFromTelemetryPublishing : WLAN connected. trying to connect to broker ...\n\r");
			Retcode_T retcode = connect2Broker();
			if(RETCODE_OK == retcode) brokerConnected = true;
			else {
				printf("[WARNING] - callConnect2BrokerFromTelemetryPublishing : MQTT re-connection to the broker failed, waiting for 5 secs ...\n\r");
				vTaskDelay(MILLISECONDS(5000));
			}
		}
	} while (!(wlanConnected && brokerConnected));

	LED_Pattern(false, LED_PATTERN_ROLLING, MILLISECONDS(500));
	LED_On(LED_INBUILT_RED);
	LED_On(LED_INBUILT_ORANGE);

	printf("[INFO] - callConnect2BrokerFromTelemetryPublishing : WLAN & broker connected.\n\r");

	resumeTasks();
}
static Retcode_T enqueueConnect2BrokerFromTelemetryPublishing(void) {
	Retcode_T retcode = RETCODE_OK;

	retcode = CmdProcessor_Enqueue(AppCmdProcessor, callConnect2BrokerFromTelemetryPublishing, NULL, UINT32_C(0));

	return retcode;
}

/**
 *
 * Publishes telemetry data
 * Serialises the samples stored in cJSON *root variable and publishes MQTT message
 */
static void publishTelemetryMessage(void * param1, uint32_t param2) {
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);

	//printf("[INFO] - publishTelemetryMessage: starting...\r\n");

    // check if we can modify root, if not, return
	if (pdFALSE == xSemaphoreTake(jsonPayloadHandle, (TickType_t ) publishFrequency) ) {
		printf("[INFO] - publishTelemetryMessage: BLOCKED by Semaphore. Not sending.\r\n");
#ifndef NDEBUG_XDK_APP
		vTaskDelay(MILLISECONDS(500));
#endif
		return;
	}
	// now we can read / modify root
    int sampleCount = cJSON_GetArraySize(root);
    if(sampleCount == 0) {
    	// nothing to publish
		printf("[INFO] - publishTelemetryMessage: sampleCount=0, nothing to publish.\r\n");
#ifndef NDEBUG_XDK_APP
		printf("[DEBUG] - publishTelemetryMessage: sampleCount=0, nothing to publish.\r\n");
#endif

#ifndef NDEBUG_XDK_APP_TASK_STATE
		eTaskState AppControllerHandleState = eTaskGetState(AppControllerHandle);
		printf("[INFO] - publishTelemetryMessage: AppControllerHandle state = %i.\r\n", AppControllerHandleState);
#endif


		xSemaphoreGive(jsonPayloadHandle);
		//vTaskDelay(MILLISECONDS(50));
    	return;
    }

    char *s = cJSON_PrintUnformatted(root);

	cJSON_Delete(root);
	root = cJSON_CreateArray();
	xSemaphoreGive(jsonPayloadHandle);

	int32_t s_length = strlen(s);

	if(s_length <= 975) {
		//max length SERVAL allows to publish
		MqttPublishInfo.Payload = s;
		MqttPublishInfo.PayloadLength = s_length;
		MqttPublishInfo.QoS = 0UL;

#ifndef NDEBUG_XDK_APP
		printf("[DEBUG] - publishTelemetryMessage: publishing:\r\n");
		printf("\ttopic:%s\r\n", MqttPublishInfo.Topic);
		printf("\tpayload:%s\r\n", MqttPublishInfo.Payload);
		//vTaskDelay(MILLISECONDS(500));
#endif

#ifdef RE_CONNECT_TESTING
		static int sequenceNumber = 0;
		sequenceNumber++;
		if( (sequenceNumber % 100) == 0) {
			/*
			 * Serval_Mqtt.h
			 *   - retcode_t Mqtt_disconnect(MqttSession_T* session);
			 * Mqtt.c:
			 *   - Mqtt_disconnect() implementation
			 *
			 * Added:
			 *  - XDK_MQTT.h
			 *     - void * getMqttSessionPtr(void);
			 *  - MQTT.c
			 *     - void * getMqttSessionPtr(void) { return &MqttSession; }
			 */
			printf("[TEST] - publishTelemetryMessage: disconnecting from broker and wait for 5 seconds ...\r\n");
			(void)Mqtt_disconnect(getMqttSessionPtr());
			vTaskDelay(MILLISECONDS(5000));
		}
#endif
		Retcode_T retcode = MQTT_PublishToTopic(&MqttPublishInfo, MQTT_PUBLISH_TIMEOUT_IN_MS);
		if(RETCODE_OK != retcode) {
			retcode = enqueueConnect2BrokerFromTelemetryPublishing();
			if(RETCODE_OK != retcode) {
				printf("[FATAL ERROR] - publishTelemetryMessage: enqueueConnect2BrokerFromTelemetryPublishing() failed.\r\n");
				Retcode_RaiseError(retcode);
				assert(0);
			}
		}

		MqttPublishInfo.QoS = 1UL;
	} else {
		printf("[WARNING] - publishTelemetryMessage: not publishing, because:\r\n");
		printf("\ttelemetry payload length = %ld\r\n", s_length);
		printf("\ts = %s\r\n", s);
		printf("\tsampleCount = %i\r\n", sampleCount);
	}
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
#ifndef NDEBUG_XDK_APP_PUBLISH
    uint32_t samplesCallsCounter = 0;
#endif

    while (1) {
#ifndef NDEBUG_XDK_APP_PUBLISH
    	samplesCallsCounter++;
#endif
	    TickType_t startTicks = xTaskGetTickCount();
#ifndef NDEBUG_XDK_APP
		printf("[DEBUG] - SampleTask: starting while loop again...\r\n");
		//vTaskDelay(MILLISECONDS(500));
#endif
#ifndef NDEBUG_XDK_APP
		printf("[DEBUG] - SampleTask: samplingFrequency=%ld\r\n", samplingFrequency);
		//vTaskDelay(MILLISECONDS(500));
#endif

		// check if we can access root first
		if (pdFALSE == xSemaphoreTake(jsonPayloadHandle, (TickType_t ) samplingFrequency)) {
			printf("[INFO] - SampleTask: BLOCKED by Semaphore. Not sampling.\r\n");
#ifndef NDEBUG_XDK_APP
			//vTaskDelay(MILLISECONDS(500));
#endif
		} else {
			// check if last sample was sent out
			// by publishTelemetryMessage()
			// - sets root to empty array if sent out
			uint8_t samplesCollected = cJSON_GetArraySize(root);
			if ( samplesCollected == samplesPerEvent ) {
				printf("[INFO] - SampleTask: last sample collection not sent out yet, not sampling new values.\r\n");
#ifndef NDEBUG_XDK_APP
				//vTaskDelay(MILLISECONDS(500));
#endif

#ifndef NDEBUG_XDK_APP_TASK_STATE
		eTaskState TelemetryHandleState = eTaskGetState(TelemetryHandle);
		printf("[INFO] - SampleTask: TelemetryHandleState state = %i.\r\n", TelemetryHandleState);
#endif

			} else {
				// now we are good to go

				retCode = Sensor_GetData(&sensorValue);
				if(RETCODE_OK != retCode) {
					printf("[ERROR] - SampleTask: Sensor_GetData() failed:\r\n");
					Retcode_RaiseError(retCode);
#ifndef NDEBUG_XDK_APP
					vTaskDelay(MILLISECONDS(1000));
#endif
				}
				// create the timestamp
				char *date;
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
				// now take the samples
				cJSON *sample;
				sample = cJSON_CreateObject();
				cJSON_AddItemToObject(sample, "timestamp", cJSON_CreateString(date));
				free(date);
				cJSON_AddItemToObject(sample, "deviceId", cJSON_CreateString(deviceId));
#ifndef NDEBUG_XDK_APP_PUBLISH
				cJSON_AddItemToObject(sample, "samplesSeq", cJSON_CreateNumber(samplesCallsCounter));
#endif

				if (isHumidity) cJSON_AddNumberToObject(sample, "humidity", (long int ) sensorValue.RH);

				if (isLight) cJSON_AddNumberToObject(sample, "light", (long int ) sensorValue.Light);

				if (isTemperature) cJSON_AddNumberToObject(sample, "temperature", (sensorValue.Temp /= 1000));

				if (isAccelerator) {
					cJSON_AddNumberToObject(sample, "acceleratorX", sensorValue.Accel.X);
					cJSON_AddNumberToObject(sample, "acceleratorY", sensorValue.Accel.Y);
					cJSON_AddNumberToObject(sample, "acceleratorZ", sensorValue.Accel.Z);
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
#ifndef NDEBUG_XDK_APP
				char * jsonStr = cJSON_Print(sample);
				printf("[DEBUG] - SampleTask: sample:\r\n%s\r\n", jsonStr);
				free(jsonStr);
#endif
				//do not delete sample, root points to it's contents
				cJSON_AddItemToArray(root, sample);
			} // good to go
			xSemaphoreGive(jsonPayloadHandle);
		} // got the semaphore

	    TickType_t endTicks = xTaskGetTickCount();
		vTaskDelay(samplingFrequency-(endTicks-startTicks));
    } // while
}

Retcode_T startTasks(void) {
	Retcode_T retcode = RETCODE_OK;
	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(SampleTask, (const char* const ) "SampleTask",
						TASK_STACK_SIZE_APP_CONTROLLER * 2, NULL,
						TASK_PRIO_SAMPLE_TASK, &AppControllerHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_OUT_OF_RESOURCES);
			return retcode;
		}
	}
	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(publishTelemetryMessageTask,
						(const char* const ) "TelemetryPublisher",
						TASK_STACK_SIZE_APP_CONTROLLER, NULL,
						TASK_PRIO_PUBLISH_TELEMETRY_TASK, &TelemetryHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_FATAL, RETCODE_OUT_OF_RESOURCES);
			return retcode;
		}
	}


	return retcode;
}

void deleteTasks(void) {
	vTaskDelete(AppControllerHandle);
	vTaskDelete(TelemetryHandle);
}


static uint32_t suspendTasksCounter = 0;
/**
 * Suspend active tasks - used to avoid race conoditions on globale variables
 */
Retcode_T suspendTasks(){
	Retcode_T retcode = RETCODE_OK;
	printf("[DEBUG] - suspendTasks: counter=%ld\r\n", ++suspendTasksCounter);

	// grab the mutex
	// otherwise we'll get an assert if the holder of a mutex doesn't exist any more
	// choosing max holding time * 1.5 - this could be done more precisely..
	if (pdFALSE == xSemaphoreTake(jsonPayloadHandle, MILLISECONDS(1500)) ) {
		printf("[FATAL ERROR] - suspendTasks: BLOCKED by Semaphore. Can't suspend tasks.\r\n");
		assert(0);
	}
	deleteTasks();
	// now clean up the shared resources and give mutex back so resume can start
	cJSON_Delete(root);
	root = cJSON_CreateArray();
	xSemaphoreGive(jsonPayloadHandle);

	return retcode;
}
/**
 * Resume all suspended tasks
 */
Retcode_T resumeTasks() {
	Retcode_T retcode = RETCODE_OK;
	retcode = startTasks();
	if(RETCODE_OK != retcode) {
		printf("[FATAL ERROR] - resumeTasks: failed to start tasks again.\r\n");
		Retcode_RaiseError(retcode);
		assert(0);
	}
#ifndef NDEBUG_XDK_APP_TASK_STATE
	// use this for debugging only

	//INCLUDE_eTaskGetState must be defined as 1 in FreeRTOSConfig.h
	eTaskState AppControllerHandleState = eTaskGetState(AppControllerHandle);
	printf("[INFO] - resumeTasks: AppControllerHandle state = %i.\r\n", AppControllerHandleState);

	eTaskState TelemetryHandleState = eTaskGetState(TelemetryHandle);
	printf("[INFO] - resumeTasks: TelemetryHandle state = %i.\r\n", TelemetryHandleState);


#ifdef RTOS_INFO
	typedef enum
	{
		eRunning = 0,	/* A task is querying the state of itself, so must be running. */
		eReady = 1,			/* The task being queried is in a read or pending ready list. */
		eBlocked = 2,		/* The task being queried is in the Blocked state. */
		eSuspended = 3,		/* The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */
		eDeleted,		/* The task being queried has been deleted, but its TCB has not yet been freed. */
		eInvalid			/* Used as an 'invalid state' value. */
	} eTaskState;

	/*
		TaskStatus_t AppControllerTaskStatus;
		// configUSE_TRACE_FACILITY must be defined as 1 in FreeRTOSConfig.h
		vTaskGetInfo(AppControllerHandle, &AppControllerTaskStatus, pdTRUE, eInvalid);
		eTaskState eCurrentState;
	*/
#endif
#endif

	return retcode;
}


/**
 * Sets up the MQTT subscriptions for commands and configuration
 * This task cancels itself after the first execution
 */
static void MQTTSubscribe(void * param1) {
	BCDS_UNUSED(param1);
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

	LED_Pattern(true, LED_PATTERN_ROLLING, MILLISECONDS(500));

	retcode = WLAN_Enable();

	// waits forever until we have WLAN connectivity
	while (  (WLAN_DISCONNECTED == WlanConnect_GetStatus()) && (NETWORKCONFIG_IP_NOT_ACQUIRED == NetworkConfig_GetIpStatus()) ){
		printf("[WARNING] - AppControllerEnable : no WLAN connectivity, waiting for 1 second ...\n\r");
		vTaskDelay(MILLISECONDS(1000));
	}
	retcode = RETCODE_OK;

	LED_Pattern(false, LED_PATTERN_ROLLING, MILLISECONDS(500));


	if (RETCODE_OK == retcode) {
		retcode = LED_On(LED_INBUILT_RED);
	}
	if (RETCODE_OK == retcode) {
		retcode = ServalPAL_Enable();
	}
	if (RETCODE_OK == retcode) {
		LED_Pattern(true, LED_PATTERN_ROLLING, MILLISECONDS(500));
		retcode = SNTP_Enable();
		if(RETCODE_OK == retcode) {
			// try multiple times until we have a time, otherwise abort
			int sntpTriesloopCounter = 0;
			bool sntpSuccess = false;
			while (!sntpSuccess && sntpTriesloopCounter++ < 1000) {
				printf("[INFO] - AppControllerEnable: retrieving time from SNTP server, tries: %d\r\n", sntpTriesloopCounter);

				retcode = SNTP_GetTimeFromServer(&sntpTime, 10000L);

				if( RETCODE_OK == retcode ) sntpSuccess = true;

				if(!sntpSuccess) {
					printf("[INFO] - AppControllerEnable: SNTP server timeout, retrying in 1 second ...\r\n");
					vTaskDelay(MILLISECONDS(1000));
				}
			}
			if(!sntpSuccess) {
				printf("[FATAL ERROR] - AppControllerEnable: SNTP time synchronization failed. aborting ...\r\n");
				assert(0);
			}
			LED_Pattern(false, LED_PATTERN_ROLLING, MILLISECONDS(500));
		}
	}
	if (RETCODE_OK == retcode) {
		retcode = MQTT_Enable();
	}

	if (RETCODE_OK == retcode) {
		retcode = Sensor_Enable();
	}

	tickOffset = xTaskGetTickCount();

	if(RETCODE_OK == retcode) {
		// waits forever until we have full connectivity
		LED_Pattern(true, LED_PATTERN_ROLLING, MILLISECONDS(500));
		bool wlanConnected = false;
		bool brokerConnected = false;
		do {
			wlanConnected = !( (WLAN_DISCONNECTED == WlanConnect_GetStatus()) || (NETWORKCONFIG_IP_NOT_ACQUIRED == NetworkConfig_GetIpStatus()));
			if(!wlanConnected) {
				printf("[WARNING] - AppControllerEnable : no WLAN connectivity, waiting for 5 seconds ...\n\r");
				vTaskDelay(MILLISECONDS(5000));
			} else {
				printf("[INFO] - AppControllerEnable : WLAN connected. trying to connect to broker ...\n\r");
				Retcode_T retcode = connect2Broker();
				if(RETCODE_OK == retcode) brokerConnected = true;
				else {
					printf("[WARNING] - AppControllerEnable : MQTT connect to broker failed, waiting for 5 secs ...\n\r");
					vTaskDelay(MILLISECONDS(5000));
				}
			}
		} while (!(wlanConnected && brokerConnected));

		LED_Pattern(false, LED_PATTERN_ROLLING, MILLISECONDS(500));

		retcode = LED_On(LED_INBUILT_ORANGE);
	}

	if (RETCODE_OK == retcode) {
		if (pdPASS
				!= xTaskCreate(MQTTSubscribe, (const char* const ) "SubscribeTask",
						TASK_STACK_SIZE_APP_CONTROLLER, NULL,
						TASK_PRIO_SUBSCRIBER_TASK, &MQTTSubscribeHandle)) {
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
		}
#ifndef NDEBUG_XDK_APP
		printf("[DEBUG] - AppControllerEnable: MQTTSubscribeHandle = %p\r\n", MQTTSubscribeHandle);
#endif
	}


	if (RETCODE_OK == retcode) {
		retcode = startTasks();
	}
	if (RETCODE_OK != retcode) {
		printf("AppControllerEnable : Failed \r\n");
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}

// now print out the deviceId

	printf("\r\nAppControllerEnable: ---------------------------------------- \r\n");
	printf("AppControllerEnable: XDK Device Id: %s\r\n", deviceId);
	printf("AppControllerEnable: ---------------------------------------- \r\n\r\n");

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
    bool storageStatus = false;
    if (RETCODE_OK == retcode)
    {
    	retcode = Storage_IsAvailable(STORAGE_MEDIUM_SD_CARD, &storageStatus);
    }

    if ((RETCODE_OK == retcode) && (storageStatus == true)){
    	readConfigFromFile();
    } else {
    	printf("[FATAL ERROR] - AppController_Init: cannot read the config file.\r\n");
    	assert(0);
    }


	// set up device id and topic
	unsigned int * serialStackAddress0 = (unsigned int*)0xFE081F0;
	unsigned int * serialStackAddress1 = (unsigned int*)0xFE081F4;
	unsigned int serialUnique0 = *serialStackAddress0;
	unsigned int serialUnique1 = *serialStackAddress1;
	size_t sz0;
	sz0 = snprintf(NULL, 0, "%08x%08x", serialUnique1, serialUnique0);
	deviceId = (char *) malloc(sz0 + 1);
	snprintf(deviceId, sz0 + 1, "%08x%08x", serialUnique1, serialUnique0);
	MqttConnectInfo.ClientId = deviceId;
	char* theTopic = formatTopic(baseTopic, "CREATE/iot-event/%s/%s/metrics");
	topic = theTopic;
	MqttPublishInfo.Topic = theTopic;

	char* tempTopic = formatTopic(baseTopic, "CREATE/iot-control/%s/device/%s/command");
	MqttDeviceCommandSubscribeInfoSingleDevice.Topic = tempTopic;

	tempTopic = formatTopic(baseTopic, "UPDATE/iot-control/%s/device/%s/configuration");
	MqttDeviceConfigurationSubscribeInfoSingleDevice.Topic = tempTopic;
	const char* iotControlSDeviceCommandTemplate =
			"CREATE/iot-control/%s/device/command";
	tempTopic =formatTopic(baseTopic,
			iotControlSDeviceCommandTemplate);
	MqttDeviceCommandSubscribeInfoRegionLocationProductionLine.Topic = tempTopic;
	const char* iotControlSDeviceConfigurationTemplate =
			"UPDATE/iot-control/%s/device/configuration";
	tempTopic =formatTopic(baseTopic,
			iotControlSDeviceConfigurationTemplate);
	MqttDeviceConfigurationSubscribeInfoRegionLocationProductionLine.Topic = tempTopic;

	char newbaseTopic[256] = "";
	strcpy(newbaseTopic, baseTopic);
	// chop last item off baseTopic
	char* lastslash;
	lastslash = strrchr(newbaseTopic, '/');
	if (lastslash !=NULL) *lastslash = '\0';

	tempTopic =formatTopic(newbaseTopic, iotControlSDeviceCommandTemplate);

	MqttDeviceCommandSubscribeInfoRegionLocation.Topic = tempTopic;

	tempTopic =formatTopic(newbaseTopic, iotControlSDeviceConfigurationTemplate);

	MqttDeviceConfigurationSubscribeInfoRegionLocation.Topic = tempTopic;

	lastslash = strrchr(newbaseTopic, '/');
	if (lastslash !=NULL) *lastslash = '\0';
	tempTopic =formatTopic(newbaseTopic, iotControlSDeviceCommandTemplate);
	MqttDeviceCommandSubscribeInfoRegion.Topic = tempTopic;
	tempTopic =formatTopic(newbaseTopic, iotControlSDeviceConfigurationTemplate);
	MqttDeviceConfigurationSubscribeInfoRegion.Topic = tempTopic;

	MqttDeviceCommandSubscribeAll.Topic = "CREATE/iot-control/device/command";

	MqttDeviceConfigurationSubscribeAll.Topic = "UPDATE/iot-control/device/configuration";

	char* statusTopic = formatTopic(baseTopic, "UPDATE/iot-control/%s/device/%s/status");
	MqttPublishResponse.Topic = statusTopic;


	root = cJSON_CreateArray();
	jsonPayloadHandle = xSemaphoreCreateMutex();

	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

/**@} */
/** ************************************************************************* */
