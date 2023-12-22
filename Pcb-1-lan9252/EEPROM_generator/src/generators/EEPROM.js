/**
 * SOES EEPROM generator
 * EEPROM .bin / .hex code generation logic

* This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator

 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

// ####################### EEPROM generating ####################### //

function hex_generator(form, stringOnly=false)
{
	//WORD ADDRESS 0-7
	var record = getConfigDataBytes(form);
	if (stringOnly) { return getConfigDataString(record, form.ESC.value); }

	/** Takes form, returns config data: 
	 * first 16 bytes (8 words) with check sum */
	function getConfigDataBytes(form) {
		const recordLength = parseInt(form.EEPROMsize.value);
		var record = new Uint8Array(recordLength);
		record.fill(0xFF);
		//Start of EEPROM contents; A lot of information can be found in 5.4 of ETG1000.6
		let pdiControl = 0x05;
		const spiMode = parseInt(form.SPImode.value);  // valid values ara 0, 1, 2 or 3
		let reserved_0x05 = 0x0000; 

		switch(form.ESC.value)  {
			case SupportedESC.AX58100: 
				reserved_0x05 = 0x001A; // enable IO for SPI driver on AX58100:
			    // Write 0x1A value (INT edge pulse length, 8 mA Control + IO 9:0 Drive Select) to 0x0A (Host Interface Extend Setting and Drive Strength
				break;
			case SupportedESC.LAN9252: 
				pdiControl = 0x80;
				break;
			case SupportedESC.LAN9253_Beckhoff: 
				reserved_0x05 = 0xC040; // enable ERRLED, STATE_RUNLED and MI Write 
				// in ASIC CONFIGURATION REGISTER: 0142h-0143h (refer to DS00003421A-page 268)
				break;
			default:
				break;
		}
		
		//WORD ADDRESS 0-7
		writeEEPROMbyte_byteaddress(pdiControl, 0, record); //PDI control: SPI slave (mapped to register 0x0140)
		writeEEPROMbyte_byteaddress(0x06, 1, record); //ESC configuration: Distributed clocks Sync Out and Latch In enabled (mapped register 0x0141)
		writeEEPROMbyte_byteaddress(spiMode, 2, record); //SPI mode (mapped to register 0x0150)
		writeEEPROMbyte_byteaddress(0x44, 3, record); //SYNC /LATCH configuration (mapped to 0x0151). Make both Syncs output
		writeEEPROMword_wordaddress(0x0064, 2, record); //Syncsignal Pulselenght in 10ns units(mapped to 0x0982:0x0983)
		writeEEPROMword_wordaddress(0x00, 3, record); //Extended PDI configuration (none for SPI slave)(0x0152:0x0153)
		writeEEPROMword_wordaddress(0x00, 4, record); //Configured Station Alias (0x0012:0x0013)
		writeEEPROMword_wordaddress(reserved_0x05, 5, record); //Reserved, 0 (when not AX58100)
		writeEEPROMword_wordaddress(0, 6, record); //Reserved, 0
		const crc = FindCRC(record, 14);
		writeEEPROMword_wordaddress(crc, 7, record); //CRC
		
		return record;
	}

	//WORD ADDRESS 8-15
	writeEEPROMDword_wordaddress(parseInt(form.VendorID.value),8,record);		//CoE 0x1018:01
	writeEEPROMDword_wordaddress(parseInt(form.ProductCode.value),10,record);	//CoE 0x1018:02
	writeEEPROMDword_wordaddress(parseInt(form.RevisionNumber.value),12,record);//CoE 0x1018:03
	writeEEPROMDword_wordaddress(parseInt(form.SerialNumber.value),14,record);	//CoE 0x1018:04
	//WORD ADDRESS 16-23
	writeEEPROMword_wordaddress(0,16,record); //Execution Delay Time; units?
	writeEEPROMword_wordaddress(0,17,record); //Port0 Delay Time; units?
	writeEEPROMword_wordaddress(0,18,record); //Port1 Delay Time; units?
	writeEEPROMword_wordaddress(0,19,record); //Reserved, zero
	writeEEPROMword_wordaddress(0,20,record); //Bootstrap Rx mailbox offset    //Bootstrap not supported
	writeEEPROMword_wordaddress(0,21,record); //Bootstrap Rx mailbox size
	writeEEPROMword_wordaddress(0,22,record); //Bootstrap Tx mailbox offset
	writeEEPROMword_wordaddress(0,23,record); //Bootstrap Tx mailbox size
	//WORD ADDRESS 24-...
	writeEEPROMword_wordaddress(parseInt(form.RxMailboxOffset.value),24,record); //Standard Rx mailbox offset   
	writeEEPROMword_wordaddress(parseInt(form.MailboxSize.value),25,record); //Standard Rx mailbox size
	writeEEPROMword_wordaddress(parseInt(form.TxMailboxOffset.value),26,record); //Standard Tx mailbox offset
	writeEEPROMword_wordaddress(parseInt(form.MailboxSize.value),27,record); //Standard Tx mailbox size
	writeEEPROMword_wordaddress(0x04,28,record); //CoE protocol, see Table18 in ETG1000.6
	for (var count = 29; count <= 61; count++) {		//fill reserved area with zeroes
		writeEEPROMword_wordaddress(0,count,record);
	}
	writeEEPROMword_wordaddress((Math.floor(parseInt(form.EEPROMsize.value)/128))-1,62,record); //EEPROM size
	writeEEPROMword_wordaddress(1,63,record); //Version
	////////////////////////////////////
	///    Vendor Specific Info	      //
	////////////////////////////////////
	
	//Strings
	var array_of_strings = [form.TextDeviceType.value, form.TextGroupType.value, form.ImageName.value, form.TextDeviceName.value];
	var offset = 0;
	offset = writeEEPROMstrings(record, 0x80, array_of_strings); //See ETG1000.6 Table20
	//General info
	offset = writeEEPROMgeneral_settings(form,offset,record); //See ETG1000.6 Table21
	//FMMU
	offset = writeFMMU(form,offset, record); //see Table 22 ETG1000.6
	//SyncManagers
	offset = writeSyncManagers(form, offset, record); //See Table 23 ETG1000.6
	//End of EEPROM contents
	const eepromSize = getForm().EEPROMsize.value;
	
	return record;
	
	/** See ETG1000.6 Table20 for Category string */
	function writeEEPROMstrings(record, offset, a_strings)
	{
		var number_of_strings = a_strings.length;
		var total_string_data_length = 0;
		var length_is_even;
		for(var strcounter = 0; strcounter < number_of_strings ; strcounter++)
		{
			total_string_data_length += a_strings[strcounter].length //add length of strings
		}
		total_string_data_length += number_of_strings; //for each string a byte is needed to indicate the length
		total_string_data_length += 1; //for byte to give 'number of strings'
		if(total_string_data_length %2) //if length is even (ends at word boundary)
			length_is_even = false;
		else
			length_is_even = true;
		writeEEPROMword_wordaddress(0x000A, offset/2, record); //Type: STRING
		writeEEPROMword_wordaddress(Math.ceil(total_string_data_length/2), (offset/2) + 1, record); //write length of complete package
		offset += 4; //2 words written
		writeEEPROMbyte_byteaddress(number_of_strings, offset++, record);
		for(var strcounter = 0; strcounter < number_of_strings ; strcounter++)
		{
			writeEEPROMbyte_byteaddress(a_strings[strcounter].length, offset++, record);
			for(var charcounter = 0 ; charcounter < a_strings[strcounter].length ; charcounter++)
			{
				writeEEPROMbyte_byteaddress(a_strings[strcounter].charCodeAt(charcounter), offset++, record);
			}	
		}
		if(length_is_even == false)
		{
			writeEEPROMbyte_byteaddress(0, offset++, record);
		}
		return offset;
	}
	/** See ETG1000.6 Table21 */
	function writeEEPROMgeneral_settings(form,offset,record)
	{
		const General_category = 0x1E; // value: 30d
		const categorysize = 0x10;
		//Clear memory region
		for(let wordcount = 0; wordcount < categorysize + 2; wordcount++) {
			writeEEPROMword_wordaddress(0, (offset/2) + wordcount, record);
		}
		//write code 30, 'General type'. See ETG1000.6, Table 19
		writeEEPROMword_wordaddress(General_category, offset/2, record);
		//write length of General Category data
		writeEEPROMword_wordaddress(categorysize, 1+(offset/2), record);
		offset +=4;
		writeEEPROMbyte_byteaddress(2,offset++,record);//index to string for Group Info
		writeEEPROMbyte_byteaddress(3,offset++,record);//index to string for Image Name
		writeEEPROMbyte_byteaddress(1,offset++,record);//index to string for Device Order Number
		writeEEPROMbyte_byteaddress(4,offset++,record);//index to string for Device Name Information
		offset++; //byte 4 is reserved
		writeEEPROMbyte_byteaddress(getCOEdetails(form),offset++,record);//CoE Details
		writeEEPROMbyte_byteaddress(0,offset++,record); //Enable FoE
		writeEEPROMbyte_byteaddress(0,offset++,record); //Enable EoE
		writeEEPROMbyte_byteaddress(0,offset++,record); //reserved
		writeEEPROMbyte_byteaddress(0,offset++,record); //reserved
		writeEEPROMbyte_byteaddress(0,offset++,record); //reserved
		writeEEPROMbyte_byteaddress(0,offset++,record); //flags (Bit0: Enable SafeOp, Bit1: Enable notLRW
		writeEEPROMword_wordaddress(0x0000, offset/2, record); //current consumption in mA
		offset += 2;
		writeEEPROMword_wordaddress(0x0000, offset/2, record); //2 pad bytes
		offset += 2;
		writeEEPROMword_wordaddress(getPhysicalPort(form), offset/2, record);
		offset += 2;
		offset += 14; //14 pad bytes
		return offset;
	}
	/** See ETG1000.6 Table 22 */
	function writeFMMU(form, offset, record)
	{
		const FMMU_category = 0x28 // 40d
		writeEEPROMword_wordaddress(FMMU_category,offset/2,record);
		offset += 2;
		const length = 2                                 //length = 2 word = 4bytes: 3 FMMU's + padding
														 //length = 1 word = 2bytes: 2 FMMU's.
		writeEEPROMword_wordaddress(length, offset/2, record);
		offset += 2;
		writeEEPROMbyte_byteaddress(1, offset++, record); //FMMU0 used for Outputs; see Table 22 ETG1000.6
		writeEEPROMbyte_byteaddress(2, offset++, record); //FMMU1 used for Inputs;  see Table 22 ETG1000.6
		writeEEPROMbyte_byteaddress(3, offset++, record); //FMMU2 used for Mailbox State
		writeEEPROMbyte_byteaddress(0, offset++, record); //padding, disable FMMU4 if exists
		
		return offset;
	}
	/** See Table 23 ETG1000.6 */
	function writeSyncManagers(form, offset, record)
	{
		const SyncManager_category = 0x29 // 41d
		writeEEPROMword_wordaddress(SyncManager_category, offset/2, record); //SyncManager
		offset += 2;
		writeEEPROMword_wordaddress(0x10, offset/2, record); //size of structure category
		offset += 2;
		//SM0
		writeEEPROMword_wordaddress(parseInt(form.RxMailboxOffset.value),offset/2, record); //Physical start address
		offset += 2;
		writeEEPROMword_wordaddress(parseInt(form.MailboxSize.value),offset/2, record); //Physical size
		offset += 2;
		writeEEPROMbyte_byteaddress(0x26,offset++, record); //Mode of operation
		writeEEPROMbyte_byteaddress(0,offset++, record); //don't care
		writeEEPROMbyte_byteaddress(1,offset++, record); //Enable Syncmanager; bit0: enable, bit 1: fixed content, bit 2: virtual SyncManager, bit 3: Op Only
		writeEEPROMbyte_byteaddress(1,offset++, record); //SyncManagerType; 0: not used, 1: Mbx out, 2: Mbx In, 3: PDO, 4: PDI
		//SM1
		writeEEPROMword_wordaddress(parseInt(form.TxMailboxOffset.value),offset/2, record); //Physical start address
		offset += 2;
		writeEEPROMword_wordaddress(parseInt(form.MailboxSize.value),offset/2, record); //Physical size
		offset += 2;
		writeEEPROMbyte_byteaddress(0x22,offset++, record); //Mode of operation
		writeEEPROMbyte_byteaddress(0,offset++, record); //don't care
		writeEEPROMbyte_byteaddress(1,offset++, record); //Enable Syncmanager; bit0: enable, bit 1: fixed content, bit 2: virtual SyncManager, bit 3: Op Only
		writeEEPROMbyte_byteaddress(2,offset++, record); //SyncManagerType; 0: not used, 1: Mbx out, 2: Mbx In, 3: PDO, 4: PDI
		//SM2
		writeEEPROMword_wordaddress(parseInt(form.SM2Offset.value),offset/2, record); //Physical start address
		offset += 2;
		writeEEPROMword_wordaddress(0,offset/2, record); //Physical size
		offset += 2;
		writeEEPROMbyte_byteaddress(0x24,offset++, record); //Mode of operation
		writeEEPROMbyte_byteaddress(0,offset++, record); //don't care
		writeEEPROMbyte_byteaddress(1,offset++, record); //Enable Syncmanager; bit0: enable, bit 1: fixed content, bit 2: virtual SyncManager, bit 3: Op Only
		writeEEPROMbyte_byteaddress(3,offset++, record); //SyncManagerType; 0: not used, 1: Mbx out, 2: Mbx In, 3: PDO, 4: PDI
		//SM3
		writeEEPROMword_wordaddress(parseInt(form.SM3Offset.value),offset/2, record); //Physical start address
		offset += 2;
		writeEEPROMword_wordaddress(0,offset/2, record); //Physical size
		offset += 2;
		writeEEPROMbyte_byteaddress(0x20,offset++, record); //Mode of operation
		writeEEPROMbyte_byteaddress(0,offset++, record); //don't care
		writeEEPROMbyte_byteaddress(1,offset++, record); //Enable Syncmanager; bit0: enable, bit 1: fixed content, bit 2: virtual SyncManager, bit 3: Op Only
		writeEEPROMbyte_byteaddress(4,offset++, record); //SyncManagerType; 0: not used, 1: Mbx out, 2: Mbx In, 3: PDO, 4: PDI
		return offset;
	}
	function getCOEdetails(form)
	{
		let coedetails = 0;
		if(form.CoeDetailsEnableSDO.checked) coedetails |= 0x01; 	//Enable SDO
		if(form.CoeDetailsEnableSDOInfo.checked) coedetails |= 0x02;	//Enable SDO Info
		if(form.CoeDetailsEnablePDOAssign.checked) coedetails |= 0x04;	//Enable PDO Assign
		if(form.CoeDetailsEnablePDOConfiguration.checked) coedetails |= 0x08;	//Enable PDO Configuration
		if(form.CoeDetailsEnableUploadAtStartup.checked) coedetails |= 0x10;	//Enable Upload at startup
		if(form.CoeDetailsEnableSDOCompleteAccess.checked) coedetails |= 0x20;	//Enable SDO complete access
		return coedetails;
	}
	/** ETG1000.6 Table 21 */
	function getPhysicalPort(form)
	{
		let portinfo = 0;
		let physicals = [form.Port3Physical.value, form.Port2Physical.value, form.Port1Physical.value, form.Port0Physical.value];
		for (var physicalcounter = 0; physicalcounter < physicals.length ; physicalcounter++)
		{
			portinfo = (portinfo << 4); //shift previous result
			switch(physicals[physicalcounter])
			{
			case 'Y':
			case 'H':
				portinfo |= 0x01; //MII
				break;
			case 'K':
				portinfo |= 0x03; //EBUS
				break;
			default:
				portinfo |= 0; 	//No connection
			}
		}
		return portinfo;
	}

	/** computes crc value */
	function FindCRC(data,datalen) {
		var i,j;
		var c;
		var CRC=0xFF;
		var genPoly = 0x07;
		for (j=0; j<datalen; j++)
		{
			c = data[j];
			CRC ^= c;
			for(i = 0; i<8; i++)
				if(CRC & 0x80 )
				CRC = (CRC << 1) ^ genPoly;
				else
				CRC <<= 1;
			CRC &= 0xff;
		}
		return CRC;
	}
	
	function writeEEPROMbyte_byteaddress(byte, address, record)
	{
		record[address] = byte;
	}
	
	function writeEEPROMbyte_wordaddress(byte, address, record)
	{
		record[address*2] = byte;
	}
	
	function writeEEPROMword_wordaddress(word, address, record)
	{//little endian word storage!
		record[     address*2 ] = word&0xFF;
		record[1 + (address*2)] = (word>>8) & 0xFF;
	}
	
	function writeEEPROMDword_wordaddress(word, address, record)
	{//little endian word storage!
		record[     address*2 ] = word&0xFF;
		record[1 + (address*2)] = (word>>8) & 0xFF;
		record[2 + (address*2)] = (word>>16) & 0xFF;
		record[3 + (address*2)] = (word>>24) & 0xFF;
	}

	/** takes bytes array and count, returns ConfigData string */
	function getConfigDataString(record, esc) {
		const configdata_bytecount = new Set(configOnReservedBytes).has(esc) ? 14 : 7;

		var configdata = '';
		for (var bytecount = 0; bytecount < configdata_bytecount; bytecount++) {
			configdata += (record[bytecount] + 0x100).toString(16).slice(-2).toUpperCase();
		}
		return configdata;
	}
}
