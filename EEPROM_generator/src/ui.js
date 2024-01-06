/**
 * SOES EEPROM generator
 * UI behavior logic

* This tool serves as:
- EtherCAT Slave Information XML + EEPROM binary generator
- SOES code generator
 
 * Victor Sluiter 2013-2018
 * Kuba Buda 2020-2021
 */
'use strict'

// ####################### UI changes handlers ####################### //

function getForm() {
	return document.getElementById("SlaveForm");
}

function getOutputForm() {
	return document.getElementById("outCodeForm");
}

function onFormChanged() {
	const form = getForm();
	saveLocalBackup(form);
	processForm(form);
}

/** Shortcuts:
 * Ctrl + S to save project
 * Ctrl + O to load save file
 * Shortcuts start to work after user clicked on page
 */
document.onkeydown = function(e) {
	const S_keyCode = 83;
	const O_keyCode = 79;
	if (e.ctrlKey){
		if (e.keyCode === S_keyCode) {
			event.preventDefault();
			onSaveClick();
			return false;
		}
		else if (e.keyCode == O_keyCode) {
			event.preventDefault();
			onRestoreClick();
			return false;
		}
	}
};

// When the user clicks anywhere outside of the modal, close it
window.onclick = function(event) {
	if (event.target == odModal) {
		odModalClose();
	}
}

window.onload = (event) => {
	odModalSetup();
	syncModalSetup();
	const form = getForm();
	setFormValues(form, getFormDefaultValues());
	tryRestoreLocalBackup(form);
	// for convinience during tool development, trigger codegen on page refresh
	processForm(form);
	
	const _isComputerFast = automaticCodegen;
	
	if (_isComputerFast) {
		// code is regenerated on every form change. 
		// no need to remember to generate before copying or downloading
		// app is noticeably slower

		processForm(form); // make sure displayed code is up to date at startup, e.g redo, if it came from backup
	
		document.getElementById('GenerateFilesButton').style.display = 'none'; // 'generate' button no longer needed
		form.addEventListener('change', function() {
			onFormChanged();
		});
	}
	setupDarkMode();
}


// ####################### dark mode logic ####################### //
function setupDarkMode() {
	if (!localStorage.darkMode) {
		localStorage.darkMode = 'dark'; // dark mode by default
	}
	document.documentElement.setAttribute("data-theme", localStorage.darkMode);
}

function toggleDarkMode() {
	var newMode = (localStorage.darkMode == 'dark') ? "light" : "dark"
	localStorage.darkMode = newMode;
	document.documentElement.setAttribute("data-theme", localStorage.darkMode);
}

// ####################### code generation  UI logic  ####################### //

/** Code generation method, triggered by UI */
function processForm(form)
{
	const od = buildObjectDictionary(form);
	const indexes = getUsedIndexes(od);
	var outputCtl = getOutputForm();
	
	outputCtl.objectlist.value = objectlist_generator(form, od, indexes);
	outputCtl.ecat_options.value = ecat_options_generator(form, od, indexes);
	outputCtl.utypes.value = utypes_generator(form, od, indexes);
	outputCtl.HEX.hexData = hex_generator(form);
	outputCtl.HEX.value = toIntelHex(outputCtl.HEX.hexData);
	outputCtl.ESI.value = esi_generator(form, od, indexes, _dc);
	
	saveLocalBackup(form);

	return outputCtl;
}

// ####################### Button handlers ####################### //
function getProjectName(form) {
	return variableName(form.TextDeviceName.value);
}

function onGenerateDownloadClick()
{
	const form = getForm();
	var result = processForm(form);
	downloadGeneratedFilesZipped(form, result);

	function downloadGeneratedFilesZipped(form, result) {
		var zip = new JSZip();
		const projectName = getProjectName(form);
		zip.file(`${projectName}.xml`, result.ESI.value);
		zip.file('eeprom.hex', result.HEX.value);
		zip.file('eeprom.bin', result.HEX.hexData);
		zip.file('ecat_options.h', result.ecat_options.value);
		zip.file('objectlist.c', result.objectlist.value);
		zip.file('utypes.h', result.utypes.value);
		zip.file('esi.json', prepareBackupFileContent(form));

		zip.generateAsync({type:"blob"}).then(function (blob) { // generate the zip file
			downloadFile(blob, "esi.zip", "application/zip"); // trigger the download
		}, function (err) {
			console.log(err);
		});
	}

	function downloadGeneratedFiles(form, result) {
		const projectName = getProjectName(form);
		downloadFile(result.ESI.value, `${projectName}.xml`, 'text/html');
		downloadFile(result.HEX.value, 'eeprom.hex', 'application/octet-stream');
		downloadFile(result.ecat_options.value, 'ecat_options.h', 'text/plain');
		downloadFile(result.objectlist.value, 'objectlist.c', 'text/plain');
		downloadFile(result.utypes.value, 'utypes.h', 'text/plain');
		downloadBackupFile(form);
	
	}
}

function onGenerateClick() {
	processForm(getForm());
}

function onSaveClick() {
	const form = getForm();
	downloadBackupFile(form);
	saveLocalBackup(form);
}

function onRestoreClick() {
	// trigger file input dialog window
	document.getElementById('restoreFileInput').click();
}

function onRestoreComplete(fileContent) {
	const form = getForm();
	restoreBackup(fileContent, form);
	processForm(form);
}

function onResetClick() {
	if (confirm("Are you sure you want to reset project to default values?")){
		resetLocalBackup();
		location.reload(true);
	}
}

function onDownloadEsiXmlClick() {
	const form = getForm();
	const projectName = getProjectName(form);
	downloadFile(getOutputForm().ESI.value, `${projectName}.xml`, 'text/html');
}

function onDownloadBinClick() {
	const record = getOutputForm().HEX.hexData;
	if (!record) { alert("Generate code before you can download it"); return; }
	downloadFile(record, 'eeprom.bin', 'application/octet-stream');
}

// ####################### Handle modal dialog ####################### //

var odModal = {};

function odModalSetup() {
	// Get the modal
	odModal = document.getElementById("editObjectModal");
	if (odModal) {
		odModal.form = document.getElementById('EditObjectForm');
	}
	else {
		alert("Element required to edit Object Dictionary not found!");
	}
}

// When the user clicks the button, open the modal 
function odModalOpen() {
	odModal.style.display = "block";
}

function odModalClose() {
	odModal.style.display = "none";
}

/** update control values on OD modal */
function odModalUpdate(index, objd) {
	odModal.form.Index.value = `0x${index}`;
	odModal.form.ObjectName.value = objd.name;
	odModal.form.DTYPE.value = objd.dtype || DTYPE.UNSIGNED8;
	odModal.form.Access.value = objd.access || 'RO';
	odModal.objd = objd;
}

function odModalHideControls() {
	document.getElementById('dialogRowIndex').style.display = 'none';
	document.getElementById('dialogRowDtype').style.display = 'none';
	document.getElementById('dialogRowValue').style.display = 'none';
	document.getElementById('dialogRowAccess').style.display = 'none';
}

// ####################### Modal dialogs for OD edition ####################### //

function editExistingOD_ObjectDialog(odSectionName, index, otype) {
	const od = getObjDictSection(odSectionName);
	var objd = od[index]; 
	odModal.index_initial_value = index;
	checkObjectType(otype, objd);
	odModalUpdate(index, objd);
}

function addNewOD_ObjectDialog(odSectionName, otype) {
	var objd = getNewObjd(odSectionName, otype);
	var index = getFirstFreeIndex(odSectionName);
	delete odModal.index_initial_value; // add new object, not replace edited one 
	odModalUpdate(index, objd);
}

function odModalOpenForObject(otype) {
	odModalHideControls();
	document.getElementById('dialogRowIndex').style.display = '';
	document.getElementById('dialogRowAccess').style.display = '';

	switch (otype) {
		case OTYPE.VAR: {
			document.getElementById('dialogRowDtype').style.display = '';
			document.getElementById('dialogRowValue').style.display = '';
			break;
		}
		case OTYPE.ARRAY: {
			document.getElementById('dialogRowDtype').style.display = "";
			break;		
		}
		case OTYPE.RECORD: {
			break;
		}
		default: { 
			alert(`Unknown object type ${otype}, cannot open modal for it!`);
			return;
		}
	}
	odModalOpen();
	document.getElementById('modalInputIndex').focus();
}

function odModalSetTitle(message) {
	document.getElementById('editObjectTitle').innerHTML = `<strong>${message}</strong>`;
}

// ####################### Edit Object Dictionary UI logic ####################### //

function editVAR_Click(odSectionName, indexValue = null) {
	const otype = OTYPE.VAR;
	const index = indexToString(indexValue);
	var actionName = "Edit";
	odModal.odSectionName = odSectionName;

	if (objectExists(odSectionName, index)) {
		editExistingOD_ObjectDialog(odSectionName, index, otype);
		odModal.form.DTYPE.value = odModal.objd.dtype;
	} else {
		addNewOD_ObjectDialog(odSectionName, otype);
		actionName = "Add"
	}
	odModalSetTitle(`${actionName} ${odSectionName.toUpperCase()} variable`);
	odModalOpenForObject(otype);
}

function editARRAY_Click(odSectionName, indexValue = null) {
	const otype = OTYPE.ARRAY;
	const index = indexToString(indexValue);
	var actionName = "Edit";
	odModal.odSectionName = odSectionName;
	odModal.form.Access

	if (objectExists(odSectionName, index)) {
		editExistingOD_ObjectDialog(odSectionName, index, otype);
		odModal.form.DTYPE.value = odModal.objd.dtype;
	} else {
		addNewOD_ObjectDialog(odSectionName, otype);
		actionName = "Add"
	}
	odModalSetTitle(`${actionName} ${odSectionName.toUpperCase()} array`);
	odModalOpenForObject(otype);
}

function editRECORD_Click(odSectionName, indexValue = null) {
	const otype = OTYPE.RECORD;
	const index = indexToString(indexValue);
	var actionName = "Edit";
	odModal.odSectionName = odSectionName;

	if (objectExists(odSectionName, index)) {
		editExistingOD_ObjectDialog(odSectionName, index, otype);
	} else {
		addNewOD_ObjectDialog(odSectionName, otype);
		actionName = "Add"
	}
	odModalSetTitle(`${actionName} ${odSectionName.toUpperCase()} record`);
	odModalOpenForObject(otype);
}

function onEditObjectSubmit(modalform) {
	if (odModal.subitem) {
		onEditSubitemSubmit(odModal.subitem);
		delete odModal.subitem;
		return;
	}
	const objd = odModal.objd;
	const objectType = objd.otype;
	const index = indexToString(modalform.Index.value);

	objd.name = modalform.ObjectName.value;

	switch (objectType) {
		case OTYPE.VAR:
			objd.dtype = modalform.DTYPE.value;
			
			if (objd.dtype == DTYPE.VISIBLE_STRING) {
				objd.data = '' 
			} else {
				objd.value = modalform.InitalValue.value;
			}
			break;
		case OTYPE.ARRAY:
			objd.dtype = modalform.DTYPE.value;
			
			break;
		case OTYPE.RECORD:
		
			break;
		default:
			alert(`Unexpected type ${objectType} on object ${modalform.ObjectName} being edited!`);
			break;
	}
	const odSection = getObjDictSection(odModal.odSectionName);
	if (odModal.index_initial_value) {
		removeObject(odSection, odModal.index_initial_value); // detach from OD, to avoid duplicate if index changed
	}
	addObject(odSection, objd, index);	// attach updated object
	odModalClose();
	reloadOD_Section(odModal.odSectionName);
	delete odModal.odSectionName;
	odModal.objd = {};
	
	onFormChanged();
}

function onRemoveClick(odSectionName, indexValue, subindex = null) {
	const index = indexToString(indexValue);
	const odSection = getObjDictSection(odSectionName);
	const objd = odSection[index];
	if(!objd) { alert(`${odSectionName.toUpperCase()} object ${index} does not exist!`); return; }

	if(subindex) {
		if(!objd.items) { alert(`Object 0x${index} "${objd.name}" does not have any items!`); return; }
		if(objd.items.length < subindex) { alert(`Object 0x${index} "${objd.name}" does not have enough items!`); return; }
		if(objd.items.length < 3) { // only max subindex and one more subitem
			alert(`Object 0x${index} "${objd.name}" has only 1 subitem, it should not be empty. Remove entire object instead.`); return; }
	}

	if (confirm(getConfirmMessage(objd, index, subindex))) {
		
		if (subindex) {
			const subitemsToRemove = 1;			
			objd.items.splice(subindex, subitemsToRemove); 
		} else {
			removeObject(odSection, index);
		}
		reloadOD_Section(odSectionName);
		onFormChanged();
	}

	function getConfirmMessage(objd, index, subindex) {
		if (subindex) {
			return `Are you sure you want to remove subitem ${subindex} "${objd.items[subindex].name }" from object 0x${index} "${objd.name}"?`
		}
		return `Are you sure you want to remove object 0x${index} "${objd.name}"?`
	}
}

function addSubitemClick(odSectionName, indexValue) {
	const index = indexToString(indexValue);
	const odSection = getObjDictSection(odSectionName);
	const objd = odSection[index]; 

	// we expect objd to have items array with at least [{ name: 'Max SubIndex' }]
	if(!objd.items || !objd.items.length ) { alert(`Object ${index} "${objd.name}" has no subitems!`); return; }

	switch(objd.otype) {
		case OTYPE.ARRAY: {
			addArraySubitem(objd);
			break;
		}
		case OTYPE.RECORD: {
			addRecordSubitem(objd);
			break;
		}
		default: {
			alert(`Object ${index} "${objd.name}" has OTYPE ${objd.otype} so cannot add subitems`);
		}
	}
	const subindex = objd.items.length - 1; // subitem is added to end of items list
	editSubitemClick(odSectionName, indexValue, subindex, "Add");
}

function editSubitemClick(odSectionName, indexValue, subindex, actionName = "Edit") {
	const index = indexToString(indexValue);
	const odSection = getObjDictSection(odSectionName);
	const objd = odSection[index];

	if(!objd.items || objd.items.length <= subindex ) { alert(`Object ${index} "${objd.name}" does not have ${subindex} subitems!`); return; }
	
	odModalSetTitle(`${actionName} ${odSectionName.toUpperCase()} object 0x${index} "${objd.name}" subitem 0x${indexToString(subindex)}`);
	
	const subitem = objd.items[subindex];
	
	odModalHideControls();
	
	document.getElementById('dialogRowValue').style.display = "";
	odModal.form.InitalValue.value = subitem.value ?? 0;
	
	if (objd.otype == OTYPE.RECORD) {
		document.getElementById('dialogRowDtype').style.display = "";
		odModal.form.DTYPE.value = subitem.dtype;
		document.getElementById('dialogRowAccess').style.display = ''; // access for record subitems can differ
		odModal.form.Access.value = subitem.access || 'RO';
	}
	odModal.form.ObjectName.value = subitem.name;
	odModal.subitem = { odSectionName: odSectionName, index: index, subindex: subindex };
	odModalOpen();
	document.getElementById('modalInputObjectName').focus();
}

function onEditSubitemSubmit(modalSubitem) {
	const odSection = getObjDictSection(modalSubitem.odSectionName);
	const objd = odSection[modalSubitem.index];
	const subitem = objd.items[modalSubitem.subindex];

	subitem.name =  odModal.form.ObjectName.value;
	subitem.value = odModal.form.InitalValue.value;
	if (objd.otype == OTYPE.RECORD) {
		subitem.dtype = odModal.form.DTYPE.value;
		subitem.access = odModal.form.Access.value;
	}
	odModalClose();
	onFormChanged();
	reloadOD_Section(modalSubitem.odSectionName);
}

// ####################### Display Object Dictionary state ####################### //

function reloadOD_Sections() {
	reloadOD_Section(sdo);
	reloadOD_Section(txpdo);
	reloadOD_Section(rxpdo);
}

function reloadOD_Section(odSectionName) {
	const odSection = getObjDictSection(odSectionName);
	var indexes = getUsedIndexes(odSection);
	var section = '';
	indexes.forEach(index => {
		const objd = odSection[index];
		section += `<div class="odItem"><span class="odItemContent"><strong>0x${index}</strong> &nbsp; &nbsp; "${objd.name}" ${objd.otype} ${objd.dtype ?? ''}</span><span>`;
		if (objd.otype == OTYPE.ARRAY || objd.otype == OTYPE.RECORD) {
			section += `<button onClick='addSubitemClick(${odSectionName}, 0x${index})'>&nbsp; ➕ Add subitem &nbsp;</button>`;
		}
		section += `<button onClick='onRemoveClick(${odSectionName}, 0x${index})'>&nbsp; ❌ Remove &nbsp;</button>`;
		section += `<button onClick='edit${objd.otype}_Click(${odSectionName}, 0x${index})'>&nbsp; 🛠️ &nbsp; Edit &nbsp;</button>`;
		section += `</span></div>`;
		if (objd.items) {
			var subindex = 1; // skip Max Subindex
			objd.items.slice(subindex).forEach(subitem => {
				var subindexHex = subindex < 16 ? `0${subindex.toString(16)}` : subindex.toString(16);
				section += `<div class="odSubitem"><span class="odSubitemContent"><strong>:0x${subindexHex}</strong>&nbsp;&nbsp; "${subitem.name}" ${subitem.dtype ?? ''}</span>`;
				section += `<span><button onClick='onRemoveClick(${odSectionName}, 0x${index}, ${subindex})'>&nbsp; ❌ Remove &nbsp;</button>`;
				section += `<button onClick='editSubitemClick(${odSectionName}, 0x${index}, ${subindex})'>&nbsp; 🛠️ &nbsp; Edit &nbsp;</button>`;
				section += `</span></div>`;
				++subindex;
			});
		}
	});
	const odSectionElement = document.getElementById(`tr_${odSectionName}`); 
	if (odSectionElement) {
		odSectionElement.innerHTML = section;
	} 
}

// ####################### Synchronization settings UI ####################### //

var syncModal = {};
var _dc = []

function syncModalSetup() {
	// Get the modal
	syncModal = document.getElementById("syncModal");
	if (syncModal) {
		syncModal.form = document.getElementById('syncModalForm');
	}
	else {
		alert("Element required to edit Object Dictionary not found!");
	}
}

function syncModalClose() {
	syncModal.style.display = "none";
	delete syncModal.edited;
	reloadSyncModes();
}

function syncModalOpen() {
	syncModal.style.display = "block";
}

function syncModeEdit(sync) {
	syncModal.edited = sync;

	syncModal.form.Name.value = sync.Name;
	syncModal.form.Description.value = sync.Description;
	syncModal.form.AssignActivate.value = sync.AssignActivate;
	syncModal.form.Sync0cycleTime.value = sync.Sync0cycleTime;
	syncModal.form.Sync0shiftTime.value = sync.Sync0shiftTime;
	syncModal.form.Sync1cycleTime.value = sync.Sync1cycleTime;
	syncModal.form.Sync1shiftTime.value = sync.Sync1shiftTime;

	syncModalOpen();
}

function onSyncSubmit(syncForm) {

	syncModal.edited.Name = syncForm.Name.value;
	syncModal.edited.Description = syncForm.Description.value;
	syncModal.edited.AssignActivate = syncForm.AssignActivate.value;
	syncModal.edited.Sync0cycleTime = syncForm.Sync0cycleTime.value;
	syncModal.edited.Sync0shiftTime = syncForm.Sync0shiftTime.value;
	syncModal.edited.Sync1cycleTime = syncForm.Sync1cycleTime.value;
	syncModal.edited.Sync1shiftTime = syncForm.Sync1shiftTime.value;

	syncModalClose();
	onFormChanged();
}

// ####################### Synchronization settings button handlers ####################### //

function addSyncClick() {
	const newSyncMode = {
		Name: "DcOff",
		Description: "DC unused",
		AssignActivate: "#x000",
		Sync0cycleTime: 0,
		Sync0shiftTime: 0,
		Sync1cycleTime: 0,
		Sync1shiftTime: 0,
	}
	_dc.push(newSyncMode);
	syncModeEdit(newSyncMode);
}

function onEditSyncClick(i) {
	const syncMode = _dc[i];
	syncModeEdit(syncMode);
}

function onRemoveSyncClick(i) {
	_dc.splice(i, 1);
	reloadSyncModes();
	onFormChanged();
}

// ####################### Synchronization settings UI ####################### //

function reloadSyncModes() {
	var section = '';
	var i = 0;
	_dc.forEach(sync => {
		section += `<div class="odItem"><span class="odItemContent"><strong>${sync.Name}</strong> &nbsp; &nbsp; ${sync.Description} &nbsp; [${sync.AssignActivate}]</span><span>`;
		section += `<button onClick='onRemoveSyncClick(${i})'>&nbsp; ❌ Remove &nbsp;</button>`;
		section += `<button onClick='onEditSyncClick(${i})'>&nbsp; 🛠️ &nbsp; Edit &nbsp;</button>`;
		section += `</span></div>`;
		++i;
	});
	const sectionElement = document.getElementById(`dcSyncModes`);
	if (sectionElement) {
		sectionElement.innerHTML = section;
	}
}