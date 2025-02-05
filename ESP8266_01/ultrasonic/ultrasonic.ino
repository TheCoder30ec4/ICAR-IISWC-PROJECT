// Define UART pins for the sensor
#define RXD2 16 // GPIO pin for receiving data from the sensor (TX of sensor)
#define TXD2 17 // GPIO pin for transmitting data to the sensor (RX of sensor)

// Create a hardware serial instance for UART communication
HardwareSerial mySerial(2);

// Array to store incoming serial data
unsigned char data_buffer[4] = {0};

// Integer to store the measured distance in millimeters
int distance_mm = 0;

// Float to store the distance in centimeters
float distance_cm = 0.0;

// Variable to hold checksum
unsigned char CS;

void setup() {
  // Initialize the serial monitor for debugging
  Serial.begin(115200);
  
  // Initialize the hardware serial for the sensor (UART2)
  mySerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Print a startup message
  Serial.println("JSN-SR04T Ultrasonic Sensor - ESP32");
  Serial.println("Reading distance in cm...");
}

void loop() {
  // Check if data is available from the sensor
  if (mySerial.available() > 0) {

    delay(4); // Delay to allow full packet to arrive

    // Check for the packet header character 0xFF
    if (mySerial.read() == 0xFF) {
      // Insert header into the data buffer
      data_buffer[0] = 0xFF;

      // Read the remaining 3 bytes of the packet
      for (int i = 1; i < 4; i++) {
        data_buffer[i] = mySerial.read();
      }

      // Compute checksum
      CS = data_buffer[0] + data_buffer[1] + data_buffer[2];

      // Validate the checksum
      if (data_buffer[3] == CS) {
        // Compute the distance in millimeters
        distance_mm = (data_buffer[1] << 8) + data_buffer[2];

        // Convert distance to centimeters
        distance_cm = distance_mm / 10.0;

        // Print the distance to the serial monitor in cm
        Serial.print("Distance: ");
        Serial.print(distance_cm, 1); // Print with 1 decimal place
        Serial.println(" cm");
      } else {
        // Print an error message if the checksum is invalid
        Serial.println("Checksum error! Invalid data.");
      }
    }
  }

  // Small delay to avoid flooding the serial monitor
  delay(10);
}
