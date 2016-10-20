
#include "hx_atcmd.h"
#include "hx_utils.h"
#include "hx_serial.h"
#include "string.h"
#include "int.h"



/*
	arguements:
	at_tbl : define at cmd list and operation functions
	tbl_len : at cmd list size
	buff: recv data save here
	buff_size: buffer len ,max save chars
	msg: user defined data struct ,  usefor describ mes info

	return:
	normal return number is the current step.
	when all step continued , return sum of item of at_tbl.
*/
int atc_sequence_poll(
	const struct ATCMD_T *at_tbl, 
	int tbl_len,
	char *buff,
	int buff_size,
	void *msg
)
{
	int res;
	static int step = 0;
	if(step >= tbl_len)
		return tbl_len;
	static int is_send = 0;
	int tmout_at_once = at_tbl[step].tmout_at_once;
	int try_times = at_tbl[step].trytimes;
	static int tmof_last_state_change = -1;
	if(tmof_last_state_change == -1)
		tmof_last_state_change = hx_get_tick_count();
	
	if(!is_send){
		if(at_tbl[step].cmd){	//if has cmd than send it 
			//this is delay cmd ???
			if(try_times == -1 && tmout_at_once>0){
				if(hx_check_timeout2(&tmof_last_state_change,tmout_at_once)){
					step ++ ;
					is_send = 0;
				}
				return step;
			}else{
				hx_uart_printf(UART_AT_PORT,"%s\r\n",at_tbl[step].cmd);
				is_send = 1;
				HX_DBG_PRINT("ATCMD_TX: [%s]\r\n",at_tbl[step].cmd);
			}
		}else if(at_tbl[step].callback){	//has none, than callback
			memset(buff,0,buff_size);
			at_tbl[step].callback(step,AT_PUT,buff,NULL);
			hx_uart_printf(UART_AT_PORT,"%s\r\n",buff);
			is_send = 1;
			HX_DBG_PRINT("ATCMD_TX: [%s]\r\n",buff);
		}else{
			//no this stuation
			HX_DBG_PRINT("**error: ATCMD bad line:\r\n");
			HX_DBG_PRINT("\t step: %s\r\n", step);
			HX_DBG_PRINT("\t cmd: %s\r\n", at_tbl[step].cmd);
			HX_DBG_PRINT("\t want_res: %s\r\n", at_tbl[step].want_res);
			HX_DBG_PRINT("\t timeout_At_once: %d\r\n", at_tbl[step].tmout_at_once);
			HX_DBG_PRINT("\t trytimes: %d\r\n", at_tbl[step].trytimes);
			HX_DBG_PRINT("\t cmd: %d\r\n", at_tbl[step].callback);
		}
	}
	again:
	res = hx_uart_gets_noblock(UART_AT_PORT,buff,buff_size);
	if(res>=0){
		int get_ok = 0;
		HX_DBG_PRINT("\tRX: [%s]\r\n",buff);
		char *s = strtrim(buff,"\r\n ");
		if(at_tbl[step].want_res &&
			strcmp(buff,at_tbl[step].want_res)==0){
			get_ok = 1;
		}else if(at_tbl[step].callback){
			res = at_tbl[step].callback(step,AT_GET,s,msg);
			if(res == 0)
				get_ok = 1;
		}
		if(get_ok){
			step ++ ;
			is_send = 0;
			tmof_last_state_change = hx_get_tick_count();
			if(step == tbl_len){
				HX_DBG_PRINT("\tModule Init Complete\r\n");
				return tbl_len;		//到头了
			}else{
				return step-1;
			}
		}else{
			goto again;
		}
	}else{
		res = hx_check_timeout(tmof_last_state_change,tmout_at_once);
		if(res){
			tmof_last_state_change = hx_get_tick_count();
			if(--try_times<0){
				step = 0;
				is_send = 0;	//复位
				return 0;
			}else{
				is_send = 0;	//重发
			}
		}
	}
	return step;
}


