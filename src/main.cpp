#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SPI.h>
#include <SD.h>

#define FIRMWARE_VERSION 0.00
#define LED 5
#define API_IP "172.20.10.4"
#define API_BASE_URL "http://172.20.10.4"
#define TOKEN_ENDPOINT "http://172.20.10.4:5000/connect/token"

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

String getAccessToken(String tokenEndpoint, String clientId, String clientSecret, const char *certificate)
{
    if (WiFi.status() == WL_CONNECTED) //Check WiFi connection status
    {
        HTTPClient http;
        http.begin(tokenEndpoint);                              //Specify destination for HTTP request
        http.addHeader("Content-Type", "application/x-www-form-urlencoded"); //Specify content-type header

        int httpResponseCode = http.POST("grant_type=client_credentials&client_id=" + clientId + "&client_secret=" + clientSecret); //Send the actual POST request

        if (httpResponseCode == 200)
        {
            DynamicJsonBuffer jsonBuffer;
            JsonObject &root = jsonBuffer.parseObject(http.getString());

            if (!root.success())
            {
                Serial.println(F("Parsing failed!"));
                return "";
            }

            return root["access_token"];
        }
        else if (httpResponseCode > 0)
        {
            String response = http.getString(); //Get the response to the request

            Serial.println(httpResponseCode); //Print return code
            Serial.println(response);         //Print request answer
        }
        else
        {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        http.end(); //Free resources
    }
    else
    {
        Serial.println("Error in WiFi connection");
    }
}

String checkNewVersion()
{
    if (WiFi.status() != WL_CONNECTED) //Check WiFi connection status
    {
        return "";
    }

    String token = getAccessToken(TOKEN_ENDPOINT, "f2ab0a57-d834-429d-a538-8d6f64f10722", "f72baeae-a836-425e-88d5-99275dc4aa46", "");
    Serial.println(token);

    HTTPClient http;
    http.begin(String(API_BASE_URL) + ":5000/updates/check?currentVersion=" + FIRMWARE_VERSION); //Specify destination for HTTP request
    http.addHeader("Authorization", "Bearer " + token);
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
    IPAddress address(172, 20, 10, 4);
    if (client.connect(address, 5000))
    {
        // Connection Succeed.
        // Fecthing the bin
        Serial.println("Fetching Bin: " + String(updateUrl));

        // Get the contents of the bin file
        client.print(String("GET ") + "/updates/version?version="+ "0.01" + " HTTP/1.1\r\n" +
                     "Host: " + API_IP + "\r\n" +
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
                if (written % 1000 == 0)
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