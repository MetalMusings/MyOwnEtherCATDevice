// Write a valid eeprom for LAN9252.
//
#include <Wire.h>
#include <I2C_eeprom.h>
// Define LAN9252 or AX58100
// Output below from binary eeprom:  xxd --include eeprom_data.bin
// #define LAN9252
// #define AX58100
#define LAN9252
//#define PROGRAM_EEPROM
//  Don't define PROGRAM_EEPROM if you only want to read out contents
#ifdef LAN9252
#define ESC_STRING "LAN9252"
const uint8_t eeprom_data[] = {
  0x04, 0x0f, 0x00, 0x44, 0x10, 0x27, 0xf0, 0xff, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xd4, 0x00, 0xd8, 0x04, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x01, 0x00, 0x0a, 0x00, 0x1c, 0x00,
  0x02, 0x0d, 0x4c, 0x41, 0x4e, 0x39, 0x32, 0x35, 0x32, 0x2d, 0x44, 0x49,
  0x47, 0x49, 0x4f, 0x28, 0x39, 0x32, 0x35, 0x32, 0x20, 0x38, 0x20, 0x43,
  0x68, 0x2e, 0x20, 0x44, 0x69, 0x67, 0x2e, 0x20, 0x49, 0x6e, 0x2d, 0x2f,
  0x4f, 0x75, 0x74, 0x70, 0x75, 0x74, 0x20, 0x32, 0x78, 0x4d, 0x49, 0x49,
  0x20, 0x28, 0x4e, 0x6f, 0x20, 0x44, 0x43, 0x29, 0x1e, 0x00, 0x10, 0x00,
  0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x01, 0x00,
  0x01, 0x02, 0x29, 0x00, 0x08, 0x00, 0x01, 0x0f, 0x00, 0x00, 0x44, 0x00,
  0x01, 0x03, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0xff, 0xff
};

#endif
#ifdef AX58100
#define ESC_STRING "Ax58100"
//  xxd --include eeprom_data.bin
unsigned char eeprom_data[] = {
    0x04, 0x0d, 0x00, 0x44, 0x10, 0x27, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xc1, 0x00, 0x95, 0x0b, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x01, 0x00, 0x0a, 0x00, 0x1b, 0x00,
    0x02, 0x0b, 0x41, 0x58, 0x35, 0x38, 0x31, 0x30, 0x30, 0x5f, 0x45, 0x56,
    0x42, 0x28, 0x41, 0x58, 0x35, 0x38, 0x31, 0x30, 0x30, 0x20, 0x45, 0x56,
    0x42, 0x20, 0x28, 0x44, 0x69, 0x67, 0x69, 0x74, 0x61, 0x6c, 0x20, 0x31,
    0x36, 0x2d, 0x49, 0x6e, 0x70, 0x75, 0x74, 0x2f, 0x31, 0x36, 0x2d, 0x4f,
    0x75, 0x74, 0x70, 0x75, 0x74, 0x29, 0x1e, 0x00, 0x10, 0x00, 0x01, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x01, 0x00, 0x01, 0x02,
    0x29, 0x00, 0x0c, 0x00, 0x02, 0x0f, 0x00, 0x00, 0x44, 0x00, 0x01, 0x03,
    0x03, 0x0f, 0x00, 0x00, 0x44, 0x00, 0x01, 0x03, 0x00, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x04, 0xff, 0xff};

#endif

// 32 kbit bytes
#define EEPROM_MAXBYTES 32 * 1024 / 8

// the address of your EEPROM
#define DEVICEADDRESS (0x50)

I2C_eeprom eeprom(DEVICEADDRESS, EEPROM_MAXBYTES);
void dumpEEPROM(uint16_t memoryAddress, uint16_t length);
void printHex(byte b);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println();
  Serial.println("IT IS BEGINNING");
  Serial.println("WAIT FOR IT");

  Wire.begin();
  eeprom.begin();
#ifdef PROGRAM_EEPROM
  Serial.println("----------------------------------------------");
  Serial.print("Writing ");
  Serial.print(sizeof(eeprom_data));
  Serial.print(" long data for ");
  Serial.println(ESC_STRING);
  uint8_t ok;
  ok = eeprom.writeBlockVerify(0, eeprom_data, sizeof(eeprom_data));
  if (ok)
  {
    Serial.println("Success writeBlockVerify");
    Serial.println("\nThat's it. It worked\n");
  }
  else
  {
    Serial.println("Failed writeBlockVerify");
  }

  Serial.println("----------------------------------------------");
#endif
  dumpEEPROM(0, sizeof(eeprom_data));
}

void loop()
{
  // Nothing to do during loop
}

void dumpEEPROM(uint16_t memoryAddress, uint16_t length)
{
  const int BLOCK_TO_LENGTH = 12;

  Serial.print("       ");
  for (int x = 0; x < 10; x++)
  {
    if (x != 0)
      Serial.print("    ");
    Serial.print(x, HEX);
  }
  Serial.println("\n--------------------------------------------------------");

  // block to defined length
  memoryAddress = memoryAddress / BLOCK_TO_LENGTH * BLOCK_TO_LENGTH;
  // length = (length + BLOCK_TO_LENGTH - 1) / BLOCK_TO_LENGTH * BLOCK_TO_LENGTH;

  byte b = eeprom.readByte(memoryAddress);
  for (unsigned int i = 0; i < length; i++)
  {
    char buf[16];
    if (memoryAddress % BLOCK_TO_LENGTH == 0)
    {
      if (i != 0)
        Serial.println();
      if (memoryAddress == 0)
        Serial.print("   ");
      else if (memoryAddress < 100)
        Serial.print("  ");
              else if (memoryAddress < 1000)
        Serial.print(" ");

      Serial.print(memoryAddress);
      Serial.print(":  ");
    }
    printHex(b);
    b = eeprom.readByte(++memoryAddress);
  }
  Serial.println("\n-----------------------------------");
}

void printHex(byte b)
{
  char buf[20];
  sprintf(buf, "0x%02x ", b);
  Serial.print(buf);
}