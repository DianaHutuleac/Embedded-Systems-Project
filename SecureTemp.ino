#include <Wire.h> // Include the Wire library for I2C communication

// MCP23017 I2C address
#define MCP23017_ADDR 0x20 // Define the I2C address for the MCP23017 chip

// MCP23017 registers
#define IODIRA 0x00 // Register address for setting the direction of GPIOA
#define IODIRB 0x01 // Register address for setting the direction of GPIOB
#define GPIOA 0x12 // Register address for the data of GPIOA
#define GPIOB 0x13 // Register address for the data of GPIOB
#define OLATA 0x14 // Register address for the output latches of GPIOA
#define OLATB 0x15 // Register address for the output latches of GPIOB

// LCD commands
#define LCD_CLEARDISPLAY 0x01 // Command to clear the LCD display
#define LCD_RETURNHOME 0x02 // Command to return the cursor to the home position
#define LCD_ENTRYMODESET 0x04 // Command to set the entry mode
#define LCD_DISPLAYCONTROL 0x08 // Command to control the display
#define LCD_FUNCTIONSET 0x20 // Command to set the function
#define LCD_SETDDRAMADDR 0x80 // Command to set the DDRAM address

// LCD flags for function set
#define LCD_4BITMODE 0x00 // Flag to set 4-bit mode
#define LCD_2LINE 0x08 // Flag to set 2-line display
#define LCD_5x8DOTS 0x00 // Flag to set 5x8 character font

// LCD flags for display on/off control
#define LCD_DISPLAYON 0x04 // Flag to turn on the display
#define LCD_DISPLAYOFF 0x00 // Flag to turn off the display
#define LCD_CURSOROFF 0x00 // Flag to turn off the cursor
#define LCD_BLINKOFF 0x00 // Flag to turn off cursor blinking

#define LCD_WIDTH 16  // Define the width of the LCD (16 characters)

// Keypad configuration
const byte ROWS = 4; // Define the number of rows in the keypad
const byte COLS = 4; // Define the number of columns in the keypad

// Define the characters on the keypad
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Define the row and column pins of the keypad
byte rowPins[ROWS] = {9, 8, 7, 6}; // Pins connected to the rows of the keypad
byte colPins[COLS] = {5, 4, 3, 2}; // Pins connected to the columns of the keypad

char preRecordedPassword[5] = "1234"; // Define the default passcode
const int ledPin = 13; // Define the pin for the LED
bool systemLocked = true; // Variable to track if the system is locked
bool scrolling = false; // Variable to track if a message is scrolling

char enteredPassword[5]; // Buffer to store the entered password
int index = 0; // Index for the entered password

#define NO_KEY '\0' // Define NO_KEY as a null character to represent no key press

int incorrectAttempts = 0; // Counter for incorrect password attempts
const int maxAttempts = 3; // Maximum allowed incorrect attempts
unsigned long lockoutTime = 30000; // Lockout duration in milliseconds (30 seconds)
unsigned long lockoutStartTime = 0; // Variable to store the start time of the lockout period
bool isLockout = false; // Variable to track if the system is in lockout mode

#define DHTPIN 11  // Define the digital pin connected to the DHT22 sensor

void setup() {
  pinMode(ledPin, OUTPUT); // Set the LED pin as an output
  Serial.begin(9600); // Initialize serial communication at 9600 baud rate
  Wire.begin(); // Initialize I2C communication

  // Initialize MCP23017 I/O expander
  writeRegister(MCP23017_ADDR, IODIRA, 0x00); // Set all pins of GPIOA as outputs
  writeRegister(MCP23017_ADDR, IODIRB, 0x00); // Set all pins of GPIOB as outputs

  // Initialize the LCD
  delay(50); // Wait for the LCD to power up
  
  // Follow the recommended initialization sequence for the LCD
  lcdCommand(0x03); // Set the LCD to 8-bit mode
  delayMicroseconds(4500); // Wait for more than 4.1ms
  lcdCommand(0x03); // Set the LCD to 8-bit mode again
  delayMicroseconds(4500); // Wait for more than 4.1ms
  lcdCommand(0x03); // Set the LCD to 8-bit mode again
  delayMicroseconds(150); // Wait for more than 100µs
  lcdCommand(0x02); // Set the LCD to 4-bit mode

  // Set the function of the LCD: 4-bit mode, 2 lines, 5x8 font
  lcdCommand(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);

  // Set the display control: display on, cursor off, blink off
  lcdCommand(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);

  // Clear the LCD display
  lcdCommand(LCD_CLEARDISPLAY);
  delayMicroseconds(2000); // Wait for the command to execute

  // Set the entry mode: increment automatically, no display shift
  lcdCommand(LCD_ENTRYMODESET | 0x02);

  // Initialize the DHT22 sensor
  pinMode(DHTPIN, INPUT); // Set the DHT22 pin as an input
}

void loop() {
  if (isLockout) { // Check if the system is in lockout mode
    unsigned long elapsedTime = millis() - lockoutStartTime; // Calculate the elapsed time since lockout started
    if (elapsedTime >= lockoutTime) { // Check if the lockout time has passed
      isLockout = false; // Reset lockout status
      incorrectAttempts = 0; // Reset incorrect attempts counter
      lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
      lcdPrint("Enter passcode:"); // Prompt the user to enter the passcode
    } else {
      lcdSetCursor(0, 1); // Set the cursor to the second line
      lcdPrint("Retry in: "); // Display retry message
      lcdPrint(String((lockoutTime - elapsedTime) / 1000).c_str()); // Display the remaining lockout time in seconds
      lcdPrint(" sec "); // Display "sec" to indicate seconds
    }
    return; // Exit the loop
  }

  if (systemLocked) { // Check if the system is locked
    static bool scrollingMessage = true; // Static variable to track if the scrolling message is active
    static bool changePasswordMode = false; // Static variable to track if the change password mode is active
    static bool newPasswordMode = false; // Static variable to track if the new password mode is active
    static bool oldPasswordMode = false; // Static variable to track if the old password mode is active

    if (scrollingMessage) { // Check if the scrolling message is active
      scrollText("  Enter passcode or press C to change it"); // Scroll the prompt message on the LCD
      scrollingMessage = false; // Deactivate the scrolling message
    }

    char customKey = getKey(); // Get the pressed key from the keypad
    if (customKey != NO_KEY) { // Check if a key was pressed
      scrollingMessage = false; // Deactivate the scrolling message
      if (customKey == 'C') { // Check if the 'C' key was pressed to change the password
        changePasswordMode = true; // Activate change password mode
        lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
        lcdPrint("Old passcode:"); // Prompt the user to enter the old passcode
      } else {
        if (changePasswordMode) { // Check if change password mode is active
          changePasswordMode = false; // Deactivate change password mode
          oldPasswordMode = true; // Activate old password mode
          lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
          lcdPrint("Old passcode"); // Prompt the user to enter the old passcode
        } else if (oldPasswordMode) { // Check if old password mode is active
          if (index < 4) { // Check if the entered password length is less than 4
            enteredPassword[index++] = customKey; // Add the pressed key to the entered password
            lcdSetCursor(index - 1, 1); // Set the cursor to the next position
            lcdPrint("*"); // Display an asterisk for each entered character
          }

          if (index == 4) { // Check if the entered password length is 4
            enteredPassword[index] = '\0'; // Null-terminate the entered password
            delay(500); // Wait for 500 milliseconds
            if (strcmp(enteredPassword, preRecordedPassword) == 0) { // Compare the entered password with the stored password
              oldPasswordMode = false; // Deactivate old password mode
              newPasswordMode = true; // Activate new password mode
              lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
              lcdPrint("New passcode"); // Prompt the user to enter the new passcode
              index = 0; // Reset the index for the new password
            } else { // If the entered password is incorrect
              incorrectAttempts++; // Increment the incorrect attempts counter
              lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
              scrollText("Wrong passcode"); // Scroll the wrong passcode message
              delay(2000); // Wait for 2 seconds
              lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
              lcdPrint("Old passcode:"); // Prompt the user to enter the old passcode again
              index = 0; // Reset the index for the next attempt
              checkLockout(); // Check if the system should enter lockout mode
            }
          }
        } else if (newPasswordMode) { // Check if new password mode is active
          if (index < 4) { // Check if the entered password length is less than 4
            enteredPassword[index++] = customKey; // Add the pressed key to the entered password
            lcdSetCursor(index - 1, 1); // Set the cursor to the next position
            lcdPrint("*"); // Display an asterisk for each entered character
          }

          if (index == 4) { // Check if the entered password length is 4
            enteredPassword[index] = '\0'; // Null-terminate the entered password
            delay(500); // Wait for 500 milliseconds
            // Update the stored password with the new one
            strcpy(preRecordedPassword, enteredPassword); // Copy the new password to the stored password
            newPasswordMode = false; // Deactivate new password mode
            systemLocked = false; // Unlock the system
            lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
            lcdPrint("Passcode changed"); // Display passcode changed message
            digitalWrite(ledPin, HIGH); // Turn on the LED
            systemLocked = false; // Ensure the system is unlocked
            delay(2000); // Wait for 2 seconds
            lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
            lcdPrint("See temperature"); // Prompt the user to see the temperature
            delay(2000); // Wait for 2 seconds

            lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
            lcdPrint("See temperature"); // Prompt the user to see the temperature
            index = 0; // Reset the index for the next use
          }
        } else {
          // Normal mode for checking the passcode
          if (index < 4) { // Check if the entered password length is less than 4
            enteredPassword[index++] = customKey; // Add the pressed key to the entered password
            lcdSetCursor(index - 1, 1); // Set the cursor to the next position
            lcdPrint("*"); // Display an asterisk for each entered character
          }

          if (index == 4) { // Check if the entered password length is 4
            enteredPassword[index] = '\0'; // Null-terminate the entered password
            delay(500); // Wait for 500 milliseconds
            if (strcmp(enteredPassword, preRecordedPassword) == 0) { // Compare the entered password with the stored password
              lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
              lcdPrint("Passcode correct"); // Display passcode correct message
              digitalWrite(ledPin, HIGH); // Turn on the LED
              systemLocked = false; // Unlock the system
              delay(2000); // Wait for 2 seconds
              lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
              lcdPrint("See temperature"); // Prompt the user to see the temperature
              delay(2000); // Wait for 2 seconds
              incorrectAttempts = 0; // Reset the incorrect attempts counter
            } else { // If the entered password is incorrect
              incorrectAttempts++; // Increment the incorrect attempts counter
              lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
              scrollText("Wrong passcode"); // Scroll the wrong passcode message
              delay(2000); // Wait for 2 seconds
              lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
              lcdPrint("Enter passcode:"); // Prompt the user to enter the passcode again
              checkLockout(); // Check if the system should enter lockout mode
            }
            index = 0; // Reset the index for the next attempt
          }
        }
      }
    }
  } else {
    // If the system is unlocked, read data from the DHT22 sensor
    float humidity, temperature; // Variables to store the temperature and humidity values
    if (readDHT22(temperature, humidity)) { // Read data from the DHT22 sensor
      Serial.print("Humidity: "); // Print humidity label to serial monitor
      Serial.print(humidity); // Print humidity value to serial monitor
      Serial.print("%  Temperature: "); // Print temperature label to serial monitor
      Serial.print(temperature); // Print temperature value to serial monitor
      Serial.println("C"); // Print unit of temperature to serial monitor

      // Display temperature and humidity on the LCD
      lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
      lcdSetCursor(0, 0); // Set the cursor to the first line
      lcdPrint("Temp: "); // Display temperature label
      lcdPrint(String(temperature).c_str()); // Display temperature value
      lcdPrint("C"); // Display unit of temperature

      lcdSetCursor(0, 1); // Set the cursor to the second line
      lcdPrint("Humidity: "); // Display humidity label
      lcdPrint(String(humidity).c_str()); // Display humidity value
      lcdPrint("%"); // Display unit of humidity

      delay(2000);  // Wait for 2 seconds before the next reading
    } else {
      Serial.println("Failed to read from DHT22 sensor!"); // Print error message to serial monitor if reading fails
    }
  }
}

void lcdCommand(uint8_t command) {
  // Send a command to the LCD
  write4bits((command & 0xF0) >> 4, false); // Send the higher nibble
  write4bits(command & 0x0F, false); // Send the lower nibble
  delayMicroseconds(2000); // Wait for the command to settle
}

void lcdPrint(const char *str) {
  // Print a string to the LCD
  while (*str) { // Loop through each character in the string
    writeChar(*str++); // Write each character to the LCD
  }
}

void writeChar(uint8_t value) {
  // Write a single character to the LCD
  write4bits((value & 0xF0) >> 4, true); // Send the higher nibble
  write4bits(value & 0x0F, true); // Send the lower nibble
}

void write4bits(uint8_t value, bool isData) {
  // Write 4 bits to the LCD
  uint8_t out = 0; // Initialize the output variable
  
  // Map the bits to the corresponding GPB pins
  if (value & 0x01) out |= (1 << 4); // D4 -> GPB4
  if (value & 0x02) out |= (1 << 3); // D5 -> GPB3
  if (value & 0x04) out |= (1 << 2); // D6 -> GPB2
  if (value & 0x08) out |= (1 << 1); // D7 -> GPB1

  if (isData) {
    out |= 0x80; // RS -> GPB7 (1 for data)
  } else {
    out &= ~0x80; // RS -> GPB7 (0 for command)
  }

  // RW is always 0 (write mode)
  out &= ~0x40; // RW -> GPB6 (0 for write)

  // Send data to LCD
  writeRegister(MCP23017_ADDR, GPIOB, out | 0x20); // EN high -> GPB5
  delayMicroseconds(1); // Ensure the EN pin is high for a short period
  writeRegister(MCP23017_ADDR, GPIOB, out & ~0x20); // EN low -> GPB5
  delayMicroseconds(100); // Commands need > 37us to settle
}

void writeRegister(uint8_t addr, uint8_t reg, uint8_t value) {
  // Write a value to a register on the MCP23017
  Wire.beginTransmission(addr); // Begin transmission to the I2C address
  Wire.write(reg); // Write the register address
  Wire.write(value); // Write the value to the register
  Wire.endTransmission(); // End transmission
}

void scrollText(const char* message) {
  // Scroll a message across the LCD
  int len = strlen(message); // Get the length of the message
  if (len <= 16) { // If the message length is less than or equal to 16 characters
    lcdSetCursor(0, 0); // Set the cursor to the first line
    lcdPrint(message); // Print the message
    delay(2000); // Wait for 2 seconds
    return; // Exit the function
  }

  int position = 0; // Initialize the position variable
  while (position <= len - 16) { // Loop through the message to scroll it
    lcdSetCursor(0, 0); // Set the cursor to the first line
    lcdPrint(message + position); // Print a part of the message starting from the current position
    delay(500); // Adjust scrolling speed as needed
    position++; // Move to the next position
  }
}

char getKey() {
  // Get the key pressed on the keypad
  for (byte c = 0; c < COLS; c++) { // Loop through each column
    pinMode(colPins[c], OUTPUT); // Set the current column pin as output
    digitalWrite(colPins[c], LOW); // Set the current column pin to LOW
    for (byte r = 0; r < ROWS; r++) { // Loop through each row
      pinMode(rowPins[r], INPUT_PULLUP); // Set the current row pin as input with pull-up
      if (digitalRead(rowPins[r]) == LOW) { // Check if the key is pressed
        while (digitalRead(rowPins[r]) == LOW); // Wait for the key to be released
        pinMode(colPins[c], INPUT); // Set the current column pin as input
        pinMode(rowPins[r], INPUT); // Set the current row pin as input
        return hexaKeys[r][c]; // Return the key value
      }
      pinMode(rowPins[r], INPUT); // Set the current row pin as input
    }
    pinMode(colPins[c], INPUT); // Set the current column pin as input
  }
  return NO_KEY; // Return NO_KEY if no key is pressed
}

void lcdSetCursor(uint8_t col, uint8_t row) {
  // Set the cursor position on the LCD
  const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 }; // Define the row offsets
  lcdCommand(LCD_SETDDRAMADDR | (col + row_offsets[row])); // Set the DDRAM address with the row and column offset
}

void checkLockout() {
  // Check if the system should enter lockout mode
  if (incorrectAttempts >= maxAttempts) { // Check if the incorrect attempts exceed the maximum allowed attempts
    isLockout = true; // Set the lockout status to true
    lockoutStartTime = millis(); // Record the lockout start time
    lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
    scrollText("Too many attempts"); // Scroll the too many attempts message
    delay(2000); // Wait for 2 seconds
    lcdCommand(LCD_CLEARDISPLAY); // Clear the LCD display
  }
}

// DHT22 Functions
void startSignal() {
  // Send start signal to the DHT22 sensor
  pinMode(DHTPIN, OUTPUT); // Set the DHT22 pin as output
  digitalWrite(DHTPIN, LOW); // Pull the pin low for 18 milliseconds
  delay(18);  
  digitalWrite(DHTPIN, HIGH); // Pull the pin high for 40 microseconds
  delayMicroseconds(40);  
  pinMode(DHTPIN, INPUT); // Set the DHT22 pin as input
}

bool readResponse() {
  // Read the response from DHT22 sensor
  unsigned int loopCnt = 10000;
  while(digitalRead(DHTPIN) == HIGH) { // Wait for the pin to go LOW
    if (loopCnt-- == 0) return false; // Timeout after 10000 cycles
  }

  loopCnt = 10000;
  while(digitalRead(DHTPIN) == LOW) { // Wait for the pin to go HIGH
    if (loopCnt-- == 0) return false; // Timeout after 10000 cycles
  }

  loopCnt = 10000;
  while(digitalRead(DHTPIN) == HIGH) { // Wait for the pin to go LOW
    if (loopCnt-- == 0) return false; // Timeout after 10000 cycles
  }
  return true; // Return true if response is received
}

int readByte() {
  // Read a byte of data from the DHT22 sensor
  int byte = 0;
  for (int i = 0; i < 8; i++) {
    unsigned int loopCnt = 10000;
    while(digitalRead(DHTPIN) == LOW) { // Wait for the pin to go HIGH
      if (loopCnt-- == 0) return -1; // Timeout after 10000 cycles
    }

    unsigned long length = micros(); // Record the time the pin went HIGH
    
    loopCnt = 10000;
    while(digitalRead(DHTPIN) == HIGH) { // Wait for the pin to go LOW
      if (loopCnt-- == 0) return -1; // Timeout after 10000 cycles
    }
    length = micros() - length; // Calculate the duration the pin was HIGH
    
    if (length > 40) { // If the duration was greater than 40 microseconds
      byte |= (1 << (7 - i));  // Set the corresponding bit in the byte
    }
  }
  return byte; // Return the byte read from the sensor
}

bool readDHT22(float& temperature, float& humidity) {
  // Read temperature and humidity data from the DHT22 sensor
  uint8_t data[5] = {0, 0, 0, 0, 0}; // Array to store the 40 bits of data from the sensor

  startSignal(); // Send start signal to the DHT22 sensor
  if (!readResponse()) { // Wait for and check the response from the sensor
    return false; // If the response is not received, return false
  }

  // Read 5 bytes (40 bits) of data from the sensor
  for (int i = 0; i < 5; i++) {
    int byte = readByte(); // Read each byte
    if (byte == -1) { // If reading a byte fails, return false
      return false;
    }
    data[i] = byte; // Store the byte in the data array
  }

  // Verify the checksum to ensure data integrity
  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    return false; // If checksum doesn't match, return false
  }

  // Convert the data
  humidity = ((data[0] << 8) + data[1]) * 0.1; // Combine the first two bytes for humidity and scale
  temperature = (((data[2] & 0x7F) << 8) + data[3]) * 0.1; // Combine the next two bytes for temperature and scale
  if (data[2] & 0x80) { // Check if the temperature is negative
    temperature = -temperature; // If negative, convert to negative value
  }

  return true; // If everything is successful, return true
}
