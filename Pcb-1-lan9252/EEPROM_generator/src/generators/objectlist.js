/**
 * SOES EEPROM generator
 * objectlist.c code generation logic

* This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator
 
 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

// ####################### Objectlist.c generating ####################### //

function get_objdBitsize(element) {
	let bitsize = dtype_bitsize[element.dtype];
	if (element.dtype == DTYPE.VISIBLE_STRING) {
		bitsize = bitsize * element.data.length;
	}
	return bitsize;
}


/** Takes object dictionary item from sdo, rxpdo or txpdo section
 * Adds objd.data that is displayed in objectlist.c
 * and links to OD variable declared on OD struct in utypes.h
 */
function objectlist_link_utypes(objd) {
	switch (objd.otype) {
		case  OTYPE.VAR: {
			objd.data = objectlist_VAR_data(objd);
			break;
		} 
		case OTYPE.ARRAY: {
			var subindex = 1;
			objd.items.slice(subindex).forEach(subitem => {
				subitem.data = objectlist_ARRAY_data(objd, subindex);
				++subindex;
			});
			break;
		}
		case OTYPE.RECORD: {
			var subindex = 1;
			objd.items.slice(subindex).forEach(subitem => {
				subitem.data = objectlist_RECORD_data(objd, subitem);
				++subindex;
			});
			break;
		}
	}
	
	function objectlist_VAR_data(objd) {
		return `&Obj.${variableName(objd.name)}`;
	}
	
	function objectlist_ARRAY_data(objd, subindex) {
		return `&Obj.${variableName(objd.name)}[${subindex - 1}]`;
	}
	
	function objectlist_RECORD_data(objd, subitem) {
		return `&Obj.${variableName(objd.name)}.${variableName(subitem.name)}`;
	}
}

function objectlist_generator(form, od, indexes)
{
	var objectlist  = '#include "esc_coe.h"\n#include "utypes.h"\n#include <stddef.h>\n\n';

	//Variable names
	indexes.forEach(index => {
		const objd = od[index];
		objectlist += objectlist_variableName(index, objd);
		
	});
	objectlist += '\n';
	//SDO objects declaration
	indexes.forEach(index => {
		const objd = od[index];
		objectlist += objectlist_SdoObjectDeclaration(index, objd);
	})

	objectlist += '\n\nconst _objectlist SDOobjects[] =\n{';
	//SDO object dictionary declaration
	indexes.forEach(index => {
		const objd = od[index];
		objectlist += objectlist_DictionaryDeclaration(index, objd);
	})
	objectlist += '\n  {0xffff, 0xff, 0xff, 0xff, NULL, NULL}\n};\n';

	return objectlist;

	function objectlist_variableName(index, objd) {
		var objectlist = `\nstatic const char acName${index}[] = "${objd.name}";`;
		switch (objd.otype) {
			case OTYPE.VAR:
				break;
			case OTYPE.ARRAY:
			case OTYPE.RECORD: 
				for (let subindex = 0; subindex < objd.items.length; subindex++) {
					const item = objd.items[subindex];
					objectlist += `\nstatic const char acName${index}_${subindex_padded(subindex)}[] = "${item.name}";`;
				}
				break;
			default:
				alert("Unexpected object type in object dictionary: ", objd)
				break;
		};
		return objectlist;
	}

	function objectlist_SdoObjectDeclaration(index, objd) {
		var objectlist = `\nconst _objd SDO${index}[] =\n{`;

		switch (objd.otype) {
			case OTYPE.VAR: {
				const value = objectlist_getItemValue(objd, objd.dtype);
				objectlist += `\n  {0x0, DTYPE_${objd.dtype}, ${get_objdBitsize(objd)}, ${objectlist_objdFlags(objd)}, acName${index}, ${value}, ${objeclist_objdData(objd)}},`;
				break;
			}
			case OTYPE.ARRAY: {
				objectlist += `\n  {0x00, DTYPE_${DTYPE.UNSIGNED8}, ${8}, ATYPE_RO, acName${index}_00, ${objd.items.length - 1}, NULL},`; // max subindex
				const bitsize = dtype_bitsize[objd.dtype]; /* TODO what if it is array of strings? */
				let subindex = 1;
				objd.items.slice(subindex).forEach(subitem => {
					var subi = subindex_padded(subindex);
					const value = objectlist_getItemValue(subitem, objd.dtype);
					objectlist += `\n  {0x${subi}, DTYPE_${objd.dtype}, ${bitsize}, ${objectlist_objdFlags(objd)}, acName${index}_${subi}, ${value}, ${subitem.data || 'NULL'}},`;
					subindex++;
				});
				break;
			}
			case OTYPE.RECORD: {
				objectlist += `\n  {0x00, DTYPE_${DTYPE.UNSIGNED8}, ${8}, ATYPE_RO, acName${index}_00, ${objd.items.length - 1}, NULL},`; // max subindex
				let subindex = 1;
				objd.items.slice(subindex).forEach(subitem => {
					var subi = subindex_padded(subindex);
					const bitsize = dtype_bitsize[subitem.dtype];
					const value = objectlist_getItemValue(subitem, subitem.dtype);
					const atypeflag = objectlist_objdFlags(subitem);
					objectlist += `\n  {0x${subi}, DTYPE_${subitem.dtype}, ${bitsize}, ${atypeflag}, acName${index}_${subi}, ${value}, ${subitem.data || 'NULL'}},`;
					subindex++;
				});

				break;
			}
			default:
				alert("Unexpected object type om object dictionary");
				break;
		};
		objectlist += '\n};';

		return objectlist;
	}

	function objectlist_DictionaryDeclaration(index, objd) {
		var objectlist = ``;
		switch (objd.otype) {
			case OTYPE.VAR:
			case OTYPE.ARRAY:
			case OTYPE.RECORD:
				let maxsubindex = 0;
				if (objd.items) {
					maxsubindex = objd.items.length - 1;
				}
				objectlist += `\n  {0x${index}, OTYPE_${objd.otype}, ${maxsubindex}, ${objd.pad1 || 0}, acName${index}, SDO${index}},`;
				break;
			default:
				alert("Unexpected object type om object dictionary")
				break;
		};
		return objectlist;
	}

	function float32ToHex(float32) {
	// made by: Jozo132 (https://github.com/Jozo132)
		const getHex = i => ('00' + i.toString(16)).slice(-2);
		var view = new DataView(new ArrayBuffer(4))
		view.setFloat32(0, float32);
		return Array.apply(null, { length: 4 }).map((_, i) => getHex(view.getUint8(i))).join('');
	}

	function objectlist_getItemValue(item, dtype) {
		let value = '0';
		if (item.value) {
			value = `${item.value}`;
			if (dtype == DTYPE.REAL32) {
				return `0x${float32ToHex(value)}`;
			}
		}
		return value;
	}

	function subindex_padded(subindex) {
		// pad with 0 if single digit
		if (subindex > 9) {
			return `${subindex}`;
		}
		return `0${subindex}`;
	}
	
	/** Gets flags for objectlist item: 
	 * 
	 * ATYPE: access type (RO/RW/WO/RWpre) 
	 * 
	 * PDO mappings */
	function objectlist_objdFlags(element) {
		let flags = "ATYPE_RO"; // RO by default
		if (element.access) {
			flags = `ATYPE_${element.access}`; 
		} 
		
		if (element.pdo_mappings) {
			element.pdo_mappings .forEach(mapping => {
				flags = `${flags} | ATYPE_${mapping.toUpperCase()}`;
			});
		}
		return flags;
	}
	
	function objeclist_objdData(element) {
		let el_data = 'NULL';
	
		if (element.data) {
			el_data = element.data;
			if (element.dtype == DTYPE.VISIBLE_STRING) {
				el_data = `"${element.data}"`;
			}
		}
		/* TODO el_data is assigned also for PDO mapped variables */
		return el_data;
	}	
}
