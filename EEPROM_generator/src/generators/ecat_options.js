/**
 * SOES EEPROM generator
 * ecat_options.h code generation logic

* This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator

 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

// ####################### ecat_options.h generation ####################### //

function ecat_options_generator(form, od, indexes)
{
	let ecat_options = '#ifndef __ECAT_OPTIONS_H__\n#define __ECAT_OPTIONS_H__\n\n#define USE_FOE          0\n#define USE_EOE          0\n\n';

	//Mailbox size
	ecat_options += '#define MBXSIZE          ' + parseInt(form.MailboxSize.value).toString()
				+ '\n#define MBXSIZEBOOT      ' + parseInt(form.MailboxSize.value).toString()
				+ '\n#define MBXBUFFERS       3\n\n';
	//Mailbox 0 Config
	ecat_options += `#define MBX0_sma         0x${indexToString(form.RxMailboxOffset.value)}`
				+ '\n#define MBX0_sml         MBXSIZE' 
				+ '\n#define MBX0_sme         MBX0_sma+MBX0_sml-1' 
				+ '\n#define MBX0_smc         0x26\n';
	//Mailbox 1 Config
	ecat_options += `#define MBX1_sma         MBX0_sma+MBX0_sml` //'0x${indexToString(form.TxMailboxOffset.value)}`;
				+ '\n#define MBX1_sml         MBXSIZE' 
				+ '\n#define MBX1_sme         MBX1_sma+MBX1_sml-1'
				+ '\n#define MBX1_smc         0x22\n\n';
	// Mailbox boot configuration
	ecat_options += `#define MBX0_sma_b       0x${indexToString(form.RxMailboxOffset.value)}`
				+ '\n#define MBX0_sml_b       MBXSIZEBOOT' 
				+ '\n#define MBX0_sme_b       MBX0_sma_b+MBX0_sml_b-1' 
				+ '\n#define MBX0_smc_b       0x26\n';
	ecat_options += `#define MBX1_sma_b       MBX0_sma_b+MBX0_sml_b` //'0x${indexToString(form.TxMailboxOffset.value)}`;
				+ '\n#define MBX1_sml_b       MBXSIZEBOOT' 
				+ '\n#define MBX1_sme_b       MBX1_sma_b+MBX1_sml_b-1'
				+ '\n#define MBX1_smc_b       0x22\n\n';
	//SyncManager2 Config
	ecat_options += `#define SM2_sma          0x${indexToString(form.SM2Offset.value)}`
				+ '\n#define SM2_smc          0x24' 
				+ '\n#define SM2_act          1\n';
	//SyncManager3 Config
	ecat_options += `#define SM3_sma          0x${indexToString(form.SM3Offset.value)}`
				+ '\n#define SM3_smc          0x20'
				+ '\n#define SM3_act          1\n\n';
	// Mappings config
	const MAX_MAPPINGS_SM2 = getMaxMappings(od, indexes, rxpdo);
	const MAX_MAPPINGS_SM3 = getMaxMappings(od, indexes, txpdo);
	ecat_options += `#define MAX_MAPPINGS_SM2 ${MAX_MAPPINGS_SM2}`
				+ `\n#define MAX_MAPPINGS_SM3 ${MAX_MAPPINGS_SM3}\n\n`
	// PDO buffer config
	ecat_options += '#define MAX_RXPDO_SIZE   512' 	// TODO calculate based on offset, size 
				+ '\n#define MAX_TXPDO_SIZE   512\n\n'
				+ '#endif /* __ECAT_OPTIONS_H__ */\n';

	return ecat_options;

	function getMaxMappings(od, indexes, pdoName) {
		let result = 0;

		indexes.forEach(index => {
			const objd = od[index];
			if(objd.pdo_mappings) {
				if(objd.items) {
					objd.items.slice(1).forEach(subitem => {
						objd.pdo_mappings.forEach(mapping => {
							if (mapping == pdoName) {
								++result;
								if (subitem.dtype == DTYPE.BOOLEAN) {
									++result; // boolean padding is mapping too
									// TODO handle array of booleans
								}
							}
						});
					});
				} else if(objd.pdo_mappings) {
					objd.pdo_mappings.forEach(mapping => {
						if (mapping == pdoName) { 
							++result;
							if (objd.dtype == DTYPE.BOOLEAN) {
								++result; // boolean padding is mapping too
							}
						}
					});
				};		
			};
		});
		return result;
	}
}
