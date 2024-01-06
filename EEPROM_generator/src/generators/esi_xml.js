/**
 * SOES EEPROM generator
 * ESI XML code generation logic

* This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator

 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

// ####################### ESI.xml generating ####################### //

function esiDTbitsize(dtype) {
	return ESI_DT[dtype].bitsize;
}

//See ETG2000 for ESI format
function esi_generator(form, od, indexes, dc)
{
	//VendorID
	var esi =`<?xml version="1.0" encoding="UTF-8"?>\n<EtherCATInfo>\n  <Vendor>\n    <Id>${parseInt(form.VendorID.value).toString()}</Id>\n`;
	//VendorName
	esi += `    <Name LcId="1033">${form.VendorName.value}</Name>\n  </Vendor>\n  <Descriptions>\n`;
	//Groups
	esi += `    <Groups>\n      <Group>\n        <Type>${form.TextGroupType.value}</Type>\n        <Name LcId="1033">${form.TextGroupName5.value}</Name>\n      </Group>\n    </Groups>\n    <Devices>\n`;
	//Physics  
	esi += `      <Device Physics="${form.Port0Physical.value + form.Port1Physical.value + form.Port2Physical.value || + form.Port3Physical.value}">\n        <Type ProductCode="#x${parseInt(form.ProductCode.value).toString(16)}" RevisionNo="#x${parseInt(form.RevisionNumber.value).toString(16)}">${form.TextDeviceType.value}</Type>\n`;
	//Add  Name info
	esi += `        <Name LcId="1033">${form.TextDeviceName.value}</Name>\n`;
	//Add in between
	esi += `        <GroupType>${form.TextGroupType.value}</GroupType>\n`;
	//Add profile
	esi += `        <Profile>\n          <ProfileNo>${form.ProfileNo.value}</ProfileNo>\n          <AddInfo>0</AddInfo>\n          <Dictionary>\n            <DataTypes>`;
	const customTypes = {};
	const variableTypes = {};
	
	function addVariableType(element) {
		if (element && element.otype && (element.otype != OTYPE.VAR && element.otype != OTYPE.ARRAY)) { 
			alert(`${element.name} is not OTYPE VAR, cannot treat is as variable type`); return; 
		}
		if (!element || !element.dtype) {
			alert(`${element.name} has no DTYPE, cannot treat is as variable type`); return; 
		}		
		let el_name = esiVariableTypeName(element);
		if (!variableTypes[el_name]) {
			const bitsize = (element.dtype == DTYPE.VISIBLE_STRING) ? esiBitsize(element) : esiDTbitsize(element.dtype);
			variableTypes[el_name] = bitsize;
		}
	}
	function addObjectDictionaryDataType(od, index) {
		const objd = od[index];
		const dtName = esiDtName(objd, index);
		var result = '';

		if (objd.otype == OTYPE.VAR) {
			addVariableType(objd); // variable types will have to be be done later anyway, add to that queue
		} else if (!customTypes[dtName]) {
			// generate data types code for complex objects
			const bitsize = esiBitsize(objd);
			customTypes[dtName] = true;
			result += `\n              <DataType>`;
			
			let flags = `\n                    <Access>ro</Access>`; // PDO assign flags for variables are set in dictionary objects section
			if (objd.otype == OTYPE.ARRAY) {
				addVariableType(objd); // queue variable type to add after array code is generated
				let esi_type = ESI_DT[objd.dtype];
				let arr_bitsize = (objd.items.length - 1) * esi_type.bitsize
				result += `\n                <Name>${dtName}ARR</Name>\n                <BaseType>${esi_type.name}</BaseType>\n                <BitSize>${arr_bitsize}</BitSize>`;
				result += `\n                <ArrayInfo>\n                  <LBound>1</LBound>\n                  <Elements>${objd.items.length - 1}</Elements>\n                </ArrayInfo>`;
				result += `\n              </DataType>`;
				result += `\n              <DataType>`;
			}
			result += `\n                <Name>${dtName}</Name>\n                <BitSize>${bitsize}</BitSize>`;
			result += `\n                <SubItem>\n                  <SubIdx>0</SubIdx>\n                  <Name>Max SubIndex</Name>\n                  <Type>USINT</Type>`
				+ `\n                  <BitSize>8</BitSize>\n                  <BitOffs>0</BitOffs>\n                  <Flags>${flags}\n                  </Flags>\n                </SubItem>`;
			
			flags += getPdoMappingFlags(objd); // PDO assign flags for composite type
			
			switch (objd.otype) {
			case OTYPE.ARRAY: {
				let arr_bitsize = (objd.items.length - 1) * esiDTbitsize(objd.dtype);
				result += `\n                <SubItem>\n                  <Name>Elements</Name>\n                  <Type>${dtName}ARR</Type>\n                  <BitSize>${arr_bitsize}</BitSize>`
						+`\n                  <BitOffs>16</BitOffs>\n                  <Flags>${flags}\n                  </Flags>\n                </SubItem>`;
				break;
			} case OTYPE.RECORD: {
				let subindex = 0;
				let bits_offset = 16;
				objd.items.forEach(subitem => {
					if (subindex > 0) { // skipped Max Subindex
						addVariableType(subitem); // cannot add variable type now that record code is being generated
						let subitem_dtype = ESI_DT[subitem.dtype];
						let subitem_bitsize = subitem_dtype.bitsize
						const subitemFlags = getSubitemFlags(objd, subitem);
						result += `\n                <SubItem>\n                  <SubIdx>${subindex}</SubIdx>\n                  <Name>${subitem.name}</Name>` 
							+ `\n                  <Type>${subitem_dtype.name}</Type>\n                  <BitSize>${subitem_bitsize}</BitSize>\n                  <BitOffs>${bits_offset}</BitOffs>`
							+ `\n                  <Flags>${subitemFlags}\n                  </Flags>`
							+ `\n                </SubItem>`;
						bits_offset += subitem_bitsize;
					}
					subindex++;
				});
				break;
			} default: {
				alert(`Object ${index} "${objd.name}" has unexpected OTYPE ${objd.otype}`);
				alert;
			}}
			result += `\n              </DataType>`;
		}

		return result;

		function getSubitemFlags(objd, subitem) {
			let access = 'ro';
			let modifier = '';
			if (subitem.access) {
				access = subitem.access.slice(0,2).toLowerCase();
				modifier = ' WriteRestrictions="PreOP"';
			}
			let flags = `\n                    <Access${modifier}>${access}</Access>`; // PDO assign flags for variables are set in dictionary objects section
			flags += getPdoMappingFlags(objd); // PDO assign flags for composite type
			return flags;
		}
	}
	// Add objects dictionary data types
	indexes.forEach(index => { esi += addObjectDictionaryDataType(od, index); });
	// Add variable type
	Object.entries(variableTypes).forEach(variableType => {
		esi += `\n              <DataType>`;
		esi += `\n                <Name>${variableType[0]}</Name>\n                <BitSize>${variableType[1]}</BitSize>`;			
		esi += `\n              </DataType>`;
	});
	esi += `\n            </DataTypes>\n            <Objects>`;
	// Add objects dictionary
	function addDictionaryObject(od, index) {
		const objd = od[index];
		const el_dtype = esiDtName(objd, index);
		const bitsize = esiBitsize(objd);
		let result = `\n              <Object>\n                <Index>#x${index}</Index>\n                <Name>${objd.name}</Name>\n                <Type>${el_dtype}</Type>\n                <BitSize>${bitsize}</BitSize>\n                <Info>`;
		if (objd.data) {
			if (objd.dtype == DTYPE.VISIBLE_STRING) {
				result += `\n                  <DefaultString>${objd.data}</DefaultString>`;	
			}
		}
		if (objd.value) {
			result += `\n                  <DefaultValue>${toEsiHexValue(objd.value)}</DefaultValue>`;
		}
		//Add object subitems for complex types
		if (objd.items) {
			result += addDictionaryObjectSubitems(objd.items);
		}
	
		var flags = `\n                  <Access>ro</Access>`;
		if (objd.otype == OTYPE.VAR) {
			flags += getPdoMappingFlags(objd);
		}
		if (SDO_category[index]) { 
			flags += `\n                  <Category>${SDO_category[index]}</Category>`; 
		}
		result += `\n                </Info>\n                <Flags>${flags}\n                </Flags>\n              </Object>`;
		return result;

		function addDictionaryObjectSubitems(element_items) {
			const max_subindex_value = element_items.length - 1;
			var result = ""
			let subindex = 0;
			element_items.forEach(subitem => {
				var defaultValue = (subindex > 0) ? subitem.value : max_subindex_value;
				result += `\n                  <SubItem>\n                    <Name>${subitem.name}</Name>\n                    <Info>\n                      <DefaultValue>${toEsiHexValue(defaultValue)}</DefaultValue>\n                    </Info>\n                  </SubItem>`;
				subindex++;
			});
			return result;
		}
	}
	indexes.forEach(index => { esi += addDictionaryObject(od, index); });
	const is_rxpdo = isPdoWithVariables(od, indexes, rxpdo);
	const is_txpdo = isPdoWithVariables(od, indexes, txpdo);

	esi += `\n            </Objects>\n          </Dictionary>\n        </Profile>\n        <Fmmu>Outputs</Fmmu>\n        <Fmmu>Inputs</Fmmu>\n        <Fmmu>MBoxState</Fmmu>\n`;
	//Add Rxmailbox sizes
	esi += `        <Sm DefaultSize="${parseInt(form.MailboxSize.value).toString(10)}" StartAddress="#x${indexToString(form.RxMailboxOffset.value)}" ControlByte="#x26" Enable="1">MBoxOut</Sm>\n`;
	//Add Txmailbox sizes
	esi += `        <Sm DefaultSize="${parseInt(form.MailboxSize.value).toString(10)}" StartAddress="#x${indexToString(form.TxMailboxOffset.value)}" ControlByte="#x22" Enable="1">MBoxIn</Sm>\n`;
	//Add SM2
	esi += `        <Sm StartAddress="#x${indexToString(form.SM2Offset.value)}" ControlByte="#x24" Enable="${is_rxpdo ? 1 : 0}">Outputs</Sm>\n`;
	//Add SM3
	esi += `        <Sm StartAddress="#x${indexToString(form.SM3Offset.value)}" ControlByte="#x20" Enable="${is_txpdo ? 1 : 0}">Inputs</Sm>\n`;
	if (is_rxpdo) {
		var memOffset = getSM2_MappingOffset(form);
		indexes.forEach(index => {
			const objd = od[index];
			
			if (isInArray(objd.pdo_mappings, rxpdo)) {
				esi += addEsiDevicePDO(objd, index, rxpdo, memOffset);
				++memOffset;
			}	
		});
	}
	if (is_txpdo) {
		var memOffset = form.SM3Offset.value;
		indexes.forEach(index => {
			const objd = od[index];
			if (isInArray(objd.pdo_mappings, txpdo)) {
				esi += addEsiDevicePDO(objd, index, txpdo, memOffset);
				++memOffset;
			}
		});
	}
	
	//Add Mailbox DLL
	esi += `        <Mailbox DataLinkLayer="true">\n          <CoE ${getCoEString(form)}/>\n        </Mailbox>\n`;
	//Add DCs
	esi += getEsiDCsection(dc);
	//Add EEPROM
	const configdata = hex_generator(form, true);
	esi +=`        <Eeprom>\n          <ByteSize>${parseInt(form.EEPROMsize.value)}</ByteSize>\n          <ConfigData>${configdata}</ConfigData>\n        </Eeprom>\n`;
	//Close all items
	esi +=`      </Device>\n    </Devices>\n  </Descriptions>\n</EtherCATInfo>`;

	return esi;	

	function addEsiDevicePDO(objd, index, pdo, memOffset) {
		var esi = '';
		const PdoName = pdo[0].toUpperCase();
		const SmNo = (pdo == txpdo) ? 3 : 2;
		const memoryOffset = indexToString(memOffset);
		esi += `        <${PdoName}xPdo Fixed="true" Mandatory="true" Sm="${SmNo}">\n          <Index>#x${memoryOffset}</Index>\n          <Name>${objd.name}</Name>`;
		var subindex = 0;
		switch (objd.otype) {
		case OTYPE.VAR: {
			const esiType = esiVariableTypeName(objd);
			const bitsize = esiDTbitsize(objd.dtype);
			esi += `\n          <Entry>\n            <Index>#x${index}</Index>\n            <SubIndex>#x${subindex.toString(16)}</SubIndex>\n            <BitLen>${bitsize}</BitLen>\n            <Name>${objd.name}</Name>\n            <DataType>${esiType}</DataType>\n          </Entry>`;
			esi += pdoBooleanPadding(objd);
			break;
		}
		case OTYPE.ARRAY: {
			const esiType = esiVariableTypeName(objd);
			const bitsize = esiDTbitsize(objd.dtype);
			subindex = 1;  // skip 'Max subindex'
			objd.items.slice(subindex).forEach(subitem => {
				esi += `\n          <Entry>\n            <Index>#x${index}</Index>\n            <SubIndex>#x${subindex.toString(16)}</SubIndex>\n            <BitLen>${bitsize}</BitLen>\n            <Name>${subitem.name}</Name>\n            <DataType>${esiType}</DataType>\n          </Entry>`;
				// TODO handle padding for array of booleans
				++subindex;
			});
			break;
		}
		case OTYPE.RECORD: {
			subindex = 1;  // skip 'Max subindex'
			objd.items.slice(subindex).forEach(subitem => {
				const esiType = esiVariableTypeName(subitem);
				const bitsize = esiDTbitsize(subitem.dtype);
				esi += `\n          <Entry>\n            <Index>#x${index}</Index>\n            <SubIndex>#x${subindex.toString(16)}</SubIndex>\n            <BitLen>${bitsize}</BitLen>\n            <Name>${subitem.name}</Name>\n            <DataType>${esiType}</DataType>\n          </Entry>`;
				esi += pdoBooleanPadding(subitem);
				++subindex;
			});
			break;
		}
		default: {
			alert(`Unexpected OTYPE ${objd.otype} for ${index} ${objd.name} in ESI ${PdoName}PDOs`);
			break;
		}}
		esi += `\n        </${PdoName}xPdo>\n`;
		
		return esi;

		function pdoBooleanPadding(item) {
			if (item.dtype == DTYPE.BOOLEAN) {
				return `\n          <Entry>\n            <Index>${0}</Index>\n            <SubIndex>${0}</SubIndex>\n            <BitLen>${7}</BitLen>\n          </Entry>`;
			}
			return ``;
		}
	}

	function toEsiHexValue(value) {
		if (!value) { 
			return 0;
		}
		if (value.startsWith && value.startsWith('0x')) {
			value = `#x${value.slice(2)}`;
		}
		return value;
	}

	function getPdoMappingFlags(item) {
		var flags = '';
		if (item.pdo_mappings) {
			if (item.pdo_mappings.length > 1) { 
				alert(`Object ${index} "${objd.name}" has multiple PDO mappings, that is not supported by this version of tool`
				+ `, only first ${pdoMappingFlag}XPDO will be used`);
			}
			const pdoMappingFlag = item.pdo_mappings[0].slice(0,1).toUpperCase();
			flags += `\n                  <PdoMapping>${pdoMappingFlag}</PdoMapping>`; 	
		}
		return flags;
	}

	function getEsiDCsection(dc) {
		if (!dc) {
			return '';
		}
		var dcSection = '        <Dc>';
		dc.forEach(opMode => {
			dcSection += `\n          <OpMode>\n            <Name>${opMode.Name}</Name>\n            <Desc>${opMode.Description}</Desc>\n            <AssignActivate>${opMode.AssignActivate}</AssignActivate>`;
			if (opMode.Sync0cycleTime && opMode.Sync0cycleTime != 0) { 
				dcSection += `\n            <CycleTimeSync0>${opMode.Sync0cycleTime}</CycleTimeSync0>`; 
			}
			if (opMode.Sync0shiftTime && opMode.Sync0shiftTime != 0) { 
				dcSection += `\n            <ShiftTimeSync0>${opMode.Sync0shiftTime}</ShiftTimeSync0>`; 
			}
			if (opMode.Sync1cycleTime && opMode.Sync1cycleTime != 0) { 
				dcSection += `\n            <CycleTimeSync1>${opMode.Sync1cycleTime}</CycleTimeSync1>`; 
			}
			if (opMode.Sync1shiftTime && opMode.Sync1shiftTime != 0) { 
				dcSection += `\n            <ShiftTimeSync1>${opMode.Sync1shiftTime}</ShiftTimeSync1>`; 
			}
			dcSection += `\n          </OpMode>`;
		});
		dcSection += `\n        </Dc>\n`;

		return dcSection;
	}

	//See Table 40 ETG2000
	function getCoEString(form)
	{
		var result = ""
	//	if(form.CoeDetailsEnableSDO.checked) 
	//		result += 'SdoInfo="true" ';
	//	else
	//		result += 'SdoInfo="false" ';
		if(form.CoeDetailsEnableSDOInfo.checked) 
			result += 'SdoInfo="true" ';
		else
			result += 'SdoInfo="false" ';
		if(form.CoeDetailsEnablePDOAssign.checked) 
			result += 'PdoAssign="true" ';	
		else
			result += 'PdoAssign="false" ';
		if(form.CoeDetailsEnablePDOConfiguration.checked) 
			result += 'PdoConfig="true" ';
		else
			result += 'PdoConfig="false" ';
		if(form.CoeDetailsEnableUploadAtStartup.checked) 
			result += 'PdoUpload="true" ';
		else 
			result += 'PdoUpload="false" ';
		if(form.CoeDetailsEnableSDOCompleteAccess.checked) 
			result += 'CompleteAccess="true" ';
		else 
			result +='CompleteAccess="false" ';
		return result;										
	}

	function esiVariableTypeName(element) {
		let el_name = ESI_DT[element.dtype].name;
		if (element.dtype == DTYPE.VISIBLE_STRING) {
			return `${el_name}(${element.data.length})`;
		}
		return el_name;
	}	

	function esiDtName(element, index) {
		switch (element.otype) {
			case OTYPE.VAR:
				return esiVariableTypeName(element);
			case OTYPE.ARRAY:
			case OTYPE.RECORD:
				return `DT${index}`;
			default:
				alert(`Element 0x${index} has unexpected OTYPE ${element.otype}`);
				break;
		}
	}
	
	function esiBitsize(element) {
		switch (element.otype) {
			case OTYPE.VAR: {
				let bitsize = esiDTbitsize(element.dtype);
				if (element.dtype == DTYPE.VISIBLE_STRING) {
					return bitsize * element.data.length;
				}
				return bitsize;
			}
			case OTYPE.ARRAY: {
				const maxsubindex_bitsize = esiDTbitsize(DTYPE.UNSIGNED8);
				let bitsize = esiDTbitsize(element.dtype);
				let elements = element.items.length - 1; // skip max subindex
				return maxsubindex_bitsize * 2 + elements * bitsize;
			}
			case OTYPE.RECORD: {
				const maxsubindex_bitsize = esiDTbitsize(DTYPE.UNSIGNED8);
				let bitsize = maxsubindex_bitsize * 2;
				for (let subindex = 1; subindex < element.items.length; subindex++) {
					const subitem = element.items[subindex];
					bitsize += esiDTbitsize(subitem.dtype);
					if(subitem.dtype == DTYPE.BOOLEAN) {
						bitsize += booleanPaddingBitsize;
					}
				}
				return bitsize;
			}
			default:
				alert(`Element ${element} has unexpected OTYPE ${element.otype}`);
				break;
		}
	}	
}
