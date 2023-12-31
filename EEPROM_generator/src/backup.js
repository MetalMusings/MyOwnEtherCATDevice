/**
 * SOES EEPROM generator
 * Project backup save and restore

 * This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator

 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

// ####################### Backup serialization + deserialization ####################### //

function isValidBackup(backup) {
	if (!backup || !backup.form || !backup.od ) {
		if (!confirm('Backup is incomplete or invalid, proceed anyway?')) {
			return false;
		}
	}
	return true;
}

function prepareBackupObject(form) {
	const formValues = {};
	if (form) {
		Object.entries(form).forEach(formEntry => {
			const formControl = formEntry[1]; // entry[0] is form control order number
			if(isBackedUp(formControl) && formControl.value) {
				formValues[formControl.name] = formControl.value;
			};
		});
	}

	const backup = {
		form: formValues,
		od: getObjDict(),
		dc: _dc,
	};

	return backup;
}

function isBackedUp(formControl) {
	return formControl.type != "button";
}

function loadBackup(backupObject, form) {
	if (backupObject.od) {
		setObjDictSection(sdo, backupObject.od.sdo);
		setObjDictSection(txpdo, backupObject.od.txpdo);
		setObjDictSection(rxpdo, backupObject.od.rxpdo);
	}

	if (backupObject.dc) {
		_dc = backupObject.dc;
	}
	
	setFormValues(form, backupObject);
}

function setFormValues(form, backupObject) {
	if (form) {
		Object.entries(form).forEach(formEntry => {
			const formControl = formEntry[1]; // entry[0] is index
			const formControlValue = backupObject.form[formControl.name];
			if (isBackedUp(formControl) && formControlValue) {
				setFormControlValue(formControl, formControlValue);
			};
		});
	}
}

function setFormControlValue(formControl, formControlValue) {
	if (formControl.name.startsWith('CoeDetailsEnable')) {
		if (formControlValue == true) {
			formControl.checked = true;
		} else {
		}
	} else {
		formControl.value = formControlValue;
	}
}

function prepareBackupFileContent(form) {
	var backupObject = prepareBackupObject(form);
	var backupFileContent = JSON.stringify(backupObject, null, 2); // pretty print
	return backupFileContent;
}

// ####################### Backup using JSON file from filesystem ####################### //

// Localstorage limit is usually 5MB, super large object dictionaries on older browsers might be problematic

function downloadBackupFile(form) {
	const backupFileContent = prepareBackupFileContent(form); // pretty print
	downloadFile(backupFileContent, 'esi.json', 'text/json');
}

function restoreBackup(fileContent, form) {
	var backup = JSON.parse(fileContent);
	if (isValidBackup(backup)) {
		loadBackup(backup, form);
		reloadOD_Sections();
		reloadSyncModes()
	}
}

// ####################### Backup using browser localstorage ####################### //

/** persist OD and settings changes over page reload */
function saveLocalBackup(form) {
	localStorage.etherCATeepromGeneratorBackup = prepareBackupFileContent(form);
}

function tryRestoreLocalBackup(form) {
	if (localStorage.etherCATeepromGeneratorBackup)  {
		restoreBackup(localStorage.etherCATeepromGeneratorBackup, form);
	}	
}

function resetLocalBackup() {
	if (localStorage.etherCATeepromGeneratorBackup) {
		delete localStorage.etherCATeepromGeneratorBackup;
	}
}