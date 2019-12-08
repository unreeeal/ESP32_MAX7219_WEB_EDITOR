#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include "LedMatrix.h"
#define FILESYSTEM SPIFFS
// You only need to format the filesystem once
#define FORMAT_FILESYSTEM false


#define NUMBER_OF_DEVICES 1 //number of led matrix connect in series
#define CS_PIN 15
#define CLK_PIN 14
#define MISO_PIN 2 //we do not use this pin just fill to match constructor
#define MOSI_PIN 12
#include <SPIFFS.h>


const char* ssid = "";
const char* password = "";
const char* host = "esp32fs";
WebServer server(80);

//holds the current upload
File fsUploadFile;

LedMatrix ledMatrix = LedMatrix(NUMBER_OF_DEVICES, CLK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

void handleMatrix()
{
	ledMatrix.clear();
	String message = server.arg("hex");

	uint64_t image = getUInt64fromHex(message.c_str());
	draw(image);
	ledMatrix.commit();
	server.send(200, "text/html", "ok");


}
void draw(uint64_t image)
{
	for (int i = 0; i < 8; i++) {

		byte row = (image >> i * 8) & 0xFF;
		Serial.println(row);
		ledMatrix.setColumn(7 - i, row);
	}
	ledMatrix.commit();
}

uint64_t getUInt64fromHex(char const* str)
{
	Serial.println(str);
	uint64_t accumulator = 0;
	for (size_t i = 0; isxdigit((unsigned char)str[i]); ++i)
	{
		char c = str[i];
		accumulator *= 16;
		if (isdigit(c)) /* '0' .. '9'*/
			accumulator += c - '0';
		else if (isupper(c)) /* 'A' .. 'F'*/
			accumulator += c - 'A' + 10;
		else /* 'a' .. 'f'*/
			accumulator += c - 'a' + 10;

	}

	return accumulator;
}


//format bytes
String formatBytes(size_t bytes) {
	if (bytes < 1024) {
		return String(bytes) + "B";
	}
	else if (bytes < (1024 * 1024)) {
		return String(bytes / 1024.0) + "KB";
	}
	else if (bytes < (1024 * 1024 * 1024)) {
		return String(bytes / 1024.0 / 1024.0) + "MB";
	}
	else {
		return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
	}
}

String getContentType(String filename) {
	if (server.hasArg("download")) {
		return "application/octet-stream";
	}
	else if (filename.endsWith(".htm")) {
		return "text/html";
	}
	else if (filename.endsWith(".html")) {
		return "text/html";
	}
	else if (filename.endsWith(".css")) {
		return "text/css";
	}
	else if (filename.endsWith(".js")) {
		return "application/javascript";
	}
	else if (filename.endsWith(".png")) {
		return "image/png";
	}
	else if (filename.endsWith(".gif")) {
		return "image/gif";
	}
	else if (filename.endsWith(".jpg")) {
		return "image/jpeg";
	}
	else if (filename.endsWith(".ico")) {
		return "image/x-icon";
	}
	else if (filename.endsWith(".xml")) {
		return "text/xml";
	}
	else if (filename.endsWith(".pdf")) {
		return "application/x-pdf";
	}
	else if (filename.endsWith(".zip")) {
		return "application/x-zip";
	}
	else if (filename.endsWith(".gz")) {
		return "application/x-gzip";
	}
	return "text/plain";
}

bool exists(String path) {
	bool yes = false;
	File file = FILESYSTEM.open(path, "r");
	if (!file.isDirectory()) {
		yes = true;
	}
	file.close();
	return yes;
}

bool handleFileRead(String path) {
	Serial.println("handleFileRead: " + path);
	if (path.endsWith("/")) {
		path += "index.htm";
	}
	String contentType = getContentType(path);
	String pathWithGz = path + ".gz";
	if (exists(pathWithGz) || exists(path)) {
		if (exists(pathWithGz)) {
			path += ".gz";
		}
		File file = FILESYSTEM.open(path, "r");
		server.streamFile(file, contentType);
		file.close();
		return true;
	}
	return false;
}

void uploadFileActionGet() {
	Serial.println("upload");
	char temp[400];
	int sec = millis() / 1000;
	int min = sec / 60;
	int hr = min / 60;

	snprintf(temp, 400,

		"<html>\
  <head>\
        <title>ESP Upload</title>\
     </head>\
  <body>\
   <form method=\"post\" enctype=\"multipart/form-data\">\
		<input type = \"file\" name =\"name\">\
		<input class = \"button\" type = \"submit\" value = \"Upload\">\
		</form>\
      </body>\
</html>"


);
	server.send(200, "text/html", temp);
}

void uploadFileActionPost()
{
	HTTPUpload& upload = server.upload();
	if (upload.status == UPLOAD_FILE_START) {
		String filename = upload.filename;
		if (filename.length() < 2)
		{

			server.send(500, "text/plain", "500: empty file");
			return;
		}

		if (!filename.startsWith("/")) filename = "/" + filename;
		Serial.print(F("handleFileUpload Name: ")); Serial.println(filename);
		Serial.print("trying open for w");
		fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
		filename = String();
	}
	else if (upload.status == UPLOAD_FILE_WRITE) {
		if (fsUploadFile)
			fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
	}
	else if (upload.status == UPLOAD_FILE_END) {
		if (fsUploadFile) {                                    // If the file was successfully created
			fsUploadFile.close();                               // Close the file again
			Serial.print(F("handleFileUpload Size: ")); Serial.println(upload.totalSize);
			server.sendHeader("Location", "/");      // Redirect the client to the success page
			server.send(303);
		}
		else {
			server.send(500, "text/plain", "500: couldn't create file");
		}
	}
}



void setup(void) {
	ledMatrix.init();
	Serial.begin(115200);
	Serial.print("\n");
	Serial.setDebugOutput(true);
	if (FORMAT_FILESYSTEM) FILESYSTEM.format();
	FILESYSTEM.begin();
	{
		File root = FILESYSTEM.open("/");
		File file = root.openNextFile();
		while (file) {
			String fileName = file.name();
			size_t fileSize = file.size();
			Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
			file = root.openNextFile();
		}
		Serial.printf("\n");
	}


	//WIFI INIT
	Serial.printf("Connecting to %s\n", ssid);
	if (String(WiFi.SSID()) != String(ssid)) {
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssid, password);
	}

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print("Connected! IP address: ");
	Serial.println(WiFi.localIP());

	MDNS.begin(host);
	Serial.print("Open http://");
	Serial.print(host);
	Serial.println(".local/edit to see the file browser");

	server.onNotFound([]() {
		if (!handleFileRead(server.uri())) {
			server.send(404, "text/plain", "FileNotFound");
		}
		});
	server.on("/upload", HTTP_GET, uploadFileActionGet);
	server.on("/upload", HTTP_POST, []() { server.send(200); }, uploadFileActionPost);
	server.on("/matrix", handleMatrix);

	server.begin();
	Serial.println("HTTP server started");

}

void loop(void) {
	server.handleClient();
}