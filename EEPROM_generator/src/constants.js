/**
 * SOES EEPROM generator
 * Shared constants, data types

 * This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator

 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

const automaticCodegen = true; 		// code is regenerated on every form change. 
									// no need to remember to generate before copying or downloading
									// app is noticeably slower

// ####################### Constants, lookup tables ####################### //
/** CoE Object Types */
const OTYPE = {
	VAR : 'VAR',
	ARRAY : 'ARRAY',
	RECORD: 'RECORD',
};
/** CoE Data Types */
const DTYPE = {
	BOOLEAN : 'BOOLEAN',
	INTEGER8 : 'INTEGER8',
	INTEGER16 : 'INTEGER16',
	INTEGER32 : 'INTEGER32',
	UNSIGNED8 : 'UNSIGNED8',
	UNSIGNED16 : 'UNSIGNED16',
	UNSIGNED32 : 'UNSIGNED32',
	REAL32 : 'REAL32',
	VISIBLE_STRING : 'VISIBLE_STRING',
	/* TODO implement missing less common types */
	// OCTET_STRING : 'OCTET_STRING',
	// UNICODE_STRING : 'UNICODE_STRING',
	// INTEGER24 : 'INTEGER24',
	// UNSIGNED24 : 'UNSIGNED24',
	// INTEGER64 : 'INTEGER64',
	// UNSIGNED64 : 'UNSIGNED64',
	// REAL64 : 'REAL64',
	// PDO_MAPPING : 'PDO_MAPPING',
};
/** Data types bitsize as used in objectlist.c  */
const dtype_bitsize = {
	'BOOLEAN' : 1,
	'INTEGER8' : 8,
	'INTEGER16' : 16,
	'INTEGER32' : 32,
	'UNSIGNED8' : 8,
	'UNSIGNED16' : 16,
	'UNSIGNED32' : 32,
	'REAL32' : 32,
	'VISIBLE_STRING' : 8,
};
const booleanPaddingBitsize = 7;
/** ESI XML data type */
const ESI_DT = {
	'BOOLEAN': { name: 'BOOL', bitsize: 1, ctype: 'uint8_t' },
	'INTEGER8': { name: 'SINT', bitsize: 8, ctype: 'int8_t' },
	'INTEGER16': { name: 'INT', bitsize: 16, ctype: 'int16_t' },
	'INTEGER32': { name: 'DINT', bitsize: 32, ctype: 'int32_t' },
	'UNSIGNED8': { name: 'USINT', bitsize: 8, ctype: 'uint8_t' },
	'UNSIGNED16': { name: 'UINT', bitsize: 16, ctype: 'uint16_t' },
	'UNSIGNED32': { name: 'UDINT', bitsize: 32, ctype: 'uint32_t' },
	'REAL32': { name: 'REAL', bitsize: 32, ctype: 'float' }, // TODO check C type name
	'VISIBLE_STRING': { name: 'STRING', bitsize: 8, ctype: 'char *' }, // TODO check C type name
};

/** These are required by minimal CiA 301 device */
const SDO_category = {
	'1000': 'm',
	'1009': 'o',
};

// ####################### Object Dictionary building ####################### //
/** 
 * Returns Object Dictionaty stub with mandatory objects.
 * OD index is hexadecimal value without '0x' prefix
 */ 
function getMandatoryObjects() {
	const OD = {
		'1000': { otype: OTYPE.VAR, dtype: DTYPE.UNSIGNED32, name: 'Device Type', value: 0x1389 },
		'1008': { otype: OTYPE.VAR, dtype: DTYPE.VISIBLE_STRING, name: 'Device Name', data: '' },
		'1009': { otype: OTYPE.VAR, dtype: DTYPE.VISIBLE_STRING, name: 'Hardware Version', data: '' },
		'100A': { otype: OTYPE.VAR, dtype: DTYPE.VISIBLE_STRING, name: 'Software Version', data: '' },
		'1018': { otype: OTYPE.RECORD, name: 'Identity Object', items: [
			{ name: 'Max SubIndex' },
			{ name: 'Vendor ID', dtype: DTYPE.UNSIGNED32, value: 600 },
			{ name: 'Product Code', dtype: DTYPE.UNSIGNED32 },
			{ name: 'Revision Number', dtype: DTYPE.UNSIGNED32 },
			{ name: 'Serial Number', dtype: DTYPE.UNSIGNED32, data: '&Obj.serial' },
		]},
		'1C00': { otype: OTYPE.ARRAY, dtype: DTYPE.UNSIGNED8, name: 'Sync Manager Communication Type', items: [
			{ name: 'Max SubIndex' },
			{ name: 'Communications Type SM0', value: 1 },
			{ name: 'Communications Type SM1', value: 2 },
			{ name: 'Communications Type SM2', value: 3 },
			{ name: 'Communications Type SM3', value: 4 },
		]},
	};
	return OD;
};

const sdo = 'sdo';
const txpdo = 'txpdo';
const rxpdo = 'rxpdo';

/** EtherCAT Slave Chips that are supported by this web tool */
const SupportedESC = {
	AX58100: 'AX58100',
	ET1100: 'ET1100',
	LAN9252: 'LAN9252',
	LAN9253_Beckhoff: 'LAN9253 Beckhoff',
};

/** ESCs that are using reserved bytes for configuration, for example AX58100 configdata reaches 0x0A byte */
const configOnReservedBytes = [SupportedESC.AX58100, SupportedESC.LAN9253_Beckhoff];

//** Form default values */
function getFormDefaultValues() {
	return {form: {
		VendorName: "ACME EtherCAT Devices",
		VendorID: "0x000",
		ProductCode: "0x00ab123",
		ProfileNo: "5001",
		RevisionNumber: "0x002",
		SerialNumber: "0x001",
		HWversion: "0.0.1",
		SWversion: "0.0.1",
		EEPROMsize: "2048",
		RxMailboxOffset: "0x1000",
		TxMailboxOffset: "0x1200",
		MailboxSize: "512",
		SM2Offset: "0x1600",
		SM3Offset: "0x1A00",
		TextGroupType: "DigIn",
		TextGroupName5: "Digital input",
		ImageName: "IMGCBY",
		TextDeviceType: "DigIn2000",
		TextDeviceName: "2-channel Hypergalactic input superimpermanator",
		Port0Physical: "Y",
		Port1Physical: "Y",
		Port2Physical: " ",
		Port3Physical: " ",
		ESC: SupportedESC.ET1100,
		SPImode: "3",
		CoeDetailsEnableSDO: true,
		CoeDetailsEnableSDOInfo: true,
		CoeDetailsEnablePDOAssign: false,
		CoeDetailsEnablePDOConfiguration: false,
		CoeDetailsEnableUploadAtStartup: true,
		CoeDetailsEnableSDOCompleteAccess: false,
	}};  
};
