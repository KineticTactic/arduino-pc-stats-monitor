#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <chrono>

#include "SerialClass.h"
#include "CPU.h"

std::string beautify_duration(std::chrono::seconds input_seconds)
{
	using namespace std::chrono;
	typedef duration<int, std::ratio<86400>> days;
	auto d = duration_cast<days>(input_seconds);
	input_seconds -= d;
	auto h = duration_cast<hours>(input_seconds);
	input_seconds -= h;
	auto m = duration_cast<minutes>(input_seconds);
	input_seconds -= m;
	auto s = duration_cast<seconds>(input_seconds);

	auto dc = d.count();
	auto hc = h.count();
	auto mc = m.count();
	auto sc = s.count();

	std::stringstream ss;
	ss.fill('0');
	if (dc) {
		ss << d.count() << "d";
	}
	if (dc || hc) {
		if (dc) { ss << std::setw(2); } //pad if second set of numbers
		ss << h.count() << "h";
	}
	if (dc || hc || mc) {
		if (dc || hc) { ss << std::setw(2); }
		ss << m.count() << "m";
	}
	//if ((dc || hc || mc || sc) && mc < 10) {
		//if (dc || hc || mc) { ss << std::setw(2); }
		//ss << s.count() << 's';
	//}

	return ss.str();
}

std::vector<unsigned char> ToPixels(HBITMAP BitmapHandle, int& width, int& height)
{
	BITMAP Bmp = { 0 };
	BITMAPINFO Info = { 0 };
	std::vector<unsigned char> Pixels = std::vector<unsigned char>();

	HDC DC = CreateCompatibleDC(NULL);
	std::memset(&Info, 0, sizeof(BITMAPINFO)); //not necessary really..
	HBITMAP OldBitmap = (HBITMAP)SelectObject(DC, BitmapHandle);
	GetObject(BitmapHandle, sizeof(Bmp), &Bmp);

	Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	Info.bmiHeader.biWidth = width = Bmp.bmWidth;
	Info.bmiHeader.biHeight = height = Bmp.bmHeight;
	Info.bmiHeader.biPlanes = 1;
	Info.bmiHeader.biBitCount = Bmp.bmBitsPixel;
	Info.bmiHeader.biCompression = BI_RGB;
	Info.bmiHeader.biSizeImage = ((width * Bmp.bmBitsPixel + 31) / 32) * 4 * height;

	Pixels.resize(Info.bmiHeader.biSizeImage);
	GetDIBits(DC, BitmapHandle, 0, height, &Pixels[0], &Info, DIB_RGB_COLORS);
	SelectObject(DC, OldBitmap);

	height = std::abs(height);
	DeleteDC(DC);
	return Pixels;
}


int main(int argc, char* argv[])
{
	ShowWindow(GetConsoleWindow(), SW_SHOW);

	Serial* SP;

	char incomingData[256] = "";
	int dataLength = 255;
	int readResult = 0;

	CPU cpu;

	HDC dng = GetDC(NULL);
	RECT rect;
	BYTE* bitPointer;

	long int screenUpdateInterval = 1000;

	while (true) {
		while (true) {
			std::cout << "Trying to connect" << std::endl;
			SP = new Serial("\\\\.\\COM3");
			if (SP->IsConnected()) {
				printf("We're connected\n");
				break;
			}
			Sleep(5000);
		}

		SP->WriteData("CConnecting...", 32);
		Sleep(2000);
		SP->WriteData("CConnected to    KineticPC!", 32);
		Sleep(2000);

		long int lastTransmission = GetTickCount();
		long int lastScreenUpdate = 0;

		std::string uptime;
		float cpuLoad;
		std::stringstream cpuLoadString;
		int memoryUsage;

		while (SP->IsConnected()) {
			readResult = SP->ReadData(incomingData, dataLength);
			// printf("Bytes read: (0 means no data available) %i\n",readResult);
			incomingData[readResult] = 0;

			printf("%s", incomingData);

			if (GetTickCount() - lastTransmission >= 5000) {
				std::cout << "Connection lost. Trying to connect again" << std::endl;
				break;
			}

			if (strcmp(incomingData, "GOT") != -1) {
				lastTransmission = GetTickCount();

				std::cout << std::endl;
				Sleep(200);

				std::cout << "Processing color: ";

				GetWindowRect(GetDesktopWindow(), &rect);
				int MAX_WIDTH = rect.right;
				int MAX_HEIGHT = rect.bottom;

				HDC hdcTemp = CreateCompatibleDC(dng);
				BITMAPINFO bitmap;
				bitmap.bmiHeader.biSize = sizeof(bitmap.bmiHeader);
				bitmap.bmiHeader.biWidth = MAX_WIDTH;
				bitmap.bmiHeader.biHeight = MAX_HEIGHT;
				bitmap.bmiHeader.biPlanes = 1;
				bitmap.bmiHeader.biBitCount = 32;
				bitmap.bmiHeader.biCompression = BI_RGB;
				bitmap.bmiHeader.biSizeImage = MAX_WIDTH * 4 * MAX_HEIGHT;
				bitmap.bmiHeader.biClrUsed = 0;
				bitmap.bmiHeader.biClrImportant = 0;
				HBITMAP hBitmap2 = CreateDIBSection(hdcTemp, &bitmap, DIB_RGB_COLORS, (void**)(&bitPointer), NULL, NULL);
				SelectObject(hdcTemp, hBitmap2);
				BitBlt(hdcTemp, 0, 0, MAX_WIDTH, MAX_HEIGHT, dng, 0, 0, SRCCOPY);

				int sumR = 0, sumB = 0, sumG = 0;
				int numPixels = 0;

				for (int x = 400; x < 1500; x += 20) {
					for (int y = 100; y < 900; y += 20) {
						int index = (y * MAX_WIDTH + x) * 4;
						int r = bitPointer[index + 2];
						int g = bitPointer[index + 1];
						int b = bitPointer[index + 0];

						sumR += r;
						sumG += g;
						sumB += b;

						numPixels++;
					}
				}

				SelectObject(hdcTemp, hBitmap2);
				DeleteObject(hBitmap2);
				DeleteDC(hdcTemp);

				std::cout << "count: " << numPixels;

				int avgR = sumR / numPixels;
				int avgG = sumG / numPixels;
				int avgB = sumB / numPixels;
				std::cout << ", color: (" << avgR << ", " << avgG << ", " << avgB << ")" << std::endl;

				std::string r = std::string(3 - min(3, std::to_string(avgR).length()), '0') + std::to_string(avgR);
				std::string g = std::string(3 - min(3, std::to_string(avgG).length()), '0') + std::to_string(avgG);
				std::string b = std::string(3 - min(3, std::to_string(avgB).length()), '0') + std::to_string(avgB);

				std::string data = 'A' + r + g + b;
				//SP->WriteData((r + g + b).c_str(), 9);

				if (GetTickCount() - lastScreenUpdate >= screenUpdateInterval) {
					lastScreenUpdate = GetTickCount();

					uptime = beautify_duration(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(GetTickCount64())));

					//float load = GetCPULoad() * 100;
					cpuLoad = cpu.get();
					cpuLoadString.str("");
					cpuLoadString << std::fixed << std::setprecision(cpuLoad >= 100 ? 1 : 2) << cpuLoad;

					MEMORYSTATUSEX statex;
					statex.dwLength = sizeof(statex);
					GlobalMemoryStatusEx(&statex);
					memoryUsage = statex.dwMemoryLoad;

					std::string stats = ("CPU:" + cpuLoadString.str() + (cpuLoad >= 10.0 ? " " : "  ")
						+ "RAM:" + std::to_string(memoryUsage) + "UP:" + uptime + std::string(16 - uptime.length() - 7, ' '));

					data += stats;

					data[0] = 'B';
				}
				std::cout << data << ", Length: " << data.length() << std::endl;
				SP->WriteData(data.c_str(), data.length());
			}
		}
	}
	ReleaseDC(GetDesktopWindow(), dng);

	return 0;
}