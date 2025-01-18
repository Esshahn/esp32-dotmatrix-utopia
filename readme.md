# Framed Utopia

A tiny window into a better world – one whimsical utopia at a time.

## About

Framed Utopia is an art prototype that displays bright glimpses of impossible happiness on a 64x64 LED matrix display. In a world often dominated by negative news, this digital picture frame offers a moment of respite through playful, uplifting messages about imaginary utopias.

Examples of messages you might see:
- "islands made out of marshmallows"
- "chocolate is now super healthy"
- "clouds taste like cotton candy"

The display fetches new messages from an API endpoint and shows them in random colors, creating a constantly changing kaleidoscope of positive possibilities.

## Hardware

- 64x64 RGB LED Matrix Display (HUB75 interface)
- ESP32 microcontroller
- Power supply (5V)

## Features

- Displays messages in random colors for visual variety
- Automatically adjusts brightness based on time of day
- Energy efficient: sleeps during night hours (10 PM - 7 AM)
- Refreshes content every hour
- WiFi-connected for remote content updates

## Technical Details

### Dependencies

- ESP32-HUB75-MatrixPanel-I2S-DMA
- WiFi
- HTTPClient
- Arduino_JSON

### Configuration

Create an `env.h` file with your configuration:

```cpp
const char* ssid = "your-wifi-ssid";
const char* password = "your-wifi-password";
const char* serverName = "your-api-endpoint";
```

### Display Configuration

- Resolution: 64x64 pixels
- Interface: HUB75
- Driver: FM6124
- Brightness: Adjustable (default: 100/255)

### Power Management

The display automatically:
- Operates between 7 AM and 10 PM
- Enters deep sleep mode during night hours
- Wakes up every 10 minutes to check time
- Updates content every hour during active hours

## Setup Instructions

1. Install the required libraries in your Arduino IDE
2. Create the `env.h` file with your WiFi and API credentials
3. Connect the ESP32 to your LED matrix following HUB75 pinout
4. Upload the code to your ESP32
5. Power on and wait for the "utopia" startup message

## API Endpoint Requirements

Your API endpoint should return JSON in the format:

```json
{
    "msg": "your utopian message here"
}
```

## Contributing

This is an art prototype, but feel free to fork and create your own version of positivity!

## Background

In a world where news feeds and social media often amplify negative events, Framed Utopia aims to create tiny moments of joy through absurd and delightful possibilities. It's not about ignoring reality – it's about remembering that imagination and whimsy have their place in making our day a little brighter.

## License

This project is open source and available under the MIT License.