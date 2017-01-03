#include "hx_utils.h"
#include "string.h"
#include "stdio.h"
#include "hxd_param.h"
#include "time.h"
static volatile unsigned *g_pclock = 0;
extern int brd_init(void);
int hx_utils_init(void)
{
	int res;
	//register and open stdin/out/err, 
	// please retarget stdio before this stage
	// than debug io can be use.
	hx_device_init();	
	
	extern const PARAM_DEV_T brd_params;
	// init clock for tick count
	hx_register_params(&brd_params);
	HX_DEV params_d;
	res = hx_open("param","",&params_d);
	if(res==0){
		char buff[24] = "clock";
		res = hx_read(&params_d,buff,5);
		if(res==0){
			g_pclock = ((PARAM_DEV_T*)params_d.pdev)->tbl[params_d.offset].data;
		}
	}
	hx_close(&params_d);
	
	//register and init board devices
	res = brd_init();
	if(res){
		HX_DBG_PRINTLN("board init error");
	}
	
	//startup term app
	res = hxt_term_init();
	if(res)
		return -3;
	return 0;
}

uint32_t hx_get_tick_count(void)
{
	if(g_pclock)
		return *g_pclock;
	HX_DBG_PRINTLN("ERROR: call %s",__FUNCTION__);
	while(1);
}

int bcd2int(unsigned char bcd)
{
	unsigned int tmp = bcd;
	return (0x0Fu&(tmp>>4))*10+(0x0Fu&(tmp));
}

unsigned char int2bcd(int d)
{
	return (unsigned char)(0x0F0u&((d/10)<<4))+(0x0Fu&(d%10));
}
// "\x12\x34" => "1234"
char* hx_dumphex(const void* bin,int bin_len,void *asc)
{
	unsigned int tmp;
	const unsigned char *_bin = bin;
	unsigned char *_asc = asc;
	for(size_t i=0;i<bin_len;i++)
	{
		tmp = (0xFu&(_bin[i]>>4));
		_asc[(i<<1)] = (unsigned char) (tmp<0xA?tmp+'0':tmp+'A'-0xA);
		tmp = (0xFu&(_bin[i]>>0));
		_asc[(i<<1)+1] = (unsigned char) (tmp<0xA?tmp+'0':tmp+'A'-0xA);
	}
	return asc;
}
// "\x12\x34" => "1234"
char* hx_dumphex2str(const void* bin,int bin_len,void *asc)
{
	hx_dumphex(bin,bin_len,asc);
	((char*)asc)[bin_len<<1] = 0;
	return asc;
}
// "1234" => "\12\x34"
char* hx_hexcode2bin(const void *hexcode, int len, void *bin)
{
    unsigned int tmp;
    int c,i;
    unsigned char *t = (unsigned char*)bin;
	const unsigned char *s = (const unsigned char *)hexcode;
	for(i=0;i<len;i+=2){
		tmp = s[i];
        c = 0x0F & (tmp>='A'?tmp-'A'+0x0A:tmp-'0');
        c<<=4;
        tmp = s[i+1];
        c += 0x0F & (tmp>='A'?tmp-'A'+0x0A:tmp-'0');
        *t++ = c;
	}
    return bin;
}

char *hx_strtrim2(char *p, const char *trim_list)
{
	char *s_beg = p;
	char *s_end = s_beg + strlen(s_beg) - 1;
	while(strchr(trim_list,*s_end) && s_end > s_beg)
		*s_end-- = 0;	
	while(strchr(trim_list,*s_beg) && s_beg < s_end)
		*s_beg++ = 0;
	//*p = s_beg;
	return s_beg;
}
char *hx_strtrim(char *s)
{
	return hx_strtrim2(s,"\t\r ");
}


int hx_str2value(const char *s, const char *value_type, void *vres)
{
    char fmt[10] = {0};
    int count = -1;
    char split_chars[5] = {0};
	if(!s)
		return -1;
    char str[128] = {0};
	strncpy(str,s,127);
    int res = sscanf(value_type,"%s %d %s",fmt,&count,split_chars);
    if(res<=0)
        return -1;
    if(strcmp(fmt,"bcd")==0 || strcmp(fmt,"bin")==0) {
		if(str[0]==0)
			return -2;
        if(count<=0)
            return -3;
		int l = strlen(str);
		if(l>count){
			hx_dbge(0,"%s:bcd too long,%u>%u\n",
				__FUNCTION__,l,count);
			l=count;
		}
        hx_hexcode2bin(str,l,vres);
        return 0;
    } else if(strcmp(fmt,"asc")==0) {
        if(count<0)
            return -3;
        if(count>0){
			if(str[0]==0)
				return -2;
			memcpy(vres,str,count);
        }else
            strcpy(vres,str);
        return 0;
    } else if(strcmp(fmt,"msb")==0) {
		if(str[0]==0)
			return -2;
        uint64_t v;
        sscanf(str,"%llu",&v);
        if(count==2)
            HX_MSB_W2B(v,vres);
        else if(count==4)
            HX_MSB_DW2B(v,vres);
        else if(count==8)
            HX_MSB_QW2B(v,vres);
        else
            return -3;
        return 0;
    } else if(strcmp(fmt,"lsb")==0) {
		if(str[0]==0)
			return -2;
        uint64_t v;
        sscanf(str,"%llu",&v);
        if(count==2)
            HX_LSB_W2B(v,vres);
        else if(count==4)
            HX_LSB_DW2B(v,vres);
        else if(count==8)
            HX_LSB_QW2B(v,vres);
        else
            return -3;
        return 0;
    } else if(fmt[0]=='%') {
        //use printf/scan format
        if(res==1) {
            count = 1;
            split_chars[0] = 0;
            //sscanf(str,fmt,vres);
        } else if(res >= 2) {
            if(count<=0)
                return -3;
            if(count==1)	//count ==1, no need split char
                split_chars[0] = 0;
            else if(split_chars[0]==0) {
                //array big than 0, neet split char, default is '.'
                strcpy(split_chars,".");
            }
        } else {
            return -5;
        }
        int size = 0;
        if((strcmp(fmt,"%s")==0)){
			count = 1;
            size = 0;
		}else if(strstr(fmt,"d")||strstr(fmt,"i")||strstr(fmt,"u")) {
            if(strstr(fmt,"hh"))
                size = 1;
            else if(strstr(fmt,"h"))
                size = 2;
            else if(strstr(fmt,"ll"))
                size = 8;
            else
                size = 4;
        } else {
            //...
        }
        if(count>1) {
            char *cvres = vres;
            char *p = strtok(str,split_chars);
            int i = 0;
            for(i=0; i<count-1; i++) {
                sscanf(p,fmt,cvres);
                if(size==0) {
                    int ll = strlen(cvres);
                    cvres[ll] = ' ';
                    cvres += ll+1;
                } else {
                    cvres += size;
                }
                p = strtok(NULL,split_chars);
            }
            sscanf(p,fmt,cvres);
        } else {
            sscanf(str,fmt,vres);
        }
        return 0;

    } else {
        return -9;
    }
}
int hx_value2str(const void* value,const char *format,
                 char *sres)
{
    strcpy(sres,"error");
    char fmt[10] = {0};
    int count = -1;
    char split_chars[5] = {0};
    if(!value)
        return -2;
    int res = sscanf(format,"%s %d %s",fmt,&count,split_chars);
    if(res<=0)
        return -1;
    if(strcmp(fmt,"bcd")==0||strcmp(fmt,"bin")==0) {
        if(count<=0)
            return -3;
        hx_dumphex2str(value,count,sres);
        return 0;
    } else if(strcmp(fmt,"asc")==0) {
        if(count<0)
            count = 0;
        if(count>0) {
            memcpy(sres,value,count);
            sres[count] = 0;
        } else
            strcpy(sres,value);
        return 0;
    } else if(strcmp(fmt,"msb")==0) {
        uint64_t v;
        if(count==2)
            v = HX_MSB_B2W(value);
        else if(count==4)
            v = HX_MSB_B2DW(value);
        else if(count==8)
            v = HX_MSB_B2QW(value);
        else
            return -3;
        sprintf(sres,"%llu",v);
        return 0;
    } else if(strcmp(fmt,"lsb")==0) {
        uint64_t v;
        if(count==2)
            v = HX_LSB_B2W(value);
        else if(count==4)
            v = HX_LSB_B2DW(value);
        else if(count==8)
            v = HX_LSB_B2QW(value);
        else
            return -3;
        sprintf(sres,"%llu",v);
        return 0;
    } else if(fmt[0]=='%') {
        //use printf/scan format
        if(res==1) {
            count = 1;
            split_chars[0] = 0;
            //sprintf(sres,fmt,*(uint32_t*)value);
        } else if(res >= 2) {
            if(count<=0)
                return -3;
            if(count==1)	//count ==1, no need split char
                split_chars[0] = 0;
            else if(split_chars[0]==0) {
                //array big than 0, neet split char, default is '.'
                strcpy(split_chars,".");
            }
        } else {
            return -5;
        }
        int size = 0;
        if((strcmp(fmt,"%s")==0))
            size = 0;
        else if(strstr(fmt,"d")||strstr(fmt,"i")||strstr(fmt,"u")) {
            if(strstr(fmt,"hh"))
                size = 1;
            else if(strstr(fmt,"h"))
                size = 2;
            else if(strstr(fmt,"ll"))
                size = 8;
            else
                size = 4;
        } else {
            //...
        }
        char *p = sres;
        const void *v = value;
        for(int i=0; i<count; i++) {
            if(size==0)
                p += sprintf(p,fmt,v);
            else if(size==1)
                p += sprintf(p,fmt,*(uint8_t*)v);
            else if(size==2)
                p += sprintf(p,fmt,*(uint16_t*)v);
            else if(size==4)
                p += sprintf(p,fmt,*(uint32_t*)v);
            else if(size==4)
                p += sprintf(p,fmt,*(uint64_t*)v);
            else
                return -3;
            if(i<count-1)
                p += sprintf(p,split_chars);

            v = (char*)v + size;
        }
        return 0;
    } else {
        return -9;
    }
}

//==========================================================
// uint8_t ARRAY <==>  uint16_t(2Bytes) uint32_t(4Bytes) QWORD(8Bytes)
// MSB = Big Endian, LSB = Littie Endian

uint16_t HX_LSB_B2W(const void *data)
{
	const uint8_t *d = data;
    uint16_t res;
    res = (uint16_t)(d[1]);
    res <<= 8;
    res += (uint16_t)(d[0]);
    return res;
}
uint16_t HX_MSB_B2W(const void *data)
{
	const uint8_t *d = data;
    uint16_t res;
    res = (uint16_t)(d[0]);
    res <<= 8;
    res += (uint16_t)(d[1]);
    return res;
}
uint64_t HX_MSB_B2QW(const void *data)
{
    const uint8_t *d = data;
    uint64_t res;
	res = (uint64_t)d[0];
    res <<=8;
    res += (uint64_t) d[1];
    res <<=8;
    res += (uint64_t) d[2];
    res <<=8;
    res += (uint64_t) d[3];
	res <<=8;
    res += (uint64_t) d[4];
    res <<=8;
    res += (uint64_t) d[5];
    res <<=8;
    res += (uint64_t) d[6];
    res <<=8;
    res += (uint64_t) d[7];
    return res;
}
uint32_t HX_MSB_B2DW(const void *data)
{
	const uint8_t *d = data;
    uint32_t res;
    res = (uint32_t)d[0];
    res <<=8;
    res += (uint32_t) d[1];
    res <<=8;
    res += (uint32_t) d[2];
    res <<=8;
    res += (uint32_t) d[3];
    return res;
}
uint64_t HX_LSB_B2QW(const void *data)
{
	const uint8_t *d = data;
    uint64_t res;
	res = (uint64_t)d[7];
    res <<=8;
    res += (uint64_t) d[6];
    res <<=8;
    res += (uint64_t) d[5];
    res <<=8;
    res += (uint64_t) d[4];
	res <<=8;
    res += (uint64_t) d[3];
    res <<=8;
    res += (uint64_t) d[2];
    res <<=8;
    res += (uint64_t) d[1];
    res <<=8;
    res += (uint64_t) d[0];
    return res;
}
uint32_t HX_LSB_B2DW(const void *data)
{
	const uint8_t *d = data;
    uint32_t res;
    res = (uint32_t)d[3];
    res <<=8;
    res += (uint32_t) d[2];
    res <<=8;
    res += (uint32_t) d[1];
    res <<=8;
    res += (uint32_t) d[0];
    return res;
}
void HX_LSB_W2B(uint16_t v,void *p)
{
	uint8_t *_p = p;
    _p[0] =  0xFF & (v>>0);
    _p[1] =  0xFF & (v>>8);
}
void HX_MSB_W2B(uint16_t v,void *p)
{
    uint8_t *_p = p;
    _p[0] =  0xFF & (v>>8);
    _p[1] =  0xFF & (v>>0);
}
void HX_MSB_DW2B(uint32_t v,void *p)
{
    uint8_t *_p = p;
    _p[0] =  0xFF & (v>>24);
    _p[1] =  0xFF & (v>>16);
    _p[2] =  0xFF & (v>>8);
    _p[3] =  0xFF & (v>>0);
}
void HX_MSB_QW2B(uint64_t v,void *p)
{
	uint8_t *_p = p;
    _p[0] =  0xFF & (v>>56);
    _p[1] =  0xFF & (v>>48);
    _p[2] =  0xFF & (v>>40);
    _p[3] =  0xFF & (v>>32);
    _p[4] =  0xFF & (v>>24);
    _p[5] =  0xFF & (v>>16);
    _p[6] =  0xFF & (v>>8);
    _p[7] =  0xFF & (v>>0);
}
void HX_LSB_DW2B(uint32_t v,void *p)
{
    uint8_t *_p = p;
    _p[3] =  0xFF & (v>>24);
    _p[2] =  0xFF & (v>>16);
    _p[1] =  0xFF & (v>>8);
    _p[0] =  0xFF & (v>>0);
}
void HX_LSB_QW2B(uint64_t v,void *p)
{
	uint8_t *_p = p;
    _p[7] =  0xFF & (v>>56);
    _p[6] =  0xFF & (v>>48);
    _p[5] =  0xFF & (v>>40);
    _p[4] =  0xFF & (v>>32);
    _p[3] =  0xFF & (v>>24);
    _p[2] =  0xFF & (v>>16);
    _p[1] =  0xFF & (v>>8);
    _p[0] =  0xFF & (v>>0);
}

int hx_delay_ms(int ms)
{
	hx_do_timeout(ms);
	return 0;
}
int hx_check_timeout3(int *tlist, int tid, uint32_t timeout)
{
    if(tlist[tid] <=0) {
		tlist[tid] = hx_get_tick_count();
        return -1;
    }
    if(timeout == 0)
        timeout = 1;
    if(hx_get_tick_count() < (tlist[tid]+timeout))
        return 0;
	tlist[tid] = hx_get_tick_count();
    return -1;
}
int hx_check_timeout2(int *last,uint32_t timeout)
{
    if(last <=0) {
		*last = hx_get_tick_count();
        return -1;
    }
    if(timeout == 0)
        timeout = 1;
    if(hx_get_tick_count() < (*last+timeout))
        return 0;
	*last = hx_get_tick_count();
    return -1;
}
int hx_check_timeout(int last,uint32_t timeout)
{
    if(last <=0) {
        return -1;
    }
    if(timeout == 0)
        timeout = 1;
    if(hx_get_tick_count() < (last+timeout))
        return 0;
    return -1;
}

unsigned char make_bcc(unsigned char init, const void *data, int len)
{
	unsigned char res = init;
	const unsigned char *p = (const unsigned char*)data;
	for(int i=0;i<len;i++){
		res ^= p[i];
	}
	return res;
}
unsigned char make_bcc2(const void *data, int len)
{
	return make_bcc(0,data,len);
}


void* pk_fill(void *to, int len, int d)
{
    char *res = memset(to,d,len);
    return res + len;
}
void* pk_add(void *to, int len, const void *from)
{
    char *res = (char*)memcpy(to,from,len);
    return res + len;
}
void* pk_get(void*from,int len,void* to)
{
    char *res = (char*)from;
    memcpy(to,from,len);
    return  res + len;
}


//------------------------------------------------------------------------------
int ymd2days(int y,int m,int d)
{
	const int tbl[] = {
		31,
		31+28,
		31+28+31,
		31+28+31+30,
		31+28+31+30+31,
		31+28+31+30+31+30,
		31+28+31+30+31+30+31,
		31+28+31+30+31+30+31+31,
		31+28+31+30+31+30+31+31+30,
		31+28+31+30+31+30+31+31+30+31,
		31+28+31+30+31+30+31+31+30+31+30,
		31+28+31+30+31+30+31+31+30+31+30+31,
	};
	//if(y>1970)
		y-=1970;
	int f = y%4?0:1;
	return (y * (365+f)) + (tbl[m]+(m>2?f:0)) + d;

}
long long ymdhms2sec(int y,int m,int d,int H,int M,int S)
{
	int ds = ymd2days(y,m,d);
	return ds*3600*24+H*3600+M*60+S;
}

int ymdbcd2days(uint8_t *yyyymmdd)
{
	return ymd2days(
			bcd2int(yyyymmdd[0])*100+bcd2int(yyyymmdd[1]),
			bcd2int(yyyymmdd[2]),
			bcd2int(yyyymmdd[3])
	);
}
long long ymdhmsbcd2sec(uint8_t *yyyymmddHHMMSS)
{
	long long ds = ymdbcd2days(yyyymmddHHMMSS);
	return ds*3600*24+ymdhms2sec(0,0,0,
			bcd2int(yyyymmddHHMMSS[4]),
			bcd2int(yyyymmddHHMMSS[5]),
			bcd2int(yyyymmddHHMMSS[6])
	);
}

/*
#include "lcd.h"
#define LCD_MAX_ROW				(8)
#define LCD_MAX_COL				(30)
const unsigned char g_font_ascii_8x16[] = {

0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x38,0xFC,0xFC,0x38,0x00,0x00,0x00,0x00,0x00,0x0D,0x0D,0x00,0x00,0x00,
0x00,0x0E,0x1E,0x00,0x00,0x1E,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x20,0xF8,0xF8,0x20,0xF8,0xF8,0x20,0x00,0x02,0x0F,0x0F,0x02,0x0F,0x0F,0x02,0x00,
0x38,0x7C,0x44,0x47,0x47,0xCC,0x98,0x00,0x03,0x06,0x04,0x1C,0x1C,0x07,0x03,0x00,
0x30,0x30,0x00,0x80,0xC0,0x60,0x30,0x00,0x0C,0x06,0x03,0x01,0x00,0x0C,0x0C,0x00,
0x80,0xD8,0x7C,0xE4,0xBC,0xD8,0x40,0x00,0x07,0x0F,0x08,0x08,0x07,0x0F,0x08,0x00,
0x00,0x10,0x1E,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xF0,0xF8,0x0C,0x04,0x00,0x00,0x00,0x00,0x03,0x07,0x0C,0x08,0x00,0x00,
0x00,0x00,0x04,0x0C,0xF8,0xF0,0x00,0x00,0x00,0x00,0x08,0x0C,0x07,0x03,0x00,0x00,
0x80,0xA0,0xE0,0xC0,0xC0,0xE0,0xA0,0x80,0x00,0x02,0x03,0x01,0x01,0x03,0x02,0x00,
0x00,0x80,0x80,0xE0,0xE0,0x80,0x80,0x00,0x00,0x00,0x00,0x03,0x03,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x1E,0x0E,0x00,0x00,0x00,
0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x00,
0x00,0x00,0x00,0x80,0xC0,0x60,0x30,0x00,0x0C,0x06,0x03,0x01,0x00,0x00,0x00,0x00,
0xF8,0xFC,0x04,0xC4,0x24,0xFC,0xF8,0x00,0x07,0x0F,0x09,0x08,0x08,0x0F,0x07,0x00,
0x00,0x10,0x18,0xFC,0xFC,0x00,0x00,0x00,0x00,0x08,0x08,0x0F,0x0F,0x08,0x08,0x00,
0x08,0x0C,0x84,0xC4,0x64,0x3C,0x18,0x00,0x0E,0x0F,0x09,0x08,0x08,0x0C,0x0C,0x00,
0x08,0x0C,0x44,0x44,0x44,0xFC,0xB8,0x00,0x04,0x0C,0x08,0x08,0x08,0x0F,0x07,0x00,
0xC0,0xE0,0xB0,0x98,0xFC,0xFC,0x80,0x00,0x00,0x00,0x00,0x08,0x0F,0x0F,0x08,0x00,
0x7C,0x7C,0x44,0x44,0xC4,0xC4,0x84,0x00,0x04,0x0C,0x08,0x08,0x08,0x0F,0x07,0x00,
0xF0,0xF8,0x4C,0x44,0x44,0xC0,0x80,0x00,0x07,0x0F,0x08,0x08,0x08,0x0F,0x07,0x00,
0x0C,0x0C,0x04,0x84,0xC4,0x7C,0x3C,0x00,0x00,0x00,0x0F,0x0F,0x00,0x00,0x00,0x00,
0xB8,0xFC,0x44,0x44,0x44,0xFC,0xB8,0x00,0x07,0x0F,0x08,0x08,0x08,0x0F,0x07,0x00,
0x38,0x7C,0x44,0x44,0x44,0xFC,0xF8,0x00,0x00,0x08,0x08,0x08,0x0C,0x07,0x03,0x00,
0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x06,0x00,0x00,0x00,
0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x08,0x0E,0x06,0x00,0x00,0x00,
0x00,0x80,0xC0,0x60,0x30,0x18,0x08,0x00,0x00,0x00,0x01,0x03,0x06,0x0C,0x08,0x00,
0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x00,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x00,
0x00,0x08,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x08,0x0C,0x06,0x03,0x01,0x00,0x00,
0x18,0x1C,0x04,0xC4,0xE4,0x3C,0x18,0x00,0x00,0x00,0x00,0x0D,0x0D,0x00,0x00,0x00,
0xF0,0xF8,0x08,0xC8,0xC8,0xF8,0xF0,0x00,0x07,0x0F,0x08,0x0B,0x0B,0x0B,0x01,0x00,
0xE0,0xF0,0x98,0x8C,0x98,0xF0,0xE0,0x00,0x0F,0x0F,0x00,0x00,0x00,0x0F,0x0F,0x00,
0x04,0xFC,0xFC,0x44,0x44,0xFC,0xB8,0x00,0x08,0x0F,0x0F,0x08,0x08,0x0F,0x07,0x00,
0xF0,0xF8,0x0C,0x04,0x04,0x0C,0x18,0x00,0x03,0x07,0x0C,0x08,0x08,0x0C,0x06,0x00,
0x04,0xFC,0xFC,0x04,0x0C,0xF8,0xF0,0x00,0x08,0x0F,0x0F,0x08,0x0C,0x07,0x03,0x00,
0x04,0xFC,0xFC,0x44,0xE4,0x0C,0x1C,0x00,0x08,0x0F,0x0F,0x08,0x08,0x0C,0x0E,0x00,
0x04,0xFC,0xFC,0x44,0xE4,0x0C,0x1C,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,0x00,0x00,
0xF0,0xF8,0x0C,0x84,0x84,0x8C,0x98,0x00,0x03,0x07,0x0C,0x08,0x08,0x07,0x0F,0x00,
0xFC,0xFC,0x40,0x40,0x40,0xFC,0xFC,0x00,0x0F,0x0F,0x00,0x00,0x00,0x0F,0x0F,0x00,
0x00,0x00,0x04,0xFC,0xFC,0x04,0x00,0x00,0x00,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,
0x00,0x00,0x00,0x04,0xFC,0xFC,0x04,0x00,0x07,0x0F,0x08,0x08,0x0F,0x07,0x00,0x00,
0x04,0xFC,0xFC,0xC0,0xF0,0x3C,0x0C,0x00,0x08,0x0F,0x0F,0x00,0x01,0x0F,0x0E,0x00,
0x04,0xFC,0xFC,0x04,0x00,0x00,0x00,0x00,0x08,0x0F,0x0F,0x08,0x08,0x0C,0x0E,0x00,
0xFC,0xFC,0x38,0x70,0x38,0xFC,0xFC,0x00,0x0F,0x0F,0x00,0x00,0x00,0x0F,0x0F,0x00,
0xFC,0xFC,0x38,0x70,0xE0,0xFC,0xFC,0x00,0x0F,0x0F,0x00,0x00,0x00,0x0F,0x0F,0x00,
0xF0,0xF8,0x0C,0x04,0x0C,0xF8,0xF0,0x00,0x03,0x07,0x0C,0x08,0x0C,0x07,0x03,0x00,
0x04,0xFC,0xFC,0x44,0x44,0x7C,0x38,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,0x00,0x00,
0xF8,0xFC,0x04,0x04,0x04,0xFC,0xF8,0x00,0x07,0x0F,0x08,0x0E,0x3C,0x3F,0x27,0x00,
0x04,0xFC,0xFC,0x44,0xC4,0xFC,0x38,0x00,0x08,0x0F,0x0F,0x00,0x00,0x0F,0x0F,0x00,
0x18,0x3C,0x64,0x44,0xC4,0x9C,0x18,0x00,0x06,0x0E,0x08,0x08,0x08,0x0F,0x07,0x00,
0x00,0x1C,0x0C,0xFC,0xFC,0x0C,0x1C,0x00,0x00,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,
0xFC,0xFC,0x00,0x00,0x00,0xFC,0xFC,0x00,0x07,0x0F,0x08,0x08,0x08,0x0F,0x07,0x00,
0xFC,0xFC,0x00,0x00,0x00,0xFC,0xFC,0x00,0x01,0x03,0x06,0x0C,0x06,0x03,0x01,0x00,
0xFC,0xFC,0x00,0x80,0x00,0xFC,0xFC,0x00,0x03,0x0F,0x0E,0x03,0x0E,0x0F,0x03,0x00,
0x0C,0x3C,0xF0,0xC0,0xF0,0x3C,0x0C,0x00,0x0C,0x0F,0x03,0x00,0x03,0x0F,0x0C,0x00,
0x00,0x3C,0x7C,0xC0,0xC0,0x7C,0x3C,0x00,0x00,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,
0x1C,0x0C,0x84,0xC4,0x64,0x3C,0x1C,0x00,0x0E,0x0F,0x09,0x08,0x08,0x0C,0x0E,0x00,
0x00,0x00,0xFC,0xFC,0x04,0x04,0x00,0x00,0x00,0x00,0x0F,0x0F,0x08,0x08,0x00,0x00,
0x38,0x70,0xE0,0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x03,0x07,0x0E,0x00,
0x00,0x00,0x04,0x04,0xFC,0xFC,0x00,0x00,0x00,0x00,0x08,0x08,0x0F,0x0F,0x00,0x00,
0x08,0x0C,0x06,0x03,0x06,0x0C,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
0x00,0x00,0x03,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xA0,0xA0,0xA0,0xE0,0xC0,0x00,0x00,0x07,0x0F,0x08,0x08,0x07,0x0F,0x08,0x00,
0x04,0xFC,0xFC,0x20,0x60,0xC0,0x80,0x00,0x08,0x0F,0x07,0x08,0x08,0x0F,0x07,0x00,
0xC0,0xE0,0x20,0x20,0x20,0x60,0x40,0x00,0x07,0x0F,0x08,0x08,0x08,0x0C,0x04,0x00,
0x80,0xC0,0x60,0x24,0xFC,0xFC,0x00,0x00,0x07,0x0F,0x08,0x08,0x07,0x0F,0x08,0x00,
0xC0,0xE0,0xA0,0xA0,0xA0,0xE0,0xC0,0x00,0x07,0x0F,0x08,0x08,0x08,0x0C,0x04,0x00,
0x40,0xF8,0xFC,0x44,0x0C,0x18,0x00,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,0x00,0x00,
0xC0,0xE0,0x20,0x20,0xC0,0xE0,0x20,0x00,0x27,0x6F,0x48,0x48,0x7F,0x3F,0x00,0x00,
0x04,0xFC,0xFC,0x40,0x20,0xE0,0xC0,0x00,0x08,0x0F,0x0F,0x00,0x00,0x0F,0x0F,0x00,
0x00,0x00,0x20,0xEC,0xEC,0x00,0x00,0x00,0x00,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,
0x00,0x00,0x00,0x00,0x20,0xEC,0xEC,0x00,0x00,0x30,0x70,0x40,0x40,0x7F,0x3F,0x00,
0x04,0xFC,0xFC,0x80,0xC0,0x60,0x20,0x00,0x08,0x0F,0x0F,0x01,0x03,0x0E,0x0C,0x00,
0x00,0x00,0x04,0xFC,0xFC,0x00,0x00,0x00,0x00,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,
0xE0,0xE0,0x60,0xC0,0x60,0xE0,0xC0,0x00,0x0F,0x0F,0x00,0x0F,0x00,0x0F,0x0F,0x00,
0x20,0xE0,0xC0,0x20,0x20,0xE0,0xC0,0x00,0x00,0x0F,0x0F,0x00,0x00,0x0F,0x0F,0x00,
0xC0,0xE0,0x20,0x20,0x20,0xE0,0xC0,0x00,0x07,0x0F,0x08,0x08,0x08,0x0F,0x07,0x00,
0x20,0xE0,0xC0,0x20,0x20,0xE0,0xC0,0x00,0x40,0x7F,0x7F,0x48,0x08,0x0F,0x07,0x00,
0xC0,0xE0,0x20,0x20,0xC0,0xE0,0x20,0x00,0x07,0x0F,0x08,0x48,0x7F,0x7F,0x40,0x00,
0x20,0xE0,0xC0,0x60,0x20,0x60,0xC0,0x00,0x08,0x0F,0x0F,0x08,0x00,0x00,0x00,0x00,
0x40,0xE0,0xA0,0x20,0x20,0x60,0x40,0x00,0x04,0x0C,0x09,0x09,0x0B,0x0E,0x04,0x00,
0x20,0x20,0xF8,0xFC,0x20,0x20,0x00,0x00,0x00,0x00,0x07,0x0F,0x08,0x0C,0x04,0x00,
0xE0,0xE0,0x00,0x00,0xE0,0xE0,0x00,0x00,0x07,0x0F,0x08,0x08,0x07,0x0F,0x08,0x00,
0x00,0xE0,0xE0,0x00,0x00,0xE0,0xE0,0x00,0x00,0x03,0x07,0x0C,0x0C,0x07,0x03,0x00,
0xE0,0xE0,0x00,0x00,0x00,0xE0,0xE0,0x00,0x07,0x0F,0x0C,0x07,0x0C,0x0F,0x07,0x00,
0x20,0x60,0xC0,0x80,0xC0,0x60,0x20,0x00,0x08,0x0C,0x07,0x03,0x07,0x0C,0x08,0x00,
0xE0,0xE0,0x00,0x00,0x00,0xE0,0xE0,0x00,0x47,0x4F,0x48,0x48,0x68,0x3F,0x1F,0x00,
0x60,0x60,0x20,0xA0,0xE0,0x60,0x20,0x00,0x0C,0x0E,0x0B,0x09,0x08,0x0C,0x0C,0x00,
0x00,0x40,0x40,0xF8,0xBC,0x04,0x04,0x00,0x00,0x00,0x00,0x07,0x0F,0x08,0x08,0x00,
0x00,0x00,0x00,0xBC,0xBC,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x00,0x00,
0x00,0x04,0x04,0xBC,0xF8,0x40,0x40,0x00,0x00,0x08,0x08,0x0F,0x07,0x00,0x00,0x00,
0x08,0x0C,0x04,0x0C,0x08,0x0C,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x80,0xC0,0x60,0x30,0x60,0xC0,0x80,0x00,0x07,0x07,0x04,0x04,0x04,0x07,0x07,0x00

};
extern void _lcd_draw_bmp(unsigned char col,unsigned char page,unsigned char width,unsigned char high,unsigned char *bmp,unsigned char mode,unsigned char ifmenu);
void hx_lcd_draw_bmp(
	int row,			//y in page
	int col,			//x in pixel
	int width,			//宽度单位像素
	int height,			//高度单位像素
	const void *bitmap,	//数据
	//int mode,			//0.擦除后绘图,直接绘图
	int inverse			//0.正常,1反色
)
{
	height /= 8;
	_lcd_draw_bmp(col,row,width,height,(void*)bitmap,inverse,0);//,mode);
}
static int lcd_draw_char(
	int col_in_pxl,
	int row_in_page,
	char c,
	int mode
		)
{
	 hx_lcd_draw_bmp(
		row_in_page,
		col_in_pxl,
		8,16,
		g_font_ascii_8x16+8*16/8*(c-' ')
	,1);
	return 0;
}
static int lcd_draw_chars(
	int col_in_pxl,
	int row_in_page,
	const void *data,
	int mode,
	int f,
	int cols_count_in_char_size
		)
{
//	int i = 0;
	const char *p = data;
	char c;
	for(int i=0;i<cols_count_in_char_size;i++){
		c = p[i];
		lcd_draw_char(col_in_pxl+8*i,row_in_page,c,mode);
	}
	return 0;
}
#define CHARICTER_WIDTH		(8)
void hx_lcd_disp_at_ex(
	int row,
	int col,
	const char *s,
	int align,		//-1左对齐 0居中 1右对齐
	int mode,		//0擦除对应行,1不查处
	int inverse		//0正常,1反色
)
{
	unsigned char _inverse = inverse ? 0 : 1;
	int n;//实际显示字符数
    int empty;	//显示区域字符数
    unsigned char buf[30];
    memset(buf,' ',30);

    if(row>=LCD_MAX_ROW || col>=LCD_MAX_COL)
        return ;

    n= strlen(s);

    if(align<0)
    {
        empty = LCD_MAX_COL - col;
        if(n>empty)
            n = empty;
        if(mode==0)	//清楚一整行
        {
            memcpy(buf+col,s,n);
            lcd_draw_chars(0,row*2,buf,_inverse,1,LCD_MAX_COL);
        }
        else
        {
            lcd_draw_chars(CHARICTER_WIDTH*col,row*2,(void*)s,_inverse,1,n);
        }
    }
    else if(align == 0)
    {
        empty = LCD_MAX_COL-col;
        if(empty>(LCD_MAX_COL/2))
            empty = LCD_MAX_COL - empty;
        empty <<=1;
        if(n>empty)
            n = empty;
        if(mode==0)	//清楚一整行
        {
            memcpy(buf+col-(n>>1),s,n);
            lcd_draw_chars(0,row*2,buf,_inverse,1,LCD_MAX_COL);
        }
        else
        {
            lcd_draw_chars(CHARICTER_WIDTH*(col-(n>>1)),row*2,(void*)s,_inverse,1,n);
        }
    }
    else
    {
        empty = LCD_MAX_COL - col;
        if(n>empty)
            n = empty;
        if(mode==0)	//清楚一整行
        {
            memcpy(buf+LCD_MAX_COL-n,s,n);
            lcd_draw_chars(0,row*2,buf,_inverse,1,LCD_MAX_COL);
        }
        else
        {
            lcd_draw_chars(CHARICTER_WIDTH*(LCD_MAX_COL-n),row*2,
				(void*)s,_inverse,1,n);
        }
    }
}
void hx_lcd_disp_at(int row,int col,const char *s,int align,int mode)
{
    hx_lcd_disp_at_ex(row,col,s,align,mode,0);
}

*/
