//---------------------------------------------------------------------------------------------
// includes
//

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <ram_pwrdn.h>

#include <drivers/include/nrfx_saadc.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_zcl_reporting.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>
#include <zb_nrf_platform.h>
#include "zb_mem_config_custom.h"
#include "zb_four_input.h"

#include "leds.h"
#include "buttons.h"


//---------------------------------------------------------------------------------------------
// defines
//

// Basic cluster attributes initial values. For more information, see section 3.2.2.2 of the ZCL specification.
#define FOUR_INPUT_INIT_BASIC_APP_VERSION       01                                  // Version of the application software (1 byte).
#define FOUR_INPUT_INIT_BASIC_STACK_VERSION     10                                  // Version of the implementation of the Zigbee stack (1 byte).
#define FOUR_INPUT_INIT_BASIC_HW_VERSION        11                                  // Version of the hardware of the device (1 byte).
#define FOUR_INPUT_INIT_BASIC_MANUF_NAME        "bikerglen.com"                     // Manufacturer name (32 bytes).
#define FOUR_INPUT_INIT_BASIC_MODEL_ID          "four-input"                        // Model number assigned by the manufacturer (32-bytes long string).
#define FOUR_INPUT_INIT_BASIC_DATE_CODE         "20240226"                          // Date provided by the manufacturer of the device in ISO 8601 format (YYYYMMDD), for the first 8 bytes. The remaining 8 bytes are manufacturer-specific.
#define FOUR_INPUT_INIT_BASIC_POWER_SOURCE      ZB_ZCL_BASIC_POWER_SOURCE_BATTERY   // Type of power source or sources available for the device. For possible values, see section 3.2.2.2.8 of the ZCL specification.
#define FOUR_INPUT_INIT_BASIC_LOCATION_DESC     "Earth"                             // Description of the physical location of the device (16 bytes). You can modify it during the commisioning process.
#define FOUR_INPUT_INIT_BASIC_PH_ENV            ZB_ZCL_BASIC_ENV_UNSPECIFIED        // Description of the type of physical environment. For possible values, see section 3.2.2.2.10 of the ZCL specification.

// source endpoint for our device
#define SOURCE_ENDPOINT            1

// destination short address and endpoint (always endpoint 1 on the coordinator)
#define DEST_SHORT_ADDR            0x0000
#define DEST_ENDPOINT              1

// Do not erase NVRAM to save the network parameters after device reboot or
// power-off. NOTE: If this option is set to ZB_TRUE then do full device erase
// for all network devices before running other samples.
#define ERASE_PERSISTENT_CONFIG    ZB_FALSE

// LEDs
#define ZIGBEE_NETWORK_STATE_LED   0      // on: disconnected, blinking: identify, off: normal operation
#define USER_LED                   1      // unused

// Buttons
#define BUTTON_0                   BIT(0) // send zcl off/on/toggle/custom commands
#define BUTTON_1                   BIT(1) // short press: identify, long press: factory reset

// no idea but required for successful compile
#define bat_num

// read and report battery voltage after an initial delay after joining the network 
// then read and report battery voltage after the specified period elapses.
#define READ_BATTERY_VOLTAGE_INITIAL_DELAY K_SECONDS(10)
#define READ_BATTERY_VOLTAGE_TIMER_PERIOD  K_HOURS(8)


//---------------------------------------------------------------------------------------------
// typedefs
//

// attribute storage for battery power config
struct zb_zcl_power_attrs {
    zb_uint8_t voltage; // Attribute 3.3.2.2.3.1
    zb_uint8_t size; // Attribute 3.3.2.2.4.2
    zb_uint8_t quantity; // Attribute 3.3.2.2.4.4
    zb_uint8_t rated_voltage; // Attribute 3.3.2.2.4.5
    zb_uint8_t alarm_mask; // Attribute 3.3.2.2.4.6
    zb_uint8_t voltage_min_threshold; // Attribute 3.3.2.2.4.7
    zb_uint8_t percent_remaining; // Attribute 3.3.2.2.3.1
    zb_uint8_t voltage_threshold_1; // Attribute 3.3.2.2.4.8
    zb_uint8_t voltage_threshold_2; // Attribute 3.3.2.2.4.8
    zb_uint8_t voltage_threshold_3; // Attribute 3.3.2.2.4.8
    zb_uint8_t percent_min_threshold; // Attribute 3.3.2.2.4.9
    zb_uint8_t percent_threshold_1; // Attribute 3.3.2.2.4.10
    zb_uint8_t percent_threshold_2; // Attribute 3.3.2.2.4.10
    zb_uint8_t percent_threshold_3; // Attribute 3.3.2.2.4.10
    zb_uint32_t alarm_state; // Attribute 3.3.2.2.4.11
};

typedef struct zb_zcl_power_attrs zb_zcl_power_attrs_t;

// attribute storage for our device
struct zb_device_ctx {
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_identify_attrs_t identify_attr;
	zb_zcl_power_attrs_t power_attr;
};

// storage for the destination short address and endpoint number
struct dest_context {
	zb_uint8_t endpoint;
	zb_uint16_t short_addr;
};

// coin cell voltage-capacity pairs
typedef struct {
  uint16_t      voltage;
  uint8_t       capacity;
} voltage_capacity_pair_t;


//---------------------------------------------------------------------------------------------
// Prototypes
//

// used to display reporting info structures if needed
zb_zcl_reporting_info_t *zb_zcl_get_reporting_info(zb_uint8_t slot_number);

void zboss_signal_handler (zb_bufid_t bufid);
static void configure_gpio (void);
static void button_handler (uint32_t button_state, uint32_t has_changed);
static void light_switch_send_on_off (zb_bufid_t bufid, zb_uint16_t cmd_id);
static void start_identifying (zb_bufid_t bufid);
static void identify_cb (zb_bufid_t bufid);
static void toggle_identify_led (zb_bufid_t bufid);
static void app_clusters_attr_init (void);
static void configure_attribute_reporting (void);
static void read_battery_voltage_cb (struct k_timer *timer);
static void read_battery_voltage_work_handler(struct k_work *work);
static uint8_t cr2032_CalculateLevel (uint16_t voltage);
static void send_attribute_report (zb_bufid_t bufid, zb_uint16_t cmd_id);

#if DT_NODE_EXISTS(DT_NODELABEL(sw_spi_cs_n))
static void power_down_spi_flash (void);
#endif


//---------------------------------------------------------------------------------------------
// Globals
//

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE to compile light switch (End Device) source code.
#endif

LOG_MODULE_REGISTER (app, LOG_LEVEL_INF);

// attribute storage for our device
static struct zb_device_ctx dev_ctx;

// storage for the destination short address and endpoint number
static struct dest_context dest_ctx;

// Declare attribute list for Basic cluster (server).
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT
(
	basic_server_attr_list,
	&dev_ctx.basic_attr.zcl_version,
	&dev_ctx.basic_attr.app_version,
	&dev_ctx.basic_attr.stack_version,
	&dev_ctx.basic_attr.hw_version,
	dev_ctx.basic_attr.mf_name,
	dev_ctx.basic_attr.model_id,
	dev_ctx.basic_attr.date_code,
	&dev_ctx.basic_attr.power_source,
	dev_ctx.basic_attr.location_id,
	&dev_ctx.basic_attr.ph_env,
	dev_ctx.basic_attr.sw_ver
);

// Declare attribute list for Identify cluster (client).
ZB_ZCL_DECLARE_IDENTIFY_CLIENT_ATTRIB_LIST
(
	identify_client_attr_list
);

// Declare attribute list for Identify cluster (server).
ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST
(
	identify_server_attr_list,
	&dev_ctx.identify_attr.identify_time
);

// Declare attribute list for On/Off cluster (client).
ZB_ZCL_DECLARE_ON_OFF_CLIENT_ATTRIB_LIST
(
	on_off_client_attr_list
);

// Declare attribute list for power configuration cluster (server).
ZB_ZCL_DECLARE_POWER_CONFIG_BATTERY_ATTRIB_LIST_EXT(
	power_config_server_attr_list,
	&dev_ctx.power_attr.voltage,
	&dev_ctx.power_attr.size,
	&dev_ctx.power_attr.quantity,
	&dev_ctx.power_attr.rated_voltage,
	&dev_ctx.power_attr.alarm_mask,
	&dev_ctx.power_attr.voltage_min_threshold,
	&dev_ctx.power_attr.percent_remaining,
	&dev_ctx.power_attr.voltage_threshold_1,
	&dev_ctx.power_attr.voltage_threshold_2,
	&dev_ctx.power_attr.voltage_threshold_3,
	&dev_ctx.power_attr.percent_min_threshold,
	&dev_ctx.power_attr.percent_threshold_1,
	&dev_ctx.power_attr.percent_threshold_2,
	&dev_ctx.power_attr.percent_threshold_3,
	&dev_ctx.power_attr.alarm_state
);

// Declare cluster list for four input device.
ZB_DECLARE_FOUR_INPUT_CLUSTER_LIST
(
	four_input_clusters,
	basic_server_attr_list,
	identify_client_attr_list,
	identify_server_attr_list,
	on_off_client_attr_list,
	power_config_server_attr_list
);

// Declare endpoint for four input device.
ZB_DECLARE_FOUR_INPUT_EP
(
	four_input_ep,
	SOURCE_ENDPOINT,
	four_input_clusters
);

// Declare application's device context (list of registered endpoints) for four input device.
ZBOSS_DECLARE_DEVICE_CTX_1_EP
(
	four_input_ctx, 
	four_input_ep
);

// alarm for taking an ADC reading every 6 hours
struct k_timer read_battery_voltage_timer;
K_WORK_DEFINE (read_battery_voltage_work, read_battery_voltage_work_handler);

// Voltage - Capacity pair table from thunderboard react
// Algorithm assumes the values are arranged in a descending order.
// The values in the table are an average of those found in the CR2032 datasheets from
// Energizer, Maxell and Panasonic. Table modified for zigbee half percent steps.
static voltage_capacity_pair_t vcPairs[] =
{ { 3000, 200 }, { 2900, 160 }, { 2800, 120 }, { 2700, 80 }, { 2600, 60 },
  { 2500, 40 }, { 2400, 20 }, { 2000, 0 } };


//---------------------------------------------------------------------------------------------
// main
//

int main (void)
{
	LOG_INF ("Starting Four Input Device");

	// initialize
	configure_gpio ();
	register_factory_reset_button (BUTTON_1);
	zigbee_erase_persistent_storage (ERASE_PERSISTENT_CONFIG);
	zb_set_ed_timeout (ED_AGING_TIMEOUT_64MIN);
    zb_set_keepalive_timeout (ZB_MILLISECONDS_TO_BEACON_INTERVAL(3600*1000));
    
#if DT_NODE_EXISTS (DT_NODELABEL (sw_spi_cs_n))
	// place spi flash in power down mode
	power_down_spi_flash ();
#endif

	// send things to endpoint 1 on the coordinator
	dest_ctx.short_addr = DEST_SHORT_ADDR;
	dest_ctx.endpoint = DEST_ENDPOINT;

	// configure for lowest power
	zigbee_configure_sleepy_behavior (true);
	power_down_unused_ram ();

	// register switch device context (endpoints)
	ZB_AF_REGISTER_DEVICE_CTX (&four_input_ctx);

	// initialize application clusters
	app_clusters_attr_init ();

	// register handlers to identify notifications
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(SOURCE_ENDPOINT, identify_cb);

	// initialize read battery voltage timer
	k_timer_init (&read_battery_voltage_timer, read_battery_voltage_cb, NULL);

	// start Zigbee default thread
	zigbee_enable ();

	LOG_INF ("Four Input Device started");

	// suspend main thread
	while (1) {
		k_sleep (K_FOREVER);
	}
}


//---------------------------------------------------------------------------------------------
// zigbee stack event handler
//

void zboss_signal_handler(zb_bufid_t bufid)
{
	static bool lastJoin = false;

	zb_zdo_app_signal_hdr_t *sig_hndler = NULL;
	zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &sig_hndler);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	// Update network status LED.
	// zigbee_led_status_update(bufid, ZIGBEE_NETWORK_STATE_LED);

	// Call default signal handler until there's a reason to check for different signals
	// the default handler evens calls zb_sleep_now for us
	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));

	// free buffer if it's allocated
	if (bufid) {
		zb_buf_free(bufid);
	}

	// once joined, set the poll and battery voltage intervals to an hour.
	// if using a sparkfun board with a spi flash chip, drop the flash chip 
	// into power down mode again just in case missed it the first time.
	bool thisJoin = ZB_JOINED();
	if ((lastJoin == false) && (thisJoin == true)) {
		LOG_INF ("joined network!");
		led_set_off (ZIGBEE_NETWORK_STATE_LED);
		zb_zdo_pim_set_long_poll_interval (3600*1000);
		configure_attribute_reporting ();
		status = zb_zcl_start_attr_reporting(SOURCE_ENDPOINT, ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID);
		k_timer_start(&read_battery_voltage_timer, READ_BATTERY_VOLTAGE_INITIAL_DELAY, READ_BATTERY_VOLTAGE_TIMER_PERIOD);
#if DT_NODE_EXISTS(DT_NODELABEL(sw_spi_cs_n))
		power_down_spi_flash ();
#endif
	} else if ((lastJoin == true) && (thisJoin == false)) {
		LOG_INF ("left network!");
		// no longer joined, turn on network state led and stop reading battery voltage
		led_set_on (ZIGBEE_NETWORK_STATE_LED);
		k_timer_stop(&read_battery_voltage_timer);
	}
	lastJoin = thisJoin;
}


//---------------------------------------------------------------------------------------------
// configure attribute reporting
//

#define RPT_MIN 0x0001
#define RPT_MAX 0xFFFE

static void configure_attribute_reporting (void)
{
	// If the maximum reporting interval is set to 0xffff then the device shall not issue any 
	// reports for the attribute. If it is set to 0x0000 and minimum reporting interval is set 
	// to something other than 0xffff then the device shall not do periodic reporting.
	// It can still send reports on value change in the last case, but not periodic.

	zb_zcl_reporting_info_t batt_rep_info;
	zb_ret_t status;

	memset(&batt_rep_info, 0, sizeof(batt_rep_info));
	batt_rep_info.direction = ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT;
	batt_rep_info.ep = SOURCE_ENDPOINT;
	batt_rep_info.cluster_id = ZB_ZCL_CLUSTER_ID_POWER_CONFIG;
	batt_rep_info.cluster_role = ZB_ZCL_CLUSTER_SERVER_ROLE;
	batt_rep_info.attr_id = ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID;
	batt_rep_info.dst.short_addr = 0x0000;
	batt_rep_info.dst.endpoint = 1;
	batt_rep_info.dst.profile_id = ZB_AF_HA_PROFILE_ID;
	batt_rep_info.u.send_info.min_interval = RPT_MIN;
	batt_rep_info.u.send_info.max_interval = 0;
	batt_rep_info.u.send_info.delta.u8 = 0x00;
	batt_rep_info.u.send_info.reported_value.u8 = 0;
	batt_rep_info.u.send_info.def_min_interval = RPT_MIN;
	batt_rep_info.u.send_info.def_max_interval = RPT_MAX;
	status = zb_zcl_put_reporting_info(&batt_rep_info, ZB_TRUE);       

	memset(&batt_rep_info, 0, sizeof(batt_rep_info));
	batt_rep_info.direction = ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT;
	batt_rep_info.ep = SOURCE_ENDPOINT;
	batt_rep_info.cluster_id = ZB_ZCL_CLUSTER_ID_POWER_CONFIG;
	batt_rep_info.cluster_role = ZB_ZCL_CLUSTER_SERVER_ROLE;
	batt_rep_info.attr_id = ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID;
	batt_rep_info.dst.short_addr = 0x0000;
	batt_rep_info.dst.endpoint = 1;
	batt_rep_info.dst.profile_id = ZB_AF_HA_PROFILE_ID;
	batt_rep_info.u.send_info.min_interval = RPT_MIN;
	batt_rep_info.u.send_info.max_interval = RPT_MAX;
	batt_rep_info.u.send_info.delta.u8 = 0x00;
	batt_rep_info.u.send_info.reported_value.u8 = 0;
	batt_rep_info.u.send_info.def_min_interval = RPT_MIN;
	batt_rep_info.u.send_info.def_max_interval = RPT_MAX;
	status = zb_zcl_put_reporting_info(&batt_rep_info, ZB_TRUE);       

	memset(&batt_rep_info, 0, sizeof(batt_rep_info));
	batt_rep_info.direction = ZB_ZCL_CONFIGURE_REPORTING_SEND_REPORT;
	batt_rep_info.ep = SOURCE_ENDPOINT;
	batt_rep_info.cluster_id = ZB_ZCL_CLUSTER_ID_POWER_CONFIG;
	batt_rep_info.cluster_role = ZB_ZCL_CLUSTER_SERVER_ROLE;
	batt_rep_info.attr_id = ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_ALARM_STATE_ID;
	batt_rep_info.dst.short_addr = 0x0000;
	batt_rep_info.dst.endpoint = 1;
	batt_rep_info.dst.profile_id = ZB_AF_HA_PROFILE_ID;
	batt_rep_info.u.send_info.min_interval = RPT_MIN;
	batt_rep_info.u.send_info.max_interval = RPT_MAX;
	batt_rep_info.u.send_info.delta.u8 = 0x00;
	batt_rep_info.u.send_info.reported_value.u32 = 0;
	batt_rep_info.u.send_info.def_min_interval = RPT_MIN;
	batt_rep_info.u.send_info.def_max_interval = RPT_MAX;
	status = zb_zcl_put_reporting_info(&batt_rep_info, ZB_TRUE);       
}


//---------------------------------------------------------------------------------------------
// configure LEDs and buttons
//

static void configure_gpio (void)
{
	int err;

	led_init ();

#if DT_NODE_EXISTS(DT_NODELABEL(led0))
	// turn led on until network is joined
	led_set_on (ZIGBEE_NETWORK_STATE_LED); 
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(led1))
	// turn unused user LED off
	led_set_off (USER_LED); 
#endif

	buttons_init (button_handler);
}


//---------------------------------------------------------------------------------------------
// set default values of attributes in the application's clusters
//

static void app_clusters_attr_init (void)
{
	// Basic cluster attributes data.
	dev_ctx.basic_attr.zcl_version   = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.power_source  = ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN;
    dev_ctx.basic_attr.power_source  = FOUR_INPUT_INIT_BASIC_POWER_SOURCE;
    dev_ctx.basic_attr.stack_version = FOUR_INPUT_INIT_BASIC_STACK_VERSION;
    dev_ctx.basic_attr.hw_version    = FOUR_INPUT_INIT_BASIC_HW_VERSION;

    // Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte should
    // contain string length without trailing zero.
    //
    // For example "test" string wil be encoded as:
    //   [(0x4), 't', 'e', 's', 't']

    ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.mf_name,
                          FOUR_INPUT_INIT_BASIC_MANUF_NAME,
                          ZB_ZCL_STRING_CONST_SIZE(FOUR_INPUT_INIT_BASIC_MANUF_NAME));

    ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.model_id,
                          FOUR_INPUT_INIT_BASIC_MODEL_ID,
                          ZB_ZCL_STRING_CONST_SIZE(FOUR_INPUT_INIT_BASIC_MODEL_ID));

    ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.date_code,
                          FOUR_INPUT_INIT_BASIC_DATE_CODE,
                          ZB_ZCL_STRING_CONST_SIZE(FOUR_INPUT_INIT_BASIC_DATE_CODE));


    ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.location_id,
                          FOUR_INPUT_INIT_BASIC_LOCATION_DESC,
                          ZB_ZCL_STRING_CONST_SIZE(FOUR_INPUT_INIT_BASIC_LOCATION_DESC));


    dev_ctx.basic_attr.ph_env = FOUR_INPUT_INIT_BASIC_PH_ENV;

	// Identify cluster attributes data.
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	// Power config attributes data.
	dev_ctx.power_attr.voltage               = ZB_ZCL_POWER_CONFIG_BATTERY_VOLTAGE_INVALID;
	dev_ctx.power_attr.size                  = ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_OTHER;
	dev_ctx.power_attr.quantity              = 1;
	dev_ctx.power_attr.rated_voltage         = 30;
	dev_ctx.power_attr.alarm_mask            = 0x00;
	dev_ctx.power_attr.voltage_min_threshold = 20;
	dev_ctx.power_attr.percent_remaining     = ZB_ZCL_POWER_CONFIG_BATTERY_REMAINING_UNKNOWN;
	dev_ctx.power_attr.voltage_threshold_1   = 22;
	dev_ctx.power_attr.voltage_threshold_2   = 24;
	dev_ctx.power_attr.voltage_threshold_3   = 26;
	dev_ctx.power_attr.percent_min_threshold = 2*10;
	dev_ctx.power_attr.percent_threshold_1   = 2*15;
	dev_ctx.power_attr.percent_threshold_2   = 2*20;
	dev_ctx.power_attr.percent_threshold_3   = 2*25;
	dev_ctx.power_attr.alarm_state           = 0x00000000;
}


//---------------------------------------------------------------------------------------------
// button event handler
//

static void button_handler (uint32_t button_state, uint32_t has_changed)
{
	zb_uint16_t cmd_id = 0xFFFF;
	zb_ret_t zb_err_code;

	LOG_INF ("button_handler");

	// inform default signal handler about user input at the device
	user_input_indicate ();

    // check for start of factory reset
	check_factory_reset_button (button_state, has_changed);

	// check for button presses and releases
	if (BUTTON_0 & has_changed & button_state) {			// magnet away from sensor => high => on
		cmd_id = ZB_ZCL_CMD_ON_OFF_ON_ID;
	} else if (BUTTON_0 & has_changed & ~button_state) {	// magnet near sensor => low => off
		cmd_id = ZB_ZCL_CMD_ON_OFF_OFF_ID;
	} else if (BUTTON_1 & has_changed & ~button_state) {	// button 1 released
		// button 1 released: if not factory reset, enter identify mode
		if (!was_factory_reset_done ()) {
			// Button released before Factory Reset, Start identification mode
			ZB_SCHEDULE_APP_CALLBACK (start_identifying, 0);
		}
	}

	// if needed, send a command
	if (cmd_id != 0xFFFF) {
		zb_err_code = zb_buf_get_out_delayed_ext (light_switch_send_on_off, cmd_id, 0);
		ZB_ERROR_CHECK (zb_err_code);
	}

	// led_set_on (USER_LED); 
	// led_set_off (USER_LED); 
}


//---------------------------------------------------------------------------------------------
// send light switch on off command
//
// bufid    Non-zero reference to Zigbee stack buffer that will be used to construct on/off request.
// cmd_id   ZCL command id.
//

static void light_switch_send_on_off (zb_bufid_t bufid, zb_uint16_t cmd_id)
{
	LOG_INF("Send ON/OFF command: %d", cmd_id);

	ZB_ZCL_ON_OFF_SEND_REQ(bufid,
			       dest_ctx.short_addr,
			       ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
			       dest_ctx.endpoint,
			       SOURCE_ENDPOINT,
			       ZB_AF_HA_PROFILE_ID,
			       ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
			       cmd_id,
			       NULL);
}


//---------------------------------------------------------------------------------------------
// start identifying
//

static void start_identifying (zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	if (ZB_JOINED()) {
		// Check if endpoint is in identifying mode,
		// if not, put desired endpoint in identifying mode.
		if (dev_ctx.identify_attr.identify_time ==
		    ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE) {

			zb_ret_t zb_err_code = zb_bdb_finding_binding_target(SOURCE_ENDPOINT);

			if (zb_err_code == RET_OK) {
				LOG_INF("Enter identify mode");
			} else if (zb_err_code == RET_INVALID_STATE) {
				LOG_WRN("RET_INVALID_STATE - Cannot enter identify mode");
			} else {
				ZB_ERROR_CHECK(zb_err_code);
			}
		} else {
			LOG_INF("Cancel identify mode");
			zb_bdb_finding_binding_target_cancel();
		}
	} else {
		LOG_WRN("Device not in a network - cannot enter identify mode");
	}
}


//---------------------------------------------------------------------------------------------
// identify callback
// 

static void identify_cb (zb_bufid_t bufid)
{
	zb_ret_t zb_err_code;

	if (bufid) {
		/* Schedule a self-scheduling function that will toggle the LED. */
		ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
	} else {
		/* Cancel the toggling function alarm and turn off LED. */
		zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
		ZVUNUSED(zb_err_code);

		/* Update network status/idenitfication LED. */
		if (ZB_JOINED()) {
			led_set_off (ZIGBEE_NETWORK_STATE_LED);
		} else {
			led_set_on (ZIGBEE_NETWORK_STATE_LED);
		}
	}
}


//---------------------------------------------------------------------------------------------
// toggle identify led
//

static void toggle_identify_led (zb_bufid_t bufid)
{
	static int blink_status;

	led_set (ZIGBEE_NETWORK_STATE_LED, (++blink_status) % 2);
	ZB_SCHEDULE_APP_ALARM(toggle_identify_led, bufid, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}


//---------------------------------------------------------------------------------------------
// bit-banged spi to place spi flash on some sparkfun boards into the power down state
//

#if DT_NODE_EXISTS(DT_NODELABEL(sw_spi_cs_n))

static void power_down_spi_flash (void)
{
	const struct gpio_dt_spec spi_cs_n = GPIO_DT_SPEC_GET(DT_NODELABEL(sw_spi_cs_n),gpios);
	const struct gpio_dt_spec spi_sck = GPIO_DT_SPEC_GET(DT_NODELABEL(sw_spi_sck),gpios);
	const struct gpio_dt_spec spi_mosi = GPIO_DT_SPEC_GET(DT_NODELABEL(sw_spi_mosi),gpios);
	const struct gpio_dt_spec spi_wr_n = GPIO_DT_SPEC_GET(DT_NODELABEL(sw_spi_wr_n),gpios);
	const struct gpio_dt_spec spi_hold_n = GPIO_DT_SPEC_GET(DT_NODELABEL(sw_spi_hold_n),gpios);

	gpio_pin_configure_dt (&spi_cs_n, GPIO_OUTPUT | GPIO_ACTIVE_HIGH | GPIO_PUSH_PULL);
	gpio_pin_configure_dt (&spi_sck, GPIO_OUTPUT | GPIO_ACTIVE_HIGH | GPIO_PUSH_PULL);
	gpio_pin_configure_dt (&spi_mosi, GPIO_OUTPUT | GPIO_ACTIVE_HIGH | GPIO_PUSH_PULL);
	gpio_pin_configure_dt (&spi_wr_n, GPIO_OUTPUT | GPIO_ACTIVE_HIGH | GPIO_PUSH_PULL);
	gpio_pin_configure_dt (&spi_hold_n, GPIO_OUTPUT | GPIO_ACTIVE_HIGH | GPIO_PUSH_PULL);
	gpio_pin_set_dt (&spi_cs_n, 1);
	gpio_pin_set_dt (&spi_sck, 0);
	gpio_pin_set_dt (&spi_mosi, 0);
	gpio_pin_set_dt (&spi_wr_n, 1);
	gpio_pin_set_dt (&spi_hold_n, 1);	

	k_sleep(K_MSEC(1));

	gpio_pin_set_dt (&spi_cs_n, 0);
	k_sleep(K_USEC(1));

	uint8_t data = 0xb9;
	for (int i = 0; i < 8; i++) {
		gpio_pin_set_dt (&spi_mosi, (data & 0x80) ? 1 : 0);
		data <<= 1;
		k_sleep(K_USEC(1));
		gpio_pin_set_dt (&spi_sck, 1);
		k_sleep(K_USEC(1));
		gpio_pin_set_dt (&spi_sck, 0);
		k_sleep(K_USEC(1));
	}

	k_sleep(K_USEC(1));
	gpio_pin_set_dt (&spi_cs_n, 1);
}

#endif


//---------------------------------------------------------------------------------------------
// use the adc to periodically read the battery voltage on vdd pin and update the 
// battery voltage attribute. if joined to a network, send the attribute report.
// using the old nrfx saadc driver because it permits the adc to be shutdown between
// samples. the zypher saadc driver does not have this capability.
//

#define NRFX_SAADC_CONFIG_IRQ_PRIORITY 6

static void read_battery_voltage_cb (struct k_timer *timer)
{
	// we're in an interrupt but need to complete the work outside of an interrupt
    k_work_submit (&read_battery_voltage_work);
}


static void read_battery_voltage_work_handler(struct k_work *work)
{
	static int report_count = 0;
	nrfx_err_t status;
	nrfx_saadc_channel_t channel;
	nrf_saadc_value_t sample;
	zb_zcl_reporting_info_t *info;

	LOG_INF ("===== read_battery_voltage_work_handler (%d) =====", report_count++);
	// LOG_INF ("zb_osif_is_inside_isr: %d", zb_osif_is_inside_isr());

	// initialize adc
	status = nrfx_saadc_init (NRFX_SAADC_CONFIG_IRQ_PRIORITY);

	channel.channel_config.resistor_p = NRF_SAADC_RESISTOR_DISABLED;
	channel.channel_config.resistor_n = NRF_SAADC_RESISTOR_DISABLED;
	channel.channel_config.gain       = NRF_SAADC_GAIN1_6;
	channel.channel_config.reference  = NRF_SAADC_REFERENCE_INTERNAL;
	channel.channel_config.acq_time   = NRFX_SAADC_DEFAULT_ACQTIME;
	channel.channel_config.mode       = NRF_SAADC_MODE_SINGLE_ENDED;
	channel.channel_config.burst      = NRF_SAADC_BURST_DISABLED;
	channel.pin_p                     = NRF_SAADC_INPUT_VDD;
	channel.pin_n                     = NRF_SAADC_INPUT_DISABLED;
	channel.channel_index             = 0;

    status = nrfx_saadc_channel_config (&channel);

	status = nrfx_saadc_simple_mode_set ((1<<0),
                                         NRF_SAADC_RESOLUTION_14BIT,
                                         NRF_SAADC_OVERSAMPLE_8X,
                                         NULL);
        
    status = nrfx_saadc_buffer_set (&sample, 1);

	// read sample
    status = nrfx_saadc_mode_trigger ();

	// shutdown adc to save power
	nrfx_saadc_uninit ();

	// convert to millivolts
	int32_t resolution = 14;
	int32_t gainrecip = 6;
	int32_t ref_mv = 600;
	int32_t adc_mv = (sample * ref_mv * gainrecip) >> resolution;

	// convert to 100s of millivolts
	zb_uint8_t battery_voltage = (adc_mv + 50) / 100;

	// convert to percentage remaining
	zb_uint8_t battery_level = cr2032_CalculateLevel (adc_mv);

	LOG_INF ("adc: %04x / %d mV / %d / %d%%", sample, adc_mv, battery_voltage, battery_level / 2);

	// update battery voltage attribute value
    zb_zcl_set_attr_val (SOURCE_ENDPOINT,
                         ZB_ZCL_CLUSTER_ID_POWER_CONFIG, 
                         ZB_ZCL_CLUSTER_SERVER_ROLE, 
                         ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
                         &battery_voltage, 
                         ZB_FALSE);

	// mark battery voltage attribute for reporting
	// technically, this attribute is not reportable so we force it here
	zb_zcl_mark_attr_for_reporting (SOURCE_ENDPOINT,
	                                ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
									ZB_ZCL_CLUSTER_SERVER_ROLE,
									ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID);

	// update percentage remaining attribute value
	// this attribute is reportable so set attr will call mark_attr for us.
    zb_zcl_set_attr_val (SOURCE_ENDPOINT,
                         ZB_ZCL_CLUSTER_ID_POWER_CONFIG, 
                         ZB_ZCL_CLUSTER_SERVER_ROLE, 
                         ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
                         &battery_level, 
                         ZB_FALSE);

	zb_buf_get_out_delayed_ext (send_attribute_report, 0, 0);

	if (report_count < 10) {
		for (int i = 0; i < ZB_FOUR_INPUT_REPORT_ATTR_COUNT; i++) {
			info = zb_zcl_get_reporting_info(i);
			if (info != NULL) {
				LOG_INF ("dir: %d ep: %d cluster: %d role: %d attr: %d flags: 0x%x run time: %d dst: %04x, dep: %d, prof: %d, mini: %d, maxi: %d", 
					info->direction,
					info->ep,
					info->cluster_id,
					info->cluster_role,
					info->attr_id,
					info->flags,
					info->run_time,
					info->dst.short_addr,
					info->dst.endpoint,
					info->dst.profile_id,
					info->u.send_info.min_interval,
					info->u.send_info.max_interval
				);
			}
		}
	}	
}


static void send_attribute_report (zb_bufid_t bufid, zb_uint16_t cmd_id)
{
	LOG_INF("force zboss scheduler to wake and send attribute report");
    zb_buf_free (bufid);
}


// from thunderboard react
uint8_t cr2032_CalculateLevel (uint16_t voltage)
{
  uint32_t res = 0;
  uint8_t i;

  // Iterate through voltage/capacity table until correct interval is found.
  // Then interpolate capacity within that interval based on a linear approximation
  // between the capacity at the low and high end of the interval.
  for (i = 0; i < (sizeof(vcPairs) / sizeof(voltage_capacity_pair_t)); i++) {
    if (voltage > vcPairs[i].voltage) {
      if (i == 0) {
        // Higher than maximum voltage in table.
        return vcPairs[0].capacity;
      } else {
        // Calculate the capacity by interpolation.
        res = (voltage - vcPairs[i].voltage)
              * (vcPairs[i - 1].capacity - vcPairs[i].capacity)
              / (vcPairs[i - 1].voltage - vcPairs[i].voltage);
        res += vcPairs[i].capacity;
        return (uint8_t)res;
      }
    }
  }
  // Below the minimum voltage in the table.
  return vcPairs[sizeof(vcPairs) / sizeof(voltage_capacity_pair_t) - 1].capacity;
}
