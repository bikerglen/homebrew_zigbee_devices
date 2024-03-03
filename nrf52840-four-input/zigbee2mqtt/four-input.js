const {batteryPercentage,identify} = require('zigbee-herdsman-converters/lib/modernExtend');
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const extend = require('zigbee-herdsman-converters/lib/extend');
const ota = require('zigbee-herdsman-converters/lib/ota');
const tuya = require('zigbee-herdsman-converters/lib/tuya');
const {} = require('zigbee-herdsman-converters/lib/tuya');
const utils = require('zigbee-herdsman-converters/lib/utils');
const globalStore = require('zigbee-herdsman-converters/lib/store');
const e = exposes.presets;
const ea = exposes.access;

const fromZigbee_CustomActions = {
    cluster: 'genOnOff',
    type: 'raw',
    convert: (model, msg, publish, options, meta) => {
		if ((0, utils.hasAlreadyProcessedMessage)(msg, model, msg.data[1]))
			return;

		// msg.endpoint.defaultResponse(0xfd, 0, 6, msg.data[1]).catch((error) => { });
        return { action: `cmd_${msg.data[2]}`};
    },
};

const definition = {
    zigbeeModel: ['four-input'],
    model: 'four-input',
    vendor: 'bikerglen.com',
    description: 'four contact closure input device',
    extend: [batteryPercentage(),identify()],
    fromZigbee: [fz.command_off, fz.command_on, fz.command_toggle, fromZigbee_CustomActions, fz.battery],
    toZigbee: [tz.battery_voltage], // permits reading voltage attribute over mqtt
    exposes: [e.battery_voltage()],
};

module.exports = definition;

