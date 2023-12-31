# [🔁 EEPROM generator](https://kubabuda.github.io/EEPROM_generator)

This is basic code generator tool for EtherCAT devices, using [SOES library](https://github.com/OpenEtherCATsociety/SOES).

[It is available online, here](https://kubabuda.github.io/EEPROM_generator)

You can configure:
- ESC (Ethercat Slave Chip) 
- OD (CANopen Object Dictionary) entries
- PDO mappings (which OD objects are mapped in TX, RX datagrams)

Tool generates consistent data across C sources, ESI file and EEPROM content.

It also backs up your current project in localstorage. You can save project to JSON file on your hard drive, restore from it later, and download all files at once.

## Limitations

- Only single, non-dynamic PDO is supported for TX and RX respectively
- Some data types might not be supported
- Browsers other than Firefox might not be supported

If you need more, [RT-Labs](https://rt-labs.com/ethercat/) offers professional IDE - EtherCAT SDK, and training.

# Development

Pull requests welcome.

Source code is intentionally keept in plain Javascript files so that build system like webpack or even web server is not needed.
The only dependency is web browser, that should future proof it.

## [Unit tests](https://kubabuda.github.io/EEPROM_generator/tests.html)

[Unit tests](https://kubabuda.github.io/EEPROM_generator/tests.html) are using [Jasmine](https://jasmine.github.io). 

## OD structure

OD is keept as JSON object. Expected data format:

```js
{
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
}   
```

OD model for generator has 4 sections:

- `sdo`, not mapped to PDOs
- `txpdo`, mapped to TXPDO (SM3). Expected format (for OTYPE VAR):
```js
{
    '6000': { otype: OTYPE.VAR, dtype: DTYPE.UNSIGNED32, name: 'TXPDO', value: 0x1389, pdo_mappings: ['tx'] },
}
```
- `rxpdo`, same as above, but `pdo_mappings: ['rx']`
- mandatory objects. These are added at code gen stage, with values populated form UI controls.

Code generation copies all values into single OD, adds PDO mappings and SM assignments. 

## PDO mappings

Currently single, non-dynamic PDO is supported for TX and RX respectively.

## Binary file comparison tool for Windows: [VBinDiff](https://www.cjmweb.net/vbindiff/VBinDiff-Win32)

```cmd
VBinDiff et1100.bin  lan9252.bin
```


# TODO

- add support for 64bit number types
- why default dark mode has dark text in Chromium but not Firefox, when OS is in dark mode
- generate .h file for EEPROM emulation
- add support for LAN9253 Direct, XMC4300, XMC4800
- check output for boolean ARRAY
- check output for VISIBLE_STRING ARRAY
- check output for VISIBLE_STRING RECORD subitem
- dynamic/multiple PDOs
- more unit tests
- SM2 offset: regardles of value set, SDK generates RXPDO mappings as SDO1600. SM2 offset change affects
    - `ecat_options.h` #define SM2_sma, MAX_TXPDO_SIZE and MAX_RXPDO_SIZE
    - `esi.xml` `<Sm ControlByte="#x24" Enable="1" StartAddress="#x1600">Outputs</Sm>`
    currently this tool mirrors SDK behavior, check if this hack is needed or just duplicated SDK bug
- calculate `MAX_RXPDO_SIZE`, `MAX_TXPDO_SIZE` based on selected SM2 offset
- modular device profile
- set project name from device name
- DC config in ESI .bin

- use [XML parsing](https://www.w3schools.com/xml/xml_parser.asp) for cleaner builiding ESI XML output
- read XML files: make reverse eningeering 3rd party devices easier
- add indexes list to OD model, to decrease parametes count in methods
- 0x1001 UINT8 Error Register is mandatory according to CiA DS301
- 0x1C12/0x1C13 `PdoAssign` and 0x1608/0x1A08 `PdoMapping` [should be RW](https://infosys.beckhoff.com/english.php?content=../content/1033/el6695/1317558667.html&id=)

### Code optimization

- reuse repeated string constants in objlist.c names, test if does any good or compiler does that for you anyway

# Disclaimer

The EtherCAT Technology, the trade name and logo "EtherCAT" are the intellectual
property of, and protected by Beckhoff Automation GmbH.
