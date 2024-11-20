/**
 * SOES EEPROM generator
 * Object Dictionary edition logic

* This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator

 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

/** Object Dictionary sections edited by UI
 * Assumption: single non dynamic PDO */
const _odSections = {
	sdo : {},
	txpdo : {}, // addding PDO requires matching SDO in Sync Manager, and PDO mapping
	rxpdo : {}, // this will be done when stitching sections during code generation
};

function getObjDict() {
    return _odSections;
}

function getObjDictSection(odSectionName) {
	return _odSections[odSectionName];
}

function setObjDictSection(odSectionName, backupValue) {
	_odSections[odSectionName] = backupValue;
}

function objectExists(odSectionName, index) {
	var odSection = getObjDictSection(odSectionName);
	return index && odSection[index];
}

function checkObjectType(expected, objd) {
	if (objd.otype != expected) {
		var msg = `Object ${objd.name} was expected to be OTYPE ${expected} but is ${objd.otype}`;
		alert(msg);
		throw new Exception(msg);
	}
}

function addObject(od, objd, index) {
	if (od[index]) {
		alert(`Object ${objd.name} duplicates 0x${index}: ${od[index].name} !`);
	}
	od[index] = objd;
}

function removeObject(od, index) {
	if (index) {
		if (od[index]) {
			delete od[index];
		} else {
			alert(`Cannot remove object 0x${index}: it does not exist`);
		}
	}
}

function isInArray(array, seekValue) {
	return array && (array[0] == seekValue
		|| array.find(currentValue => currentValue == seekValue));
}

function variableName(objectName) {
	const charsToReplace = [ ' ', '.', ',', ';', ':', '/' ];
	const charsToRemove = [ '+', '-', '*', '=', '!', '@' ];

	var variableName = objectName;
	charsToReplace.forEach(c => {
		variableName = variableName.replaceAll(c, '_');
	});
	charsToRemove.forEach(c => {
		variableName = variableName.replaceAll(c, '');
	});
	return variableName;
}

// ####################### Building Object Dictionary model ####################### //

/** Takes OD entries from UI SDO section and adds to given OD */
function addSDOitems(od) {
	const sdoSection = getObjDictSection(sdo);
	const indexes = getUsedIndexes(sdoSection);

	indexes.forEach(index => {
		const item = sdoSection[index];
		item.isSDOitem = true;
		objectlist_link_utypes(item);

		addObject(od, item, index);
	});
}
/** Returns true if any object in given Object Dictionary has mapping to PDO with given name */
function isPdoWithVariables(od, indexes, pdoName) {
	for (let i = 0; i < indexes.length; i++) {
		const index = indexes[i];
		const objd = od[index];
		if (isInArray(objd.pdo_mappings, pdoName)) {
			return true;
		}
	}
	return false;
}
/** Regardles of value set, SDK was generating RXPDO mappings as SDO1600
 * This offset _can_ be changed, not sure why one would need it
 */
function getSM2_MappingOffset(form) {
	return	parseInt(form.SM2Offset.value);
}
/** Takes OD entries from UI RXPDO section and adds to given OD */
function addRXPDOitems(od) {
	const rxpdoSection = getObjDictSection(rxpdo);
	const form = getForm();
	const pdo = {
		name : rxpdo,
		SMassignmentIndex : '1C12',
		smOffset : getSM2_MappingOffset(form), // usually 0x1600
	};
	addPdoObjectsSection(od, rxpdoSection, pdo);
}
/** Takes OD entries from UI TXPDO section and adds to given OD */
function addTXPDOitems(od) {
	const txpdoSection = getObjDictSection(txpdo);
	const form = getForm();
	const pdo = {
		name : txpdo,
		SMassignmentIndex : '1C13',
		smOffset : parseInt(form.SM3Offset.value), // usually 0x1A00
	};
	addPdoObjectsSection(od, txpdoSection, pdo);
}

var _booleanPaddingCount = 0;
/** 
 * Takes OD entries from given UI SDO/PDO section and adds to given OD
 * using provided SM offset, and SM assignment address.
 
 * Available sections are 'sdo', 'txpdo', 'rxpdo'
 */
function addPdoObjectsSection(od, odSection, pdo){
	var currentSMoffsetValue = pdo.smOffset;
	const indexes = getUsedIndexes(odSection);
	
	if (indexes.length) {
		const pdoAssignments = ensurePDOAssignmentExists(od, pdo.SMassignmentIndex);

		indexes.forEach(index => {
			const objd = odSection[index];
			const currentOffset = indexToString(currentSMoffsetValue)
			
			const pdoMappingObj = { otype: OTYPE.RECORD, name: objd.name, items: [
				{ name: 'Max SubIndex' },
			]};
			// create PDO assignment to SM
			const pdoAssignment = { name: "PDO Mapping", value: `0x${currentOffset}` };
			addPdoMapping(objd, pdo.name);
			
			objectlist_link_utypes(objd);

			switch (objd.otype) {
			case  OTYPE.VAR: {
				// create PDO mapping
				pdoMappingObj.items.push({ name: objd.name, dtype: DTYPE.UNSIGNED32, value: getPdoMappingValue(index, 0, objd.dtype) });
				if (objd.dtype == DTYPE.BOOLEAN) { 
					addBooleanPadding(pdoMappingObj.items, ++_booleanPaddingCount);
				}
				break;
			} 
			case OTYPE.ARRAY: {
				var subindex = 1;
				objd.items.slice(subindex).forEach(subitem => { 
					// create PDO mappings
					pdoMappingObj.items.push({ name: subitem.name, dtype: DTYPE.UNSIGNED32, value: getPdoMappingValue(index, subindex , objd.dtype) });
					// TODO handle padding on array of booleans
					++subindex;
				});
				break;
			}
			case OTYPE.RECORD: {
				var subindex = 1;
				objd.items.slice(subindex).forEach(subitem => {
					// create PDO mappings
					pdoMappingObj.items.push({ name: subitem.name, dtype: DTYPE.UNSIGNED32, value: getPdoMappingValue(index, subindex , subitem.dtype) });
					if (subitem.dtype == DTYPE.BOOLEAN) { 
						addBooleanPadding(pdoMappingObj.items, ++_booleanPaddingCount);
					}
					++subindex;
				});
				break;
			}
			default: {
				alert(`${pdoMappingValue} object ${index} ${objd.name} has unexpected object type ${objd.otype}!`);
				break;
			}}

			addObject(od, pdoMappingObj, currentOffset);
			pdoAssignments.items.push(pdoAssignment);

			addObject(od, objd, index);

			++currentSMoffsetValue;
		});

		function addBooleanPadding(mappingOjbItems, paddingCount) {
			mappingOjbItems.push({ name: `Padding ${paddingCount}`, dtype: DTYPE.UNSIGNED32, value: `0x0000000${booleanPaddingBitsize}` });
		}
	}

	function addPdoMapping(objd, pdoName) {
		// make sure there is space
		if (!objd.pdo_mappings) {
			objd.pdo_mappings = [];
		}
		// mark object as PDO mapped, if it is not already
		if(!isInArray(objd.pdo_mappings, pdoName)) {
			objd.pdo_mappings.push(pdoName);
		}
	}
	
	function ensurePDOAssignmentExists(od, index) {	
		var pdoAssignments = od[index];
		if (!pdoAssignments) {
			pdoAssignments = { otype: OTYPE.ARRAY, dtype: DTYPE.UNSIGNED16, name: `Sync Manager ${index[3]} PDO Assignment`, items: [
				{ name: 'Max SubIndex' },
			]};
			od[index] = pdoAssignments;
		}
		return pdoAssignments;
	}
	
	function getPdoMappingValue(index, subindex, dtype) {
		function toByte(value) {
			var result = value.toString(16).slice(0, 2);
			while (result.length < 2) {
				result = `0${result}`;
			}
			return result;
		}
		var bitsize = esiDTbitsize(dtype);
		
		return `0x${index}${toByte(subindex)}${toByte(bitsize)}`;
	}	
}
/** populates mandatory objects with values from UI */
function populateMandatoryObjectValues(form, od) {
	if (form) {
		od['1008'].data = form.TextDeviceName.value;
		od['1009'].data = form.HWversion.value;
		od['100A'].data = form.SWversion.value;
		od['1018'].items[1].value = parseInt(form.VendorID.value);
		od['1018'].items[2].value = parseInt(form.ProductCode.value);
		od['1018'].items[3].value = parseInt(form.RevisionNumber.value);
		od['1018'].items[4].value = parseInt(form.SerialNumber.value);
	}
}
/** builds complete object dictionary, with values from UI */
function buildObjectDictionary(form) {
	const od = getMandatoryObjects();
	populateMandatoryObjectValues(form, od);
	// populate custom objects
	addSDOitems(od);
	addTXPDOitems(od);
	addRXPDOitems(od);
	_booleanPaddingCount = 0;

	return od;
}

// ####################### Object Dictionary index manipulation ####################### //

function indexToString(index) {
	var indexValue = parseInt(index);
	return indexValue.toString(16).toUpperCase();
}
/** returns list of indexes that are used in given OD, as array of integer values */
function getUsedIndexes(od) {
	const index_min = 0x1000;
	const index_max = 0xFFFF;
	const usedIndexes = [];
	// scan index address space for ones used  
	for (let i = index_min; i <= index_max; i++) {
		const index = indexToString(i);
		const element = od[index];
		if (element) {
			usedIndexes.push(index);
		}
	}
	return usedIndexes;
}

// ####################### Object Dictionary edition ####################### //

function getFirstFreeIndex(odSectionName) {
	var addressRangeStart = {
		"sdo": 0x2000,
		"txpdo": 0x6000,
		"rxpdo": 0x7000,
	}
	var result = addressRangeStart[odSectionName];
	var odSection = getObjDictSection(odSectionName);
	while (odSection[indexToString(result)]) {
		result++;
	}

	return indexToString(result);
}
/** returns new object description for given PDO section  */
function getNewObjd(odSectionName, otype) {
	const readableNames = {
		VAR: 'Variable',
		ARRAY: 'Array',
		RECORD: 'Record'
	}
	const objd = { 
		otype: otype,
		name: `New ${readableNames[otype]}`,
		access: 'RO',
	};
	switch(otype) {
	case OTYPE.ARRAY: {
		objd.items = [
			{ name: 'Max SubIndex' },
		];
		addArraySubitem(objd);
		break;
	}
	case OTYPE.RECORD: {
		objd.items = [
			{ name: 'Max SubIndex' },
		];
		addRecordSubitem(objd);
		break;
	}}
	if (odSectionName == txpdo || odSectionName == rxpdo) {
		objd.pdo_mappings = [ odSectionName ];
	}
	return objd;
}

function addArraySubitem(objd) {
	if (objd.otype != OTYPE.ARRAY) { alert(`${objd} is not ARRAY, cannot add subitem`); return; }
	if (!objd.items) { alert(`${objd} does not have items list, cannot add subitem`); return; }
	const newSubitem = { name: 'New array subitem' }
	objd.items.push(newSubitem);

	return newSubitem;
}

function addRecordSubitem(objd) {
	if (objd.otype != OTYPE.RECORD) { alert(`${objd} is not RECORD, cannot add subitem`); return; }
	if (!objd.items) { alert(`${objd} does not have items list, cannot add subitem`); return; }

	const default_subitemDT = DTYPE.UNSIGNED8; // first from list
	const newSubitem = { name: 'New record subitem', dtype: default_subitemDT }
	objd.items.push(newSubitem);

	return newSubitem;
}
