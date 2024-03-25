#ifndef __ZB_FOUR_INPUT_H__
#define __ZB_FOUR_INPUT_H__

// TODO Dimmer Switch Device ID, Considering changing to ON/OFF Switch, 0x0000
#define ZB_DIMMER_SWITCH_DEVICE_ID 0x0104

// TODO Dimmer Switch device version
#define ZB_DEVICE_VER_DIMMER_SWITCH 0

// Four input device numer of IN (server) clusters
#define ZB_FOUR_INPUT_IN_CLUSTER_NUM 3

// Four input device number of OUT (client) clusters
#define ZB_FOUR_INPUT_OUT_CLUSTER_NUM 2

// Four input device total number of (IN+OUT) clusters
#define ZB_FOUR_INPUT_CLUSTER_NUM \
	(ZB_FOUR_INPUT_IN_CLUSTER_NUM + ZB_FOUR_INPUT_OUT_CLUSTER_NUM)

// Number of attributes for reporting on four input device
// battery percentage remaining, battery alarm + battery voltage
#define ZB_FOUR_INPUT_REPORT_ATTR_COUNT (ZB_ZCL_POWER_CONFIG_REPORT_ATTR_COUNT + 1)


// Declare cluster list for four input device
//
// cluster_list_name - cluster list variable name
// basic_server_attr_list - attribute list for Basic cluster (server role)
// identify_server_attr_list - attribute list for Identify cluster (server role)
// identify_client_attr_list - attribute list for Identify cluster (client role)
// on_off_client_attr_list - attribute list for On/Off cluster (client role)
// power_config_server_attr_list - attribute list for Power COnfig cluster (server role)

#define ZB_DECLARE_FOUR_INPUT_CLUSTER_LIST(			  \
		cluster_list_name,						      \
		basic_server_attr_list,						  \
		identify_client_attr_list,					  \
		identify_server_attr_list,					  \
		on_off_client_attr_list,                      \
		power_config_server_attr_list)		     	  \
zb_zcl_cluster_desc_t cluster_list_name[] =			  \
{										  			  \
	ZB_ZCL_CLUSTER_DESC(							  \
		ZB_ZCL_CLUSTER_ID_BASIC,					  \
		ZB_ZCL_ARRAY_SIZE(basic_server_attr_list, zb_zcl_attr_t), \
		(basic_server_attr_list),					  \
		ZB_ZCL_CLUSTER_SERVER_ROLE,					  \
		ZB_ZCL_MANUF_CODE_INVALID					  \
	),									              \
	ZB_ZCL_CLUSTER_DESC(							  \
		ZB_ZCL_CLUSTER_ID_IDENTIFY,					  \
		ZB_ZCL_ARRAY_SIZE(identify_server_attr_list, zb_zcl_attr_t), \
		(identify_server_attr_list),				  \
		ZB_ZCL_CLUSTER_SERVER_ROLE,					  \
		ZB_ZCL_MANUF_CODE_INVALID					  \
	),									              \
	ZB_ZCL_CLUSTER_DESC(							  \
		ZB_ZCL_CLUSTER_ID_POWER_CONFIG,				  \
		ZB_ZCL_ARRAY_SIZE(power_config_server_attr_list, zb_zcl_attr_t), \
		(power_config_server_attr_list),			  \
		ZB_ZCL_CLUSTER_SERVER_ROLE,					  \
		ZB_ZCL_MANUF_CODE_INVALID					  \
	),									              \
	ZB_ZCL_CLUSTER_DESC(							  \
		ZB_ZCL_CLUSTER_ID_IDENTIFY,					  \
		ZB_ZCL_ARRAY_SIZE(identify_client_attr_list, zb_zcl_attr_t), \
		(identify_client_attr_list),				  \
		ZB_ZCL_CLUSTER_CLIENT_ROLE,					  \
		ZB_ZCL_MANUF_CODE_INVALID					  \
	),									              \
	ZB_ZCL_CLUSTER_DESC(							  \
		ZB_ZCL_CLUSTER_ID_ON_OFF,					  \
		ZB_ZCL_ARRAY_SIZE(on_off_client_attr_list, zb_zcl_attr_t), \
		(on_off_client_attr_list),					  \
		ZB_ZCL_CLUSTER_CLIENT_ROLE,					  \
		ZB_ZCL_MANUF_CODE_INVALID					  \
	)									              \
}


// Declare simple descriptor for four input device
//
// ep_name - endpoint variable name
// ep_id - endpoint ID
// in_clust_num - number of supported input clusters
// out_clust_num - number of supported output clusters

#define ZB_ZCL_DECLARE_FOUR_INPUT_SIMPLE_DESC(			 \
	ep_name, ep_id, in_clust_num, out_clust_num)		 \
	ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num); \
	ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num) simple_desc_##ep_name = \
	{									            \
		ep_id,								        \
		ZB_AF_HA_PROFILE_ID,						\
		ZB_DIMMER_SWITCH_DEVICE_ID,					\
		ZB_DEVICE_VER_DIMMER_SWITCH,				\
		0,								            \
		in_clust_num,							    \
		out_clust_num,							    \
		{								            \
			ZB_ZCL_CLUSTER_ID_BASIC,				\
			ZB_ZCL_CLUSTER_ID_IDENTIFY,				\
			ZB_ZCL_CLUSTER_ID_POWER_CONFIG,         \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,				\
			ZB_ZCL_CLUSTER_ID_ON_OFF,				\
		}								            \
	}


// Declare endpoint for four input device
//
// ep_name - endpoint variable name
// ep_id - endpoint ID
// cluster_list - endpoint cluster list

#define ZB_DECLARE_FOUR_INPUT_EP(ep_name, ep_id, cluster_list)		          \
	ZB_ZCL_DECLARE_FOUR_INPUT_SIMPLE_DESC(ep_name, ep_id,		              \
		  ZB_FOUR_INPUT_IN_CLUSTER_NUM, ZB_FOUR_INPUT_OUT_CLUSTER_NUM);       \
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info## ep_name,		      \
		ZB_FOUR_INPUT_REPORT_ATTR_COUNT);				                      \
	ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL, \
		ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
		(zb_af_simple_desc_1_1_t *)&simple_desc_##ep_name,		              \
		ZB_FOUR_INPUT_REPORT_ATTR_COUNT, reporting_info## ep_name,            \
		0, NULL) // No CVC ctx

#endif // __ZB_FOUR_INPUT_H__
