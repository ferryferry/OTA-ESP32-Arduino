#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SPI.h>
#include <SD.h>

#define FIRMWARE_VERSION 0.00
#define LED 5
#define API_BASE_URL "http://172.20.10.4"

const char *ssid = "Ferry’s iPhone";
const char *password = "Jongmans1";

void setup()
{
    Serial.begin(115200);
    if (!SD.begin(4))
    {
        Serial.println("Card Mount Failed");
        return;
    }

    Serial.println("Current firmware version: " + String(FIRMWARE_VERSION));
    pinMode(LED, OUTPUT);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }

    Serial.println("Connected to the WiFi network");
}

String checkNewVersion()
{
    if (WiFi.status() != WL_CONNECTED) //Check WiFi connection status
    {
        return "";
    }

    HTTPClient http;
    http.begin(String(API_BASE_URL) + ":5000/updates/check?currentVersion=" + FIRMWARE_VERSION); //Specify destination for HTTP request
    http.addHeader("Authorization", "Bearer eyJhbGciOiJSUzI1NiIsImtpZCI6IjQ4LUxITERQVS1EV1FaRFdFNzM2S1U0LUhOU09WR0RDVVlYNF9YSlEiLCJ0eXAiOiJKV1QifQ.eyJzdWIiOiJmMmFiMGE1Ny1kODM0LTQyOWQtYTUzOC04ZDZmNjRmMTA3MjIiLCJuYW1lIjoiVGVzdCBCaWtlIChBcHApIDMiLCJ0b2tlbl91c2FnZSI6ImFjY2Vzc190b2tlbiIsImp0aSI6IjBiMDAyOTk3LTg3ZjEtNGVhNS05NDYyLTc4MDI3NGRkNDY2YSIsImNmZF9sdmwiOiJwcml2YXRlIiwiYXVkIjoicmVzb3VyY2Vfc2VydmVyIiwiYXpwIjoiZjJhYjBhNTctZDgzNC00MjlkLWE1MzgtOGQ2ZjY0ZjEwNzIyIiwibmJmIjoxNTQzMjQyMDY5LCJleHAiOjE1NDMyNDU2NjksImlhdCI6MTU0MzI0MjA2OSwiaXNzIjoiaHR0cDovLzE3Mi4yMC4xMC40OjUwMDAvIn0.4eUZWeEgeVrx9h5e0zWP2bNhHDFAFeDkvta9zOOnfWm-uUBOxzK7ea1hYbA8ZfEDIm_ne_HRyk7AorZCSsqXN_hL1-NGlq5CDlMvPQy7LNSFbGNT0B8eX8foxW1k5GqJSX41_0qkuDbvBlcrGex_Jrngo5Yhib5Os9cdSUHktVueaFrSN_H5JUeZZcJjmYUsNgR7HTM3q-1Z5HhFd3L85TSgZV63jo8L8WtFOATpQOI2Fg1dAGh6OMFB-IdQyN9QZd4ZZ0bzF6KEBLJZWm-hI-X7Xreh0xx2yfqSccxSHVNgnl7qkA9gHqLlS0Vy9pexTWbGH0esisNfw1n8n47_Qw");
    http.addHeader("Content-Type", "application/json");
    String firmwareDownloadUrl = "";

    int response = http.sendRequest("PUT", "0");
    
    if (response > 0)
    {
        firmwareDownloadUrl = String(API_BASE_URL) + ":5000" + http.getString();
    }

    http.end();
    return firmwareDownloadUrl;
}

String getHeaderValue(String header, String headerName)
{
    return header.substring(strlen(headerName.c_str()));
}

void onProgress(size_t written, size_t total)
{
    Serial.println("Written: " + String(written) + " / " + String(total));
}

void update(String host, String updateUrl)
{
    WiFiClient client;
    int contentLength = 0;
    bool isValidContentType = false;

    Serial.println("Connecting to: " + String(host));
    Serial.println("URL: " + updateUrl);
    // Connect to S3
    IPAddress address(172,20,10,4);
    if (client.connect(address, 5000))
    {
        // Connection Succeed.
        // Fecthing the bin
        Serial.println("Fetching Bin: " + String(updateUrl));

        // Get the contents of the bin file
        client.print(String("GET ") + "/updates/version?version=0.01" + " HTTP/1.1\r\n" +
                     "Host: " + "172.20.10.4" + "\r\n" +
                     "Cache-Control: no-cache\r\n" +
                     "Connection: close\r\n\r\n");

        unsigned long timeout = millis();
        while (client.available() == 0)
        {
            if (millis() - timeout > 5000)
            {
                Serial.println("Client Timeout !");
                client.stop();
                return;
            }
        }

        while (client.available())
        {
            // read line till /n
            String line = client.readStringUntil('\n');
            // remove space, to check if the line is end of headers
            line.trim();
            Serial.println(line);

            // if the the line is empty,
            // this is end of headers
            // break the while and feed the
            // remaining `client` to the
            // Update.writeStream();
            if (!line.length())
            {
                //headers ended
                break; // and get the OTA started
            }

            // Check if the HTTP Response is 200
            // else break and Exit Update
            if (line.startsWith("HTTP/1.1"))
            {
                if (line.indexOf("200") < 0)
                {
                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
                    break;
                }
            }

            // extract headers here
            // Start with content length
            if (line.startsWith("Content-Length: "))
            {
                contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
                Serial.println("Got " + String(contentLength) + " bytes from server");
            }

            // Next, the content type
            if (line.startsWith("Content-Type: "))
            {
                String contentType = getHeaderValue(line, "Content-Type: ");
                Serial.println("Got " + contentType + " payload.");
                if (contentType == "application/octet-stream")
                {
                    isValidContentType = true;
                }
            }
        }
    }
    else
    {
        // Connect to S3 failed
        // May be try?
        // Probably a choppy network?
        Serial.println("Connection to " + String(updateUrl) + " failed. Please check your setup");
        // retry??
        // execOTA();
    }

    // Check what is the contentLength and if content type is `application/octet-stream`
    Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

    // check contentLength and content type
    if (contentLength && isValidContentType)
    {
        // Open the file from the SD card to write
        File updateBinary = SD.open("/update.bin", FILE_WRITE);
        if (!updateBinary)
        {
            return;
        }

        Serial.println("Begin writing to SD card");

        size_t written = 0;

        while (written < contentLength)
        {
            if (client.available())
            {
                updateBinary.write(client.read());
                if (written % 100 == 0)
                {
                    Serial.println("Wrote: " + String(written) + " of: " + contentLength);
                }
                written++;
            }
        }

        updateBinary.close();

        Serial.println("Write completed");
        //updateBinary.write((uint8_t)client.read(), (size_t)contentLength);

        Update.onProgress(onProgress);
        // Check if there is enough to OTA Update
        bool canBegin = Update.begin(contentLength);

        // If yes, begin
        if (canBegin)
        {
            Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
            // No activity would appear on the Serial monitor
            // So be patient. This may take 2 - 5mins to complete
            File updateBinary = SD.open("/update.bin", FILE_READ);
            if (!updateBinary)
            {
                return;
            }
            size_t written = Update.writeStream(updateBinary);

            if (written == contentLength)
            {
                Serial.println("Written : " + String(written) + " successfully");
            }
            else
            {
                Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
                // retry??
                // execOTA();
            }

            if (Update.end())
            {
                // Close the filestream
                updateBinary.close();
                // Remove the update file
                SD.remove("/update.bin");

                Serial.println("OTA done!");
                if (Update.isFinished())
                {
                    Serial.println("Update successfully completed. Rebooting.");
                    ESP.restart();
                }
                else
                {
                    Serial.println("Update not finished? Something went wrong!");
                }
            }
            else
            {
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            }
        }
        else
        {
            // not enough space to begin OTA
            // Understand the partitions and
            // space availability
            Serial.println("Not enough space to begin OTA");
            client.flush();
        }
    }
    else
    {
        Serial.println("There was no content in the response");
        client.flush();
    }
}

void loop()
{
    digitalWrite(LED, HIGH);

    String updateUrl = checkNewVersion();
    if (updateUrl != "")
    {
        Serial.println("Update available!");
        Serial.println(updateUrl);

        Serial.println("Calling update method");
        update(API_BASE_URL, updateUrl);
    }

    digitalWrite(LED, LOW);
    delay(10000);
}