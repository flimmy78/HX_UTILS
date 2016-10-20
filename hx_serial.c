#include "hx_utils.h"
#include "hx_serial.h"
#include "string.h"
#include "stdarg.h"
#include "stdio.h"
#include "hx_target.h"
#include "hx_debug.h"

/*
	Board UART need
*/
extern void HX_BRD_UART_DIS_INTR(int id);
extern void HX_BRD_UART_EN_INTR(int id);
extern void HX_BRD_UART_TX_START_BYTE(int id,int data);
extern void HX_BRD_UART_TX_END_SESSION(int id);
extern int  HX_BRD_UART_REOPEN(int id, int bps);

struct UART_CTX_T {
	int rx_buf_size;
	volatile char * rx_buffer;
	volatile int rx_pos;
	volatile int read_pos;
	int tx_buf_size;
	volatile char * tx_buffer;
	volatile int tx_pos;
	volatile int tx_write_pos;
};

#ifndef __HX_TARGET_H__
#error must refer "hx_port.h"
#endif

#if (UART0_RX_BUFF_SIZE > 0) && (UART0_TX_BUFF_SIZE > 0)
static volatile char uart0_rx_buffer[UART0_RX_BUFF_SIZE];
static volatile char uart0_tx_buffer[UART0_TX_BUFF_SIZE];
#endif
#if (UART1_RX_BUFF_SIZE > 0) && (UART1_TX_BUFF_SIZE > 0)
static volatile char uart1_rx_buffer[UART1_RX_BUFF_SIZE];
static volatile char uart1_tx_buffer[UART1_TX_BUFF_SIZE];
#endif
#if (UART2_RX_BUFF_SIZE > 0) && (UART2_TX_BUFF_SIZE > 0)
static volatile char uart2_rx_buffer[UART2_RX_BUFF_SIZE];
static volatile char uart2_tx_buffer[UART2_TX_BUFF_SIZE];
#endif
#if (UART3_RX_BUFF_SIZE > 0) && (UART3_TX_BUFF_SIZE > 0)
static volatile char uart3_rx_buffer[UART3_RX_BUFF_SIZE];
static volatile char uart3_tx_buffer[UART3_TX_BUFF_SIZE];
#endif

static volatile struct UART_CTX_T g_uart[] = {
#if UART0_RX_BUFF_SIZE > 0
	{UART0_RX_BUFF_SIZE,uart0_rx_buffer,0,0, UART0_TX_BUFF_SIZE,uart0_tx_buffer,0,0},
#else
	{0,0,0,0},
#endif
#if UART1_RX_BUFF_SIZE > 0
	{UART1_RX_BUFF_SIZE,uart1_rx_buffer,0,0, UART1_TX_BUFF_SIZE,uart1_tx_buffer,0,0},
#else
	{0,0,0,0},
#endif
#if UART2_RX_BUFF_SIZE > 0
	{UART2_RX_BUFF_SIZE,uart2_rx_buffer,0,0, UART2_TX_BUFF_SIZE,uart2_tx_buffer,0,0},
#else
	{0,0,0,0},
#endif
#if UART3_RX_BUFF_SIZE > 0
	{UART3_RX_BUFF_SIZE,uart3_rx_buffer,0,0, UART3_TX_BUFF_SIZE,uart3_tx_buffer,0,0},
#else
	{0,0,0,0},
#endif
};

/*
#define UART_RX_BYTE(n,data) 							\
{ 														\
	if(g_uart[n].rx_pos < g_uart[n].rx_buf_size) 		\
	{ 													\
		g_uart[n].rx_buffer[g_uart[n].rx_pos] = data; 	\
		g_uart[n].rx_pos ++; 							\
	} else {											\
		g_uart[n].rx_pos = 0; 							\
		g_uart[n].read_pos = 0; 						\
	} 													\
}
*/

void UART_RX_BYTE(int id, int data)
{ 														
	if(g_uart[id].rx_pos < g_uart[id].rx_buf_size) 		
	{ 													
		g_uart[id].rx_buffer[g_uart[id].rx_pos] = data; 	
		g_uart[id].rx_pos ++; 							
	} else {
		g_uart[id].rx_pos = 0; 							
		g_uart[id].read_pos = 0; 						
	} 													
} 
#ifdef HX_DEBUG_SERIAL_INPUT

#if HX_DEBUG_SERIAL_INPUT == UART_DEBUG_PORT
#error cannot debug self!!!
#endif

#ifdef __HX_ENABLE_DEBUG__
static void debug_serial_in(int id, int c)
{
	if(id == HX_DEBUG_SERIAL_INPUT)
	{
		hx_uart_putc(UART_DEBUG_PORT,c);
	}
}
#endif
#endif
int hx_uart_atc_rxclr(int id)
{
	int res = -1;
	if(id>=UART_PORTS_NUM || id<0)
		return -2;
	
	HX_BRD_UART_DIS_INTR(id);
	volatile struct UART_CTX_T *p_uart = &g_uart[id];
	res = p_uart->rx_pos - p_uart->read_pos;
	p_uart->rx_pos = 0;
	p_uart->read_pos = 0;
	HX_BRD_UART_EN_INTR(id);  
	
	return res;
}
int hx_uart_getc_noblock(int id,int *c)
{
	int res = -1;
	
	if(id>=UART_PORTS_NUM || id<0)
		return -2;
	
	HX_BRD_UART_DIS_INTR(id);
	
	volatile struct UART_CTX_T *p_uart = &g_uart[id];
	
	if(p_uart->rx_pos>0)
	{
		if(p_uart->read_pos<p_uart->rx_pos)
		{
			*c = (int)(p_uart->rx_buffer[p_uart->read_pos]);
			p_uart->read_pos ++;
			res = 0;
		}
		else
		{
			p_uart->rx_pos = 0;
			p_uart->read_pos = 0;
		}
	}
	
	HX_BRD_UART_EN_INTR(id);  
	
	#ifdef HX_DEBUG_SERIAL_INPUT
	debug_serial_in(id,*c);
	#endif
	return res;
}
int hx_uart_getc_block(int id)
{
	int res;
	int c;
	for(;;){
		res = hx_uart_getc_noblock(id,&c);
		if(res>=0)
			break;
	}
	return c;
}
int hx_uart_getc_timeout(int id, int *c, int timeout)
{
	int res;
	hx_do_timeout(timeout){
		res = hx_uart_getc_noblock(id,c);
		if(res>=0)
			break;
	}
	return res;
}

//not call ctype.h ,use self is ok
static int hx_isprint(int c)	
{
	return c>=0x20 && c<127;
}
/*
	retrun >=0 is has \r\n
	<0 is null
*/
int hx_uart_gets_noblock(int id, char *buff, int buff_size)
{
	//#define MAX_CHARS_LINE				(128)
	static char g_gets_buf[MAX_CHARS_LINE];
	static int g_gets_count=0;
	
	int res;
	int c;
	while(1){	
		if(g_gets_count>=MAX_CHARS_LINE)
			g_gets_count = 0;
		res = hx_uart_getc_noblock(id,&c);
		if(res)
			return -4;
		//PRINT_Log("%c",(char)c);
		
		if(c=='\n' || c=='\r'){		//endl

			if(g_gets_count>0){
				if(g_gets_count<buff_size){
					memcpy(buff,g_gets_buf,g_gets_count);
					res = g_gets_count;
				}else{
					memcpy(buff,g_gets_buf,buff_size);
					res = g_gets_count;
				}
				buff[g_gets_count] = 0;
				g_gets_count = 0;
			}else{
				res = g_gets_count;
				buff[0] = 0;
				g_gets_count = 0;
			}
			return res;
		}else{
			if(!hx_isprint(c))
				continue;
			if(g_gets_count<(MAX_CHARS_LINE-1)){
				g_gets_buf[g_gets_count] = c;
				g_gets_count ++;
			}
			continue;
		}
	}
}
void hx_uart_gets_block(int id, char *buff, int buff_size)
{
	int res;
	for(;;){
		res = hx_uart_gets_noblock(id,buff,buff_size);
		if(res>=0)
			break;
	}
}
int hx_uart_gets_timeout(int id, char *buff, int buff_size,int timeout)
{
	int res;
	hx_do_timeout(timeout){
		res = hx_uart_gets_noblock(id,buff,buff_size);
		if(res>=0)
			break;
	}
	return res;
}

static void wait_last_trans_complete(int id)
{
	int pos,wr;
	do{
		HX_BRD_UART_DIS_INTR(id); 
		pos = g_uart[id].tx_pos;
		wr = g_uart[id].tx_write_pos;
		HX_BRD_UART_EN_INTR(id);  
	}while(wr&&(pos!=wr));
	
}

void UART_TX_BYTE(int id)
{
	g_uart[id].tx_pos ++;
	if(g_uart[id].tx_write_pos && 
		(g_uart[id].tx_pos<g_uart[id].tx_write_pos))
	{
		HX_BRD_UART_TX_START_BYTE(id,g_uart[id].tx_buffer[g_uart[id].tx_pos]);
	}else{
		g_uart[id].tx_write_pos = 0;
		HX_BRD_UART_TX_END_SESSION(id);
	}
}
#ifdef HX_DEBUG_SERIAL_OUTPUT
#ifdef __HX_ENABLE_DEBUG__
static void debug_serial_out(int id, const char *data, int len)
{
	if(id == HX_DEBUG_SERIAL_INPUT)
	{
		hx_uart_send(UART_DEBUG_PORT,data,len);
		hx_uart_puts(UART_DEBUG_PORT,"");
	}
}
#endif
#endif

void hx_uart_send(int id, const char *data, int len)
{
	#ifdef HX_DEBUG_SERIAL_OUTPUT
	debug_serial_out(id,data,len);
	#endif
	//check len big than buff
	int n = len;
	if(n==0){
		wait_last_trans_complete(id);
		return;
	}
	while(n>g_uart[id].tx_buf_size){
		wait_last_trans_complete(id);
		//start a new session
		memcpy((void*)(g_uart[id].tx_buffer),data,g_uart[id].tx_buf_size);
		g_uart[id].tx_write_pos = g_uart[id].tx_buf_size;
		g_uart[id].tx_pos = 0;
		HX_BRD_UART_TX_START_BYTE(id,g_uart[id].tx_buffer[0]);
		n -= g_uart[id].tx_buf_size;
	}
	wait_last_trans_complete(id);
	//start a new session
	memcpy((void*)(g_uart[id].tx_buffer),data,n);
	g_uart[id].tx_write_pos = n;
	g_uart[id].tx_pos = 0;
	HX_BRD_UART_TX_START_BYTE(id,g_uart[id].tx_buffer[0]);
/*	
	const char *p = data;
	int n = len;
	while(n-->0){
		//uart_putc(id, (int)*p++);
		
		UART_TX_BYTE(id, (int)*p++);
	}
	*/
}
void hx_uart_putc(int id, int c)
{
	char _c = (char)c;
	hx_uart_send(id,&_c,1);
}
void hx_uart_put(int id, const char *s)
{
	hx_uart_send(id,s,strlen((const char *)s));
}
void hx_uart_puts(int id, const char *s)
{
	hx_uart_put(id,s);
	hx_uart_put(id,"\r\n");
}
#ifdef __HX_ENABLE_DEBUG__
extern int g_enable_debug_output;
int HX_DBG_ENABLE(int en)
{
	int res = g_enable_debug_output;
	g_enable_debug_output = en;
	return res;
}
#endif
int hx_uart_printf(int port_nr, const char *format, ...)
{
#ifdef __HX_ENABLE_DEBUG__
	if(port_nr==UART_DEBUG_PORT && g_enable_debug_output==0) 
		return 0;
#endif
	
	int res;
	//use data or stack!
    //static  	
	char  buffer[VSPRINTF_BUFF_SIZE];
     
	va_list  vArgs;
    va_start(vArgs, format);
    res = vsprintf((char *)buffer, (char const *)format, vArgs);
    va_end(vArgs);
    hx_uart_put(port_nr, buffer);
	hx_uart_send(port_nr,NULL,0);
	return res;
}
int hx_uart_init(int port_nr, int bps)
{
	return HX_BRD_UART_REOPEN(port_nr,bps);
}