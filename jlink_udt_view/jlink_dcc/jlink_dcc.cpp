// jlink_dcc.cpp : 定义控制台应用程序的入口点。
//
#define _CRT_SECURE_NO_WARNINGS
#include "stdafx.h"
#include <windows.h>
#include <typeinfo>
#include <conio.h>
#include "tlhelp32.h" 
#include<signal.h>
#include "direct.h"
#include <time.h>
#include <stdio.h>
#include <string.h>


BOOL(*JLINKARM_IsHalted)(void);
BOOL(*JLINKARM_Halt)(void);
void (*JLINKARM_Open)(void);
void(*JLINKARM_Close)(void);
bool (*JLINKARM_IsOpen)(void);
UINT32 (*JLINKARM_GetSN)(void);
UINT32 (*JLINKARM_GetId)(void);
UINT32 (*JLINKARM_GetDLLVersion)(void);
UINT32 (*JLINKARM_GetSpeed)(void);
UINT32 (*JLINKARM_ReadDCC)(UINT32 *buf, UINT32 size, INT timeout);
UINT32 (*JLINKARM_WriteDCC)(const UINT32 *buf, UINT32 size, INT timeout);
UINT32 (*JLINKARM_ReadDCCFast)(UINT32 *buf, UINT32 size, INT timeout);
UINT32 (*JLINKARM_WriteDCCFast)(const UINT32 *buf, UINT32 size, INT timeout);
UINT32 (*JLINKARM_WaitDCCRead)(UINT32 timeout);
void (*JLINKARM_SetSpeed)(UINT32 spd);
UINT32(*JLINKARM_ExecCommand)(const char* cmd,UINT32 a,UINT32 b);

void(*JLINKARM_WriteU8)(UINT32 addr, BYTE dat);
void(*JLINKARM_WriteU16)(UINT32 addr, UINT16 dat);
void(*JLINKARM_WriteU32)(UINT32 addr, UINT32 dat);
void(*JLINKARM_ReadMemU8)(UINT32 addr, UINT32 leng, BYTE *buf, BYTE *status);
void(*JLINKARM_ReadMemU16)(UINT32 addr, UINT32 leng, UINT16 *buf, BYTE *status);
UINT32 (*JLINKARM_ReadMemU32)(UINT32 addr, UINT32 leng, UINT32 *buf, BYTE *status);
void(*JLINKARM_ReadMemHW)(UINT32 addr, UINT32 leng, BYTE *buf);
UINT32 (__stdcall *JLINK_TIF_Select)(UINT32 tif);
UINT32(__stdcall*JLINK_Connect)(void);
UINT32(__stdcall*JLINK_IsConnected)(void);
void(*JLINKARM_ReadMem)(UINT32 addr, UINT32 leng, BYTE *buf);
void(*JLINK_ReadMemU8)(UINT32 addr, UINT32 leng, BYTE *buf, BYTE *status);
UINT32 (*JLINK_GetMemZones)(UINT32 a, UINT32 b);
void(*JLINKARM_Go)(void);



int load_jlinkarm_dll(void)
{
	HINSTANCE lib = LoadLibrary("JLinkARM.dll");
	if (lib == NULL) {
		return -1;
	}

	JLINKARM_Open = (void(*)(void))(GetProcAddress(lib, "JLINKARM_Open"));
	JLINKARM_Close = (void(*)(void))(GetProcAddress(lib, "JLINKARM_Close"));
	JLINKARM_IsOpen = (bool(*)(void))GetProcAddress(lib, "JLINKARM_IsOpen");
	JLINKARM_GetSN = (UINT32(*)(void))GetProcAddress(lib, "JLINKARM_GetSN");
	JLINKARM_GetId = (UINT32(*)(void))GetProcAddress(lib, "JLINKARM_GetId");
	JLINKARM_GetDLLVersion = (UINT32(*)(void))GetProcAddress(lib, "JLINKARM_GetDLLVersion");
	JLINKARM_GetSpeed = (UINT32(*)(void))GetProcAddress(lib, "JLINKARM_GetSpeed");
	JLINKARM_ReadDCC = (UINT32(*)(UINT32 *, UINT32, INT))GetProcAddress(lib, "JLINKARM_ReadDCC");
	JLINKARM_WriteDCC = (UINT32(*)(const UINT32 *, UINT32, INT))GetProcAddress(lib, "JLINKARM_WriteDCC");
	JLINKARM_ReadDCCFast = (UINT32(*)(UINT32 *, UINT32, INT))GetProcAddress(lib, "JLINKARM_ReadDCCFast");
	JLINKARM_WriteDCCFast = (UINT32(*)(const UINT32 *, UINT32, INT))GetProcAddress(lib, "JLINKARM_WriteDCCFast");
	JLINKARM_WaitDCCRead = (UINT32(*)(UINT32))GetProcAddress(lib, "JLINKARM_WaitDCCRead");
	JLINKARM_SetSpeed = (void(*)(UINT32))GetProcAddress(lib, "JLINKARM_SetSpeed");
	JLINKARM_ExecCommand = (UINT32(*)(const char *, UINT32, UINT32))GetProcAddress(lib, "JLINKARM_ExecCommand");

	JLINKARM_WriteU8 = (void(*)(UINT32, BYTE))GetProcAddress(lib, "JLINKARM_WriteU8");
	JLINKARM_WriteU16 = (void(*)(UINT32, UINT16))GetProcAddress(lib, "JLINKARM_WriteU16");
	JLINKARM_WriteU32 = (void(*)(UINT32, UINT32))GetProcAddress(lib, "JLINKARM_WriteU32");
	JLINKARM_ReadMemU8 = (void(*)(UINT32, UINT32, BYTE *, BYTE *))GetProcAddress(lib, "JLINKARM_ReadMemU8");
	JLINKARM_ReadMemU16 = (void(*)(UINT32, UINT32, UINT16 *, BYTE *))GetProcAddress(lib, "JLINKARM_ReadMemU16");
	JLINKARM_ReadMemU32 = (UINT32(*)(UINT32, UINT32, UINT32 *, BYTE *))GetProcAddress(lib, "JLINKARM_ReadMemU32");

	JLINKARM_ReadMem = (void(*)(UINT32, UINT32, BYTE *))GetProcAddress(lib, "JLINKARM_ReadMem");
	JLINKARM_ReadMemHW = (void(*)(UINT32, UINT32, BYTE *))GetProcAddress(lib, "JLINKARM_ReadMemHW");

	JLINK_ReadMemU8 = (void(*)(UINT32, UINT32, BYTE *,BYTE*))GetProcAddress(lib, "JLINK_ReadMemU8");

	JLINK_GetMemZones = (UINT32(*)(UINT32, UINT32))GetProcAddress(lib,"JLINK_GetMemZones");


	JLINK_TIF_Select = (UINT32(__stdcall*)(UINT32))GetProcAddress(lib, "JLINK_TIF_Select");
	JLINK_Connect = (UINT32(__stdcall*)(void))GetProcAddress(lib, "JLINK_Connect");
	JLINK_IsConnected = (UINT32(__stdcall*)(void))GetProcAddress(lib, "JLINK_IsConnected");

	JLINKARM_Go = (void(*)(void))GetProcAddress(lib, "JLINKARM_Go");
	JLINKARM_Halt = (BOOL (*)(void))GetProcAddress(lib, "JLINKARM_Halt");
	JLINKARM_IsHalted = (BOOL(*)(void))GetProcAddress(lib, "JLINKARM_IsHalted");
	return 0;
}




/*
FOREGROUND_BLUE 前景色为蓝
FOREGROUND_GREEN 前景色为绿
FOREGROUND_RED 前景色为红
FOREGROUND_INTENSITY 前景色为强光
BACKGROUND_BLUE 背景色为蓝
BACKGROUND_GREEN 背景色为绿
BACKGROUND_RED 背景色为红
BACKGROUND_INTENSITY 背景色为强光
*/
#define FORGOUND_WHITE	(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define BACKGOUND_WHITE	(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)
int change_ch(int ch)
{
	int res;
	static int last = 0;
	UINT color_tbl[] = {
		//字体亮
		FOREGROUND_INTENSITY | FORGOUND_WHITE,	//白
		FOREGROUND_INTENSITY | FOREGROUND_RED,	//红
		FOREGROUND_INTENSITY | FOREGROUND_GREEN,	//绿
		FOREGROUND_INTENSITY | FOREGROUND_BLUE,	//蓝
		FORGOUND_WHITE,	//白
		FOREGROUND_RED,	//红
		FOREGROUND_GREEN,	//绿
		FOREGROUND_BLUE,	//蓝
		//背景亮
		BACKGROUND_INTENSITY | BACKGOUND_WHITE,	//白
		BACKGROUND_INTENSITY | BACKGROUND_RED,
		BACKGROUND_INTENSITY | BACKGROUND_GREEN,
		BACKGROUND_INTENSITY | BACKGROUND_BLUE,
		BACKGOUND_WHITE,	//白
		BACKGROUND_RED,
		BACKGROUND_GREEN,
		BACKGROUND_BLUE,
	};
	if (ch == 'w')ch = 0;
	if (ch == 'r')ch = 1;
	if (ch == 'g')ch = 2;
	if (ch == 'b')ch = 3;
	if (ch == 'W')ch = 8;
	if (ch == 'R')ch = 9;
	if (ch == 'G')ch = 10;
	if (ch == 'B')ch = 11;
	if (ch >= sizeof(color_tbl) / sizeof(UINT))
		return last;
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color_tbl[ch]);
	res = last;
	last = ch;
	return res;
}
UINT32 trad_addr = 0;

void dcc_send(int c)
{
	UINT32 res;
	UINT32 data = 0x55000000u | c;
	//while (in_api);
	//in_api = 1;
	int ts = 20;
	while (--ts > 0) {
		res = JLINKARM_WriteDCC(&data, 1, 50);
		if (res == 1) {
			break;
		}
	}
}

void trad_send(int c)
{
	UINT32 data = 0x55000000u | c;
	UINT32 res;
	JLINKARM_WriteU32(trad_addr + 12, data);	//先清数据
	JLINKARM_WriteU32(trad_addr + 8, 1);		//后清除标志
	do {
		JLINKARM_ReadMemU32(trad_addr+8, 1, &res, 0);
	} while (res);
}

FILE* flog;
time_t log_time;
DWORD log_tickcount;
void log_putchar(int c)
{
	if (flog == NULL)
		return;
	fputc(c, flog);
	if (c == '\n') {
		DWORD n = GetTickCount()- log_tickcount;
		time_t t = log_time + n / 1000;
		int ms = n % 1000;
		struct tm *tm = localtime(&t);
		fprintf(flog, "[%02u:%02u:%02u:%03u]\t",
			tm->tm_hour,tm->tm_sec,tm->tm_sec,ms);
	}
	fflush(flog);
}

//volatile BOOL in_api = 0;
DWORD WINAPI ThreadFun(LPVOID pM)
{
	UINT32 res;
	while (1) {
		unsigned char c = _getch();
		if (c == 0xE0) {
			//^V<> HPKM


			c = _getch();
			switch (c)
			{
			case 'H':	//u
				trad_send('\x1b');
				trad_send('[');
				trad_send('A');
				break;
			case 'P':	//d
				trad_send('\x1b');
				trad_send('[');
				trad_send('B');
				break;
			case 'M':	//l
				trad_send('\x1b');
				trad_send('[');
				trad_send('C');
				break;
			case 'K':	//r
				trad_send('\x1b');
				trad_send('[');
				trad_send('D');
				break;
				//case 'I':	//pu
				//	dcc_send('D');
				//	break;
				//case 'Q':	//pd
				//	dcc_send('Q');
				//	break;
				//case 'G':	//home
				//	dcc_send('G');
				//	break;
				//case 'O':	//end
				//	dcc_send('O');
				//	break;
			default:
				break;
			}
		}
		else
		{
			trad_send(c);
			//printf("input: [%c] %02X\r\n", c, (int)c);

			//in_api = 0;
		}
	}
}

int jlink_read_mem(UINT32 *c)
{
	UINT32 buff[2] = { 0 };
	//BYTE status;
	//JLINKARM_ReadMemHW(trad_addr, 8, (BYTE*)buff);
	BYTE state = 0;
	INT32 res = JLINKARM_ReadMemU32(trad_addr, 2, buff, 0);// &state);
	if (res<0) {
		return res;// printf("%d\r\n", res);
	}
	if (res == 2) {
		if (buff[0])
		{
			JLINKARM_WriteU32(trad_addr + 4, 0);		//先清数据
			JLINKARM_WriteU32(trad_addr, 0);		//后清除标志
			*c = buff[1];
			return 1;
		}
	}
	return 0;
}
int main(int argc, char *argv[])
{
	trad_addr = 0x10001000;
	signal(SIGINT, 0);
	printf("jlink_trad_view_V0.1 by houxd ,build %s %s\r\n",__DATE__,__TIME__);
	int res;
	res = load_jlinkarm_dll();
	if (res) {
		printf("Cannot Load Dll JLinkARM.dll \r\n");
		getchar();
		return -1;
	}
	printf("Load Dll JLinkARM.dll Success \r\n");

	char *self = argv[0];
	char log_file[512] = { 0 };
	{
		strcpy(log_file, argv[0]);
		
		time_t t;
		time(&t);
		struct tm *tm;
		tm = localtime(&t);
		char log_fname[512];
		sprintf(log_fname, "\\log-%04u%02u%02u.txt",
			tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
		char *p = strrchr(log_file, '\\');
		if (p) {
			strcpy(p,log_fname);
		}
		else {
			strcpy(log_file, "\\jlinkdcc_log.txt");
		}
	}
	flog = fopen(log_file, "a+");
	if (flog == NULL) {
		printf("WARNING: log file can not creat.\r\n");
	}

	char config_file[512] = { 0 };
	{	
		strcpy(config_file, argv[0]);
		char *p = strrchr(config_file, '\\');
		if (p) {
			strcpy(p, "\\jlinkarm_cfg.ini");
		}
		else {
			strcpy(config_file,  "jlinkarm_cfg.ini");
		}
	}


	char device[64]="";
	GetPrivateProfileString("JLINKARM", "device", "", device, 64, config_file);
	UINT32 speed = GetPrivateProfileInt("JLINKARM", "speed", 200, config_file);

	char interf[128] = "JTAG";
	GetPrivateProfileString("JLINKARM", "interface", "JTAG", interf, 128, config_file);

	char tmp[128] = { 0 };
	GetPrivateProfileString("JLINKARM", "address", "0x10001000", tmp, 128, config_file);
	res = sscanf(tmp, "%X", &trad_addr);
	if (res != 1) {
		res = sscanf(tmp, "%x", &trad_addr);
		if (res != 1) {
			res = sscanf(tmp, "%#X", &trad_addr);
			if (res != 1) {
				res = sscanf(tmp, "%#x", &trad_addr);
			}
		}
	}
	printf("TRAD Control Block Address: %#X\r\n", trad_addr);
	printf("Open JLink ... ");
	reinit:
	JLINKARM_Open();
	if (JLINKARM_IsOpen())
	{
		printf("OK\r\n");
		if (device[0]) {
			char buff[128];
			sprintf(buff, "device = %s", device);
			JLINKARM_ExecCommand(buff, 0, 0);
		}
		else {
			printf("Not config device, Press Any Key to setlect.\r\n");
			_getch();
		}
#define JLINKARM_TIF_JTAG	0
#define JLINKARM_TIF_SWD	1
#define JLINKARM_TIF_DBM3	2
#define JLINKARM_TIF_FINE	3
#define JLINKARM_TIF_2wire_JTAG_PIC32	4
		
		//try {
			//if(strstr(interf,"JTAG"))
			//	JLINK_TIF_Select(JLINKARM_TIF_JTAG);
			//else if(strstr(interf,"SWD"))
				JLINK_TIF_Select(JLINKARM_TIF_SWD);
				int conf = JLINK_Connect();
			//else
			//	JLINK_TIF_Select(JLINKARM_TIF_JTAG);
	//	}
		//catch (...) {}
		JLINKARM_SetSpeed(speed);
		//JLINK_Connect();


		printf("JLink Info:\r\n");
		printf("DEVICE = %s\r\n", device);
		//getc(stdin);
		//exit(0);
		printf("SN = %08u\r\n", JLINKARM_GetSN());
		printf("ID = %08X\r\n", JLINKARM_GetId());
		printf("VER = %u\r\n", JLINKARM_GetDLLVersion());
		printf("Speed = %u\r\n", JLINKARM_GetSpeed());
		printf("=============================================================\r\n");

		//JLINK_GetMemZones(0, 0);

		//UINT32 buff[8];
		//BYTE status;
		//while (1) {
		//	JLINKARM_ReadMemU32(0x10001000, 2, buff, &status);
		//	if (buff[0])
		//	{
		//		putchar(buff[1]);
		//		buff[0] = 0;
		//		JLINKARM_WriteU32(0x10001000, 0);
		//	}
		//	
		//}
		//printf("%08x %08x", buff[0], buff[1]);
		//_getch();

		//return 0;

		HANDLE handle = CreateThread(NULL, 0, ThreadFun, NULL, 0, NULL);
		UINT32 buf[256];
		fprintf(flog, "------------------------------------------------------------\r\n");
		log_tickcount = GetTickCount();
		time(&log_time);
		log_putchar('\n');	//record a time label
		int first = 1;
	L001:
		Sleep(100);
		if (!JLINKARM_IsOpen()) {
			JLINKARM_Open();
			char buff[128];
			sprintf(buff, "device = %s", device);
			JLINKARM_ExecCommand(buff, 0, 0);
			JLINKARM_SetSpeed(speed);
		}
		if(JLINKARM_IsHalted())
			JLINKARM_Go();

		while (1) {
			//while (in_api);
			//in_api = 1;
			res = jlink_read_mem(buf);//JLINKARM_ReadDCC(buf, 256, 2);
			if (res == -1) {
				if (first)
				{
					continue;
				}
				else
				{
					//JLINKARM_Halt();
					//JLINKARM_Go();
					JLINKARM_Close();
					goto L001;
				}
			}
			else 
			{
				first = 0;
			}
			//in_api = 0;
			if (res) {
				for (int i = 0; i < res; i++) {
					UINT32 data = buf[i];
					if ((data>>24) != 0x54)
						continue;
					int ch = (data >> 8) & 0xFFu;
					int chbak = change_ch(ch);
					int c = data & 0xFFu;
					if (ch >= 0 && ch <= 0x7F) {
						putchar(c);
						log_putchar(c);
					}
					change_ch(chbak);
				}
			}
			//Sleep(10);
		}

		WaitForSingleObject(handle, INFINITE);
		JLINKARM_Close();
	}

	return 0;
}
