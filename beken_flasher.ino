#include <SPI.h>
#include <avr/wdt.h>

//XTX chip

#define SPI_CHIP_ERASE_CMD      0xc7
#define SPI_WRITE_ENABLE_CMD    0x06
#define SPI_WRITE_DISABLE_CMD   0x04

#define SPI_READ_PAGE_CMD       0x03
#define SPI_WRITE_PAGE_CMD      0x02
#define SPI_ERASE_PAGE_CMD      0x20

#define SPI_SECUR_SECTOR_ERASE  0x44

#define SPI_ID_READ_CMD         0x9F
#define SPI_MAN_READ_CMD        0x90

#define SPI_STATU_WR_LOW_CMD    0x05
#define SPI_STATU_WR_HIG_CMD    0x35

#define SPI_STATU_WR_WRITE_CMD  0x01


const uint8_t CHIP_ID[] = {0x00, 0x0B, 0x40, 0x15}; 

#define AUTO_START_SPI_MODE 1
#define D2_BYTE_COUNT 250

#define READ_BLOCK_SIZE 256
#define WRITE_BLOCK_SIZE 256

// Pin definitions for Arduino Uno
#define SS_PIN 10    // Slave Select (CS) pin
#define MOSI_PIN 11  // Master Out Slave In
#define MISO_PIN 12  // Master In Slave Out
#define SCK_PIN 13   // Serial Clock

#define CEN_PIN 9    // Slave Select (CS) pin

// CLI
#define MAX_CMD_LENGTH 64
#define MAX_ARGS 8

// Global variables
char cmdBuffer[MAX_CMD_LENGTH];
int cmdIndex = 0;


uint8_t parseArguments(char *cmd, char *argv[], int maxArgs) {
  int argc = 0;
  char *token = strtok(cmd, " \t");
  while (token != NULL && argc < maxArgs) {
    argv[argc++] = token;
    token = strtok(NULL, " \t");
  }
  // If there are still tokens left, we exceeded the limit.
  if (token != NULL) {
    return -1; // Too many arguments.
  }
  return argc;
}

uint32_t parseAddres(char *arg){

  uint32_t value;

  if(arg[0] == '0' && arg[1] == 'x'){
    sscanf(arg, "%lx", &value);
    return value;
  }
  
  if(arg[0] == '0' && arg[1] == 'X'){
    sscanf(arg, "%lX", &value);
    return value;    
  }
  
  sscanf(arg, "%lu", &value);
  return value;

}

void setup() {
  // Initialize serial communication for debugging
  //Serial.begin(115200);
  //Serial.begin(500000);
  Serial.begin(1000000);

  //Serial.begin(9600);

  pinMode(CEN_PIN, OUTPUT);
  digitalWrite(CEN_PIN, LOW);

  while (!Serial) {
    ; // Wait for serial port to connect (needed for some boards)
  }

  printBanner();  

  spi_init();

  printPrompt();
}

void spi_init(){

  // SPI pins
  pinMode(SS_PIN, OUTPUT);
  pinMode(MOSI_PIN, OUTPUT);
  pinMode(SCK_PIN, OUTPUT);
  pinMode(MISO_PIN, INPUT);
  
  // Initialize SS pin high (deselect slave)
  digitalWrite(SS_PIN, HIGH);
  digitalWrite(SCK_PIN, LOW);  // Clock idle low for SPI Mode 0

  if(AUTO_START_SPI_MODE){
    set_spi_mode();

    for(int i = 0; i < 5; i += 1){
      read_chip_id();
      delay(100);
    }
  }
}

void spi_deinit(){
  pinMode(SS_PIN, INPUT);
  pinMode(MOSI_PIN, INPUT);
  pinMode(SCK_PIN, INPUT);
  pinMode(MISO_PIN, INPUT);
}


void loop() {
  // Check for incoming serial data
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();
    
    // Handle backspace/delete
    if (incomingChar == 8 || incomingChar == 127) {
      if (cmdIndex > 0) {
        cmdIndex--;
        cmdBuffer[cmdIndex] = 0;
        Serial.print(F("\b \b")); // Backspace effect
      }
      return;
    }
    
    // Handle carriage return/newline
    if (incomingChar == '\n' || incomingChar == '\r') {
      if (cmdIndex > 0) {
        cmdBuffer[cmdIndex] = 0; // Null terminate
        processCommand();
        cmdIndex = 0;
        memset(cmdBuffer, 0, sizeof(cmdBuffer));
        //
      }else{
        printPrompt();
      }
      return;
    }

    if(incomingChar == '\x03'){
      cmdIndex = 0;
      memset(cmdBuffer, 0, sizeof(cmdBuffer));
      printPrompt();
      return;
    }
    
    // Add character to buffer if there's space
    if (cmdIndex < MAX_CMD_LENGTH - 1) {
      cmdBuffer[cmdIndex++] = incomingChar;
      Serial.print(incomingChar);
    }
  }
  
  // Small delay to prevent overwhelming the serial buffer
  Serial.flush();
  //delay(1);
}

void printBanner(){
  Serial.println();
  Serial.println();
  Serial.println(F("╔════════════════════════════════════════╗"));
  Serial.println(F("║           Beken SPI flasher            ║"));
  Serial.println(F("╚════════════════════════════════════════╝"));
  Serial.println();
  Serial.println();
}

void printPrompt(){
  Serial.print(F("\n\r> "));
  Serial.flush();
}

void processCommand() {
  Serial.println(); // New line after command
  
  char cmdCopy[MAX_CMD_LENGTH];
  
  strncpy(cmdCopy, cmdBuffer, MAX_CMD_LENGTH);
  
  // Tokenize command into argv
  char *argv[4];
  int argc = parseArguments(cmdCopy, argv, 4);
  if (argc == -1) {
    Serial.println(F("Error: too many arguments (max 3)"));
    Serial.print(F("\n> "));
    return;
  }
  
  // Dispatch based on argv[0]
  if (argc == 0) {
    // Empty command - do nothing
  } 
    else if (strcmp(argv[0], "init") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: init"));
    } else {
      spi_init();
    }
  }
  else if (strcmp(argv[0], "deinit") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: deinit"));
    } else {
      spi_deinit();
    }
  }
  else if (strcmp(argv[0], "chip-reset") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: chip-reset"));
    } else {
      chip_reset();
    }
  }
    else if (strcmp(argv[0], "reboot") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: reboot"));
    } else {
      reboot();
    }
  } else if (strcmp(argv[0], "read-chip-id") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: read-chip-id"));
    } else {
      read_chip_id();
    }
  } else if (strcmp(argv[0], "read-manufacturer-id") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: manufacturer-chip-id"));
    } else {
      read_manufacturer_id();
    }
  } else if (strcmp(argv[0], "set-spi-mode") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: set-spi-mode"));
    } else {
      set_spi_mode();
    }
  } else if (strcmp(argv[0], "write-enable") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: write-enable"));
    } else {
      write_enable();
    }
  } else if (strcmp(argv[0], "write-disable") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: write-disable"));
    } else {
      write_disable();
    }
  } else if (strcmp(argv[0], "status") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: status"));
    } else {
      status();
    }
  } else if (strcmp(argv[0], "read-flash-page") == 0) {
    if (argc != 2) {
      Serial.println(F("Read a whole page (256 bytes)\nUsage: read-flash <addr>"));
    } else {
      uint32_t addr = parseAddres(argv[1]);
      read_flash_page(addr, 0);
    }
  } else if (strcmp(argv[0], "read-flash-page-raw") == 0) {
    if (argc != 2) {
      Serial.println(F("Read a whole page (256 bytes)\nUsage: read-flash-page-raw <addr>"));
    } else {
      uint32_t addr = parseAddres(argv[1]);
      read_flash_page(addr, 1);
    }
  } else if (strcmp(argv[0], "write-flash-page") == 0) {
    if (argc != 2) {
      Serial.println(F("Write a whole page (256 bytes)\nUsage: write-flash <addr>"));
    } else {
      uint32_t addr = parseAddres(argv[1]);
      write_flash_page(addr, 0);
    }
  }
   else if (strcmp(argv[0], "write-flash-page-raw") == 0) {
    if (argc != 2) {
      Serial.println(F("Write a whole page (256 bytes)\nUsage: write-flash <addr>"));
    } else {
      uint32_t addr = parseAddres(argv[1]);
      write_flash_page(addr, 1);
    }
  }
  else if (strcmp(argv[0], "read-status-register") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: read-status-register"));
    } else {
      read_status_reg();
    }
  }
  else if (strcmp(argv[0], "write-status-register") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: write-status-register"));
    } else {
      write_status_reg();
    }
  }
  else {
    Serial.print(F("ERROR: Unknown command '"));
    Serial.print(cmdBuffer);
    Serial.println(F("'. Type 'help' for available commands."));
  }
  
  //printPrompt();
}

// Misc commands

void status() {
  Serial.print(F("Uptime: "));
  Serial.print(millis());
  Serial.println(F(" ms"));
  Serial.print(F("Free RAM: "));
  Serial.print(free_ram());
  Serial.println(F(" bytes"));
}

uint8_t free_ram() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void reboot() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

void print_hex_table(const uint8_t* buffer, size_t length, const char* label = nullptr) {
  if (label != nullptr && strlen(label) > 0) {
    Serial.print(label);
    Serial.println(F(":"));
  }
  
  if (length == 0) {
    Serial.println(F("  [Empty buffer]"));
    return;
  }
  
  size_t row = 0;
  size_t col = 0;
  
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = buffer[i];
    
    // Print row index at start of each new row
    if (col == 0) {
      Serial.print(F("  0x"));
      if (row * 32 < 0x10) Serial.print(F("0"));
      if (row * 32 < 0x100) Serial.print(F("0"));
      if (row * 32 < 0x1000) Serial.print(F("0"));
      Serial.print(row * 32, HEX);
      Serial.print(F("   "));
    }
    
    // Print byte in hex format (2 digits, uppercase, zero-padded)
    if (byte < 0x10) Serial.print(F("0"));
    Serial.print(byte, HEX);
    
    // Add spacing
    if (col % 8 == 7) {
      // End of 8-byte group - add 2 spaces if not end of row
      if (col % 32 != 31) {
        Serial.print(F("  "));
      }
    } else {
      // Regular spacing between bytes
      Serial.print(F(" "));
    }
    
    col++;
    
    // Handle row completion
    if (col == 32) {
      // End of row - print newline
      Serial.println();
      row++;
      col = 0;
    } else if (i == length - 1) {
      // End of buffer but not end of row - finish the line
      for (size_t remaining = col; remaining < 32; remaining++) {
        if (remaining % 8 == 0 && remaining % 32 != 0) {
          Serial.print(F("  ")); // Double space between groups
        } else {
          Serial.print(F("   ")); // Triple space to align (2 hex chars + space)
        }
      }
      Serial.println();
    }
  }
  
  // Print summary
  Serial.print(F("  Total bytes: "));
  Serial.print(length);
  Serial.print(F(" (0x"));
  if (length < 0x10) Serial.print(F("0"));
  Serial.print(length, HEX);
  Serial.println(")");
  Serial.println();
}

void print_hex_line(const uint8_t* buffer, size_t length, const char* label = nullptr) {
  if (label != nullptr && strlen(label) > 0) {
    Serial.print(label);
    Serial.print(F(": ["));
  } else {
    Serial.print(F("["));
  }
  
  for (size_t i = 0; i < length; i++) {
    if (i > 0) Serial.print(F(" "));
    Serial.print(F("0x"));
    if (buffer[i] < 0x10) Serial.print(F("0"));
    Serial.print(buffer[i], HEX);
  }
  
  Serial.println(F("]"));
}

// Power and mode commands

void chip_reset(){
  Serial.println(F("Triggering chip reset via CEN..."));
  pinMode(CEN_PIN, OUTPUT);
  digitalWrite(CEN_PIN, LOW);
  delay(500);
  digitalWrite(CEN_PIN, HIGH);
}

void set_spi_mode() {

  chip_reset();

  delay(50);

  Serial.println(F("Sending SPI mode request..."));
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  
  delayMicroseconds(100);

  uint8_t size = 255;
  uint8_t *txBuffer = malloc(sizeof(uint8_t)*size);
  memset(txBuffer, 0xD2, size);

  SPI.transfer(txBuffer, size);

  print_hex_table(txBuffer, 255, "Buffer");
    
  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction() ;

  free(txBuffer);
}

void read_chip_id() {
  Serial.println(F("Checking SPI flash ID..."));
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  
  delayMicroseconds(100);

  uint8_t size = 4;
  uint8_t *txBuffer = malloc(sizeof(uint8_t)*size);

  txBuffer[0] =  SPI_ID_READ_CMD;
  txBuffer[1] =  0x0;
  txBuffer[2] =  0x0;
  txBuffer[3] =  0x0;

  SPI.transfer(txBuffer, size); 
    
  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();

  /*if(memcmp(txBuffer, CHIP_ID, 4) == 0){
    Serial.println("ID OK!");
  }else{
    Serial.println("WRONG ID!");
  }*/

  print_hex_line(txBuffer, 4, "Chip ID");
  free(txBuffer);
}

void read_manufacturer_id() {
  Serial.println(F("Checking SPI flash manufacturer ID..."));
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  
  delayMicroseconds(100);

  uint8_t size = 4;
  uint8_t *txBuffer = malloc(sizeof(uint8_t)*size);

  txBuffer[0] =  SPI_MAN_READ_CMD;
  txBuffer[1] =  0x0;
  txBuffer[2] =  0x0;
  txBuffer[3] =  0x0;

  SPI.transfer(txBuffer, size); 
    
  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();

  print_hex_line(txBuffer, 4, "Chip ID");
  free(txBuffer);

}

// R/W commands

void wait_busy(){

  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  delayMicroseconds(100);

  uint8_t txBuffer[2];

  //Serial.print("\nSPI flash busy [");

  while(1){
    txBuffer[0] =  SPI_STATU_WR_LOW_CMD;
    txBuffer[1] =  0x0;
    SPI.transfer(txBuffer, 2);

   
    if(!(txBuffer[1] & 0x01)){
      //Serial.println("]");
      break;
    }else{
      //Serial.print(".");
      delay(1);
    }
  }

  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();

}

void write_enable(){
  //Serial.println(F("Setting write enable mode"));
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  delayMicroseconds(100);

  uint8_t txBuffer[1];
  txBuffer[0] = SPI_WRITE_ENABLE_CMD;
  SPI.transfer(txBuffer, 1);
  
  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();

  wait_busy();

}

void write_disable(){
  //Serial.println(F("Setting write disable mode"));
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  delayMicroseconds(100);

  uint8_t txBuffer[1];
  txBuffer[0] = SPI_WRITE_DISABLE_CMD;
  SPI.transfer(txBuffer, 1);

  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();
}

void read_status_reg() {
  //Serial.println(F("Reading status register"));
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  delayMicroseconds(100);

  // Read low byte
  uint8_t txBuffer[1];
  txBuffer[0] = SPI_STATU_WR_LOW_CMD;
  //txBuffer[1] = 0x00; 
  SPI.transfer(txBuffer, 1);
  //Serial.print(F("Low MSB: 0x"));
  print_hex_line(txBuffer, 1);

  // Read high byte
  txBuffer[0] = SPI_STATU_WR_HIG_CMD;
  //txBuffer[1] = 0x00; // dummy byte
  SPI.transfer(txBuffer, 2);
  //Serial.print(F("High MSB: 0x"));
  print_hex_line(txBuffer, 1);

  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();
}

void write_status_reg() {
  //Serial.println(F("Writing status register -> 0x0 0x0"));
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  delayMicroseconds(100);

  // Read low byte
  uint8_t txBuffer[3];
  txBuffer[0] = SPI_STATU_WR_WRITE_CMD;
  txBuffer[1] = SPI_STATU_WR_WRITE_CMD;
  txBuffer[2] = SPI_STATU_WR_WRITE_CMD;

  SPI.transfer(txBuffer, 3);

  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();
}

void read_flash_page(uint32_t addr, uint8_t mode) {
  if(mode == 0){
    Serial.print(F("Reading SPI flash at "));
    Serial.println(addr);
  }
  
  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);
  
  delayMicroseconds(50);

  uint32_t size = 4 + READ_BLOCK_SIZE;
  uint8_t *txBuffer = malloc(sizeof(uint8_t) * size);

  if(txBuffer != nullptr){

    memset(txBuffer, 0x0, size);

    txBuffer[0] =  SPI_READ_PAGE_CMD;
    txBuffer[1] =  (addr & 0xFF0000) >> 16;
    txBuffer[2] =  (addr & 0xFF00) >> 8;
    txBuffer[3] =  addr & 0xFF;

    SPI.transfer(txBuffer, size); 

    if(mode == 0){
      print_hex_table(txBuffer+(4*sizeof(uint8_t)), READ_BLOCK_SIZE, "Flash page");
    }else{
      Serial.write(txBuffer+(4*sizeof(uint8_t)), READ_BLOCK_SIZE);
    }

    Serial.flush();

    free(txBuffer);

  }else{
    Serial.println(F("Unable to reserve buffer"));
  }

  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();

}

void erase_flash_page(uint32_t addr){

  //Serial.print(F("Erasing SPI flash page at "));
  //Serial.println(addr, HEX);

  uint8_t txBuffer[4];

  // Erase

  write_enable();

  SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SS_PIN, LOW);

  delayMicroseconds(50);

  txBuffer[0] =  SPI_ERASE_PAGE_CMD;
  txBuffer[1] =  (addr & 0xFF0000) >> 16;
  txBuffer[2] =  (addr & 0xFF00) >> 8;
  txBuffer[3] =  addr & 0xFF;

  SPI.transfer(txBuffer, 4); 

  digitalWrite(SS_PIN, HIGH);
  SPI.endTransaction();

  wait_busy();

}

void write_flash_page(uint32_t addr, uint8_t mode) {
  //Serial.print(F("Writing SPI flash page at "));
  //Serial.println(addr, HEX);

  // Unlock
  write_enable();
  write_status_reg(); 
  
  uint32_t size = 4 + READ_BLOCK_SIZE;
  uint8_t *txBuffer = malloc(sizeof(uint8_t) * size);

  if(txBuffer != nullptr){

    if ((addr & 0xfff) == 0){
      erase_flash_page(addr);
    }

    // Write

    write_enable();

    SPI.beginTransaction(SPISettings(16000000, MSBFIRST, SPI_MODE0));
    digitalWrite(SS_PIN, LOW);

    delayMicroseconds(50);

    memset(txBuffer, 0x5A, size);

    txBuffer[0] =  SPI_WRITE_PAGE_CMD;
    txBuffer[1] =  (addr & 0xFF0000) >> 16;
    txBuffer[2] =  (addr & 0xFF00) >> 8;
    txBuffer[3] =  addr & 0xFF;

    uint32_t index = 4;

    Serial.println("READY");

    while(1){
      if (Serial.available() > 0) {
        txBuffer[index] = (uint8_t) Serial.read();
        index ++;
      }
      if(index == WRITE_BLOCK_SIZE + 4){
        break;
      }
    }

    SPI.transfer(txBuffer, size); 

    digitalWrite(SS_PIN, HIGH);
    SPI.endTransaction();

    free(txBuffer);

    wait_busy();

    // Read back

    //read_flash_page(addr, mode);

    Serial.println("OK");

    

  }else{
    Serial.println(F("Unable to reserve buffer"));
  }
  
  Serial.flush();
  //write_disable();
  
}

