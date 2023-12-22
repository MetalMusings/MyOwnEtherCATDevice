/**
 * SOES EEPROM generator
 * Files input and output

 * This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator

 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

// ####################### File operations ####################### //

/** save file in local filesystem, by downloading from browser */
function downloadFile(content, fileName, contentType) {
    var a = document.createElement("a");
    var file = new Blob([content], {type: contentType});
    a.href = URL.createObjectURL(file);
    a.download = fileName;
    a.click();
	// a element will be garbage collected, no need to cleanup
}

/** reads saved project from file user opened */
function readFile(e) {
	var file = e.target.files[0];
	if (!file) return;
	var reader = new FileReader();
	reader.onload = function(e) {
		onRestoreComplete(e.target.result);
  	}
	reader.readAsText(file);
}

/** takes bytes array, returns Intel Hex as string */
function toIntelHex(record) {
	var hex = "";
	const bytes_per_rule = 32;
	const rulesTotalCount = record.length/bytes_per_rule;

	for (var rulenumber = 0 ; rulenumber < (rulesTotalCount); rulenumber++)
	{
		const sliceStart = rulenumber*bytes_per_rule;
		const sliceEnd = bytes_per_rule + (rulenumber * bytes_per_rule);
		const recordSlice = record.slice(sliceStart, sliceEnd);
		hex += CreateiHexRule(bytes_per_rule, rulenumber, recordSlice);
	}
	//end of file marker
	hex += ':00000001FF';
	return hex.toUpperCase();

	function CreateiHexRule(bytes_per_rule, rulenumber, record)
	{
		var record_type_datarecord  = '00';
		var rule = ':'+ bytes_per_rule.toString(16).slice(-2) + generate_hex_address(rulenumber*bytes_per_rule) + record_type_datarecord;
		for(var byteposition = 0; byteposition < bytes_per_rule ; byteposition++)
		{
			var byte = record[byteposition].toString(16).slice(-2); // convert to hexadecimal, crop to last 2 digits
			if(byte.length < 2)
				byte = '0' + byte; //minimal field width  = 2 characters.
			rule += byte;
		}
		var checksum  = 0;
		for(var rule_pos = 0 ; rule_pos < (rule.length-1)/2 ; rule_pos++)
		{
			var byte = parseInt(rule.slice(1+(2*rule_pos), 3+(2*rule_pos)),16);
			checksum  += byte;
		}
		checksum %= 0x100; //leave last byte
		checksum = 0x100-checksum; //two's complement
		rule += checksum.toString(16).slice(-2) + '\n';
		return rule;
	}
	/** takex number, returns its hexadecimal value padded/trimmed to 4 digits */
	function generate_hex_address(number)
	{
		//convert to hexadecimal string
		var output = number.toString(16);
		//take care that 4 characters are present
		while(output.length<4)
		{
			output ='0' + output;
		}
		//return 4 characters, prevents overflow
		return output.slice(-4);
	}
}
