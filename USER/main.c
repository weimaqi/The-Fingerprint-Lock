#include <stm32f4xx_gpio.h>
#include <stm32f4xx_rcc.h>
#include "as608.h"
#include "delay.h"
#include "usart.h"
#include "usart2.h"
#include "usart3.h"
#include "timer.h"
#include "usmart.h"  
#include "malloc.h" 
#include "esp8266.h"
#include "exti.h"
#include "beep.h"
#include "led.h"
#include "ov2640.h"
#include "dcmi.h"
/***********************************************************/

#define usart2_baund  57600//´®¿Ú2²¨ÌØÂÊ£¬¸ù¾İÖ¸ÎÆÄ£¿é²¨ÌØÂÊ¸ü¸Ä
#define usart3_baund  19200//´®¿Ú3
SysPara AS608Para;//Ö¸ÎÆÄ£¿éAS608²ÎÊı
u16 ValidN;//Ä£¿éÄÚÓĞĞ§Ö¸ÎÆ¸öÊı
u8 netpro=1;	//ÍøÂçÄ£Ê½
u8 ipbuf[20]="192.168.43.149";//IPµØÖ·
//u8 HEADER[] = {
//  0x42,0x4d,0x36,0x58,0x02,0,0,0,0,0,0x36,0,0,0,0x28,0,
//  0,0,0x28,0,0,0,0x28,0,0,0,0x01,0,0x10,0,0,0,
//  0,0,0,0x58,0x02,0,0x23,0x2e,0,0,0x23,0x2e,0,0,0,0,
//  0,0,0,0,0,0
// };
u8 ov2640_mode=0;						//¹¤×÷Ä£Ê½:0,RGB565Ä£Ê½;1,JPEGÄ£Ê½

#define jpeg_buf_size 31*64  			//¶¨ÒåJPEGÊı¾İ»º´æjpeg_bufµÄ´óĞ¡(*4×Ö½Ú)
__align(4) u32 jpeg_buf[jpeg_buf_size];	//JPEGÊı¾İ»º´æbuf
volatile u32 jpeg_data_len=0; 			//bufÖĞµÄJPEGÓĞĞ§Êı¾İ³¤¶È 
volatile u8 jpeg_data_ok=0;				//JPEGÊı¾İ²É¼¯Íê³É±êÖ¾ 
										//0,Êı¾İÃ»ÓĞ²É¼¯Íê;
										//1,Êı¾İ²É¼¯ÍêÁË,µ«ÊÇ»¹Ã»´¦Àí;
										//2,Êı¾İÒÑ¾­´¦ÀíÍê³ÉÁË,¿ÉÒÔ¿ªÊ¼ÏÂÒ»Ö¡½ÓÊÕ
//JPEG³ß´çÖ§³ÖÁĞ±í
const u16 jpeg_img_size_tbl[][2]=
{
	176,144,	//QCIF
	80,60,	//QQVGA 160,120
	352,288,	//CIF
	320,240,	//QVGA
	640,480,	//VGA
	800,600,	//SVGA
	1024,768,	//XGA
	1280,1024,	//SXGA
	1600,1200,	//UXGA
}; 
const u8*EFFECTS_TBL[7]={"Normal","Negative","B&W","Redish","Greenish","Bluish","Antique"};	//7ÖÖÌØĞ§ 
const u8*JPEG_SIZE_TBL[9]={"QCIF","QQVGA","CIF","QVGA","VGA","SVGA","XGA","SXGA","UXGA"};	//JPEGÍ¼Æ¬ 9ÖÖ³ß´ç 


/***********************************************************/

/***********************************************************/


//´¦ÀíJPEGÊı¾İ
//µ±²É¼¯ÍêÒ»Ö¡JPEGÊı¾İºó,µ÷ÓÃ´Ëº¯Êı,ÇĞ»»JPEG BUF.¿ªÊ¼ÏÂÒ»Ö¡²É¼¯.
void jpeg_data_process(void)
{
	if(ov2640_mode)//Ö»ÓĞÔÚJPEG¸ñÊ½ÏÂ,²ÅĞèÒª×ö´¦Àí.
	{
		if(jpeg_data_ok==0)	//jpegÊı¾İ»¹Î´²É¼¯Íê?
		{	
			DMA_Cmd(DMA2_Stream1, DISABLE);//Í£Ö¹µ±Ç°´«Êä 
			while (DMA_GetCmdStatus(DMA2_Stream1) != DISABLE){}//µÈ´ıDMA2_Stream1¿ÉÅäÖÃ  
			jpeg_data_len=jpeg_buf_size-DMA_GetCurrDataCounter(DMA2_Stream1);//µÃµ½´Ë´ÎÊı¾İ´«ÊäµÄ³¤¶È
				
			jpeg_data_ok=1; 				//±ê¼ÇJPEGÊı¾İ²É¼¯Íê°´³É,µÈ´ıÆäËûº¯Êı´¦Àí
		}
		if(jpeg_data_ok==2)	//ÉÏÒ»´ÎµÄjpegÊı¾İÒÑ¾­±»´¦ÀíÁË
		{
			DMA2_Stream1->NDTR=jpeg_buf_size;	
			DMA_SetCurrDataCounter(DMA2_Stream1,jpeg_buf_size);//´«Êä³¤¶ÈÎªjpeg_buf_size*4×Ö½Ú
			DMA_Cmd(DMA2_Stream1, ENABLE);			//ÖØĞÂ´«Êä
			jpeg_data_ok=0;						//±ê¼ÇÊı¾İÎ´²É¼¯
		}
	}
} 
//JPEG²âÊÔ
//JPEGÊı¾İ,Í¨¹ı´®¿Ú2·¢ËÍ¸øµçÄÔ.
void jpeg_test(void)
{
	u32 i; 
	u8 *p;
	u8 key;
	u8 Mark;
	u8 effect=0,saturation=2,contrast=2;
	u8 size=3;		//Ä¬ÈÏÊÇQVGA 320*240³ß´ç
	u8 msgbuf[15];	//ÏûÏ¢»º´æÇø 
	LCD_Clear(WHITE);
  POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 JPEG Mode");
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//¶Ô±È¶È
	LCD_ShowString(30,120,200,16,16,"KEY1:Saturation"); 		//É«²Ê±¥ºÍ¶È
	LCD_ShowString(30,140,200,16,16,"KEY2:Effects"); 			//ÌØĞ§ 
	LCD_ShowString(30,160,200,16,16,"KEY_UP:Size");				//·Ö±æÂÊÉèÖÃ 
	sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
	LCD_ShowString(30,180,200,16,16,msgbuf);					//ÏÔÊ¾µ±Ç°JPEG·Ö±æÂÊ
	
 	OV2640_JPEG_Mode();		//JPEGÄ£Ê½
	My_DCMI_Init();			//DCMIÅäÖÃ
	DCMI_DMA_Init((u32)&jpeg_buf,jpeg_buf_size,DMA_MemoryDataSize_Word,DMA_MemoryInc_Enable);//DCMI DMAÅäÖÃ   
	OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//ÉèÖÃÊä³ö³ß´ç 
	DCMI_Start(); 		//Æô¶¯´«Êä
	while(1)
	{
		if(jpeg_data_ok==1)	//ÒÑ¾­²É¼¯ÍêÒ»Ö¡Í¼ÏñÁË
		{  
			Mark = 0;
			p=(u8*)jpeg_buf;
			LCD_ShowString(30,210,210,16,16,"Sending JPEG data..."); //ÌáÊ¾ÕıÔÚ´«ÊäÊı¾İ
			for(i=0;i<jpeg_data_len*4;i++)		//dma´«Êä1´ÎµÈÓÚ4×Ö½Ú,ËùÒÔ³ËÒÔ4.
			{
				if(p[i]==0xFF && p[i+1]==0xD8){
					Mark = 1;
				}
				if(Mark){
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);	//Ñ­»··¢ËÍ,Ö±µ½·¢ËÍÍê±Ï  		
					USART_SendData(USART3,p[i]); 
				}
				key=KEY_Scan(0); 
				if(key)break;
			} 
			if(key)	//ÓĞ°´¼ü°´ÏÂ,ĞèÒª´¦Àí
			{  
				LCD_ShowString(30,210,210,16,16,"Quit Sending data   ");//ÌáÊ¾ÍË³öÊı¾İ´«Êä
				switch(key)
				{				    
					case KEY0_PRES:	//¶Ô±È¶ÈÉèÖÃ
						contrast++;
						if(contrast>4)contrast=0;
						OV2640_Contrast(contrast);
						sprintf((char*)msgbuf,"Contrast:%d",(signed char)contrast-2);
						break;
					case KEY1_PRES:	//±¥ºÍ¶ÈSaturation
						saturation++;
						if(saturation>4)saturation=0;
						OV2640_Color_Saturation(saturation);
						sprintf((char*)msgbuf,"Saturation:%d",(signed char)saturation-2);
						break;
					case KEY2_PRES:	//ÌØĞ§ÉèÖÃ				 
						effect++;
						if(effect>6)effect=0;
						OV2640_Special_Effects(effect);//ÉèÖÃÌØĞ§
						sprintf((char*)msgbuf,"%s",EFFECTS_TBL[effect]);
						break;
					case WKUP_PRES:	//JPEGÊä³ö³ß´çÉèÖÃ   
						size++;  
						if(size>8)size=0;   
						OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//ÉèÖÃÊä³ö³ß´ç  
						sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
						break;
				}
				LCD_Fill(30,180,239,190+16,WHITE);
				LCD_ShowString(30,180,210,16,16,msgbuf);//ÏÔÊ¾ÌáÊ¾ÄÚÈİ
				delay_ms(800); 				  
			}else LCD_ShowString(30,210,210,16,16,"Send data complete!!");//ÌáÊ¾´«Êä½áÊøÉèÖÃ 
			jpeg_data_ok=2;	//±ê¼ÇjpegÊı¾İ´¦ÀíÍêÁË,¿ÉÒÔÈÃDMAÈ¥²É¼¯ÏÂÒ»Ö¡ÁË.
		}		
	}    
} 
//RGB565²âÊÔ
//RGBÊı¾İÖ±½ÓÏÔÊ¾ÔÚLCDÉÏÃæ
void rgb565_test(void)
{ 
	u8 key;
	u8 effect=0,saturation=2,contrast=2;
	u8 scale=1;		//Ä¬ÈÏÊÇÈ«³ß´çËõ·Å
	u8 msgbuf[15];	//ÏûÏ¢»º´æÇø 
	LCD_Clear(WHITE);
    POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 RGB565 Mode");
	
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//¶Ô±È¶È
	LCD_ShowString(30,130,200,16,16,"KEY1:Saturation"); 		//É«²Ê±¥ºÍ¶È
	LCD_ShowString(30,150,200,16,16,"KEY2:Effects"); 			//ÌØĞ§ 
	LCD_ShowString(30,170,200,16,16,"KEY_UP:FullSize/Scale");	//1:1³ß´ç(ÏÔÊ¾ÕæÊµ³ß´ç)/È«³ß´çËõ·Å
	
	OV2640_RGB565_Mode();	//RGB565Ä£Ê½
	My_DCMI_Init();			//DCMIÅäÖÃ
	DCMI_DMA_Init((u32)&LCD->LCD_RAM,1,DMA_MemoryDataSize_HalfWord,DMA_MemoryInc_Disable);//DCMI DMAÅäÖÃ  
 	OV2640_OutSize_Set(lcddev.width,lcddev.height); 
	DCMI_Start(); 		//Æô¶¯´«Êä
	while(1)
	{ 
		key=KEY_Scan(0); 
		if(key)
		{ 
			DCMI_Stop(); //Í£Ö¹ÏÔÊ¾
			switch(key)
			{				    
				case KEY0_PRES:	//¶Ô±È¶ÈÉèÖÃ
					contrast++;
					if(contrast>4)contrast=0;
					OV2640_Contrast(contrast);
					sprintf((char*)msgbuf,"Contrast:%d",(signed char)contrast-2);
					break;
				case KEY1_PRES:	//±¥ºÍ¶ÈSaturation
					saturation++;
					if(saturation>4)saturation=0;
					OV2640_Color_Saturation(saturation);
					sprintf((char*)msgbuf,"Saturation:%d",(signed char)saturation-2);
					break;
				case KEY2_PRES:	//ÌØĞ§ÉèÖÃ				 
					effect++;
					if(effect>6)effect=0;
					OV2640_Special_Effects(effect);//ÉèÖÃÌØĞ§
					sprintf((char*)msgbuf,"%s",EFFECTS_TBL[effect]);
					break;
				case WKUP_PRES:	//1:1³ß´ç(ÏÔÊ¾ÕæÊµ³ß´ç)/Ëõ·Å	    
					scale=!scale;  
					if(scale==0)
					{
						OV2640_ImageWin_Set((1600-lcddev.width)/2,(1200-lcddev.height)/2,lcddev.width,lcddev.height);//1:1ÕæÊµ³ß´ç
						OV2640_OutSize_Set(lcddev.width,lcddev.height); 
						sprintf((char*)msgbuf,"Full Size 1:1");
					}else 
					{
						OV2640_ImageWin_Set(0,0,1600,1200);				//È«³ß´çËõ·Å
						OV2640_OutSize_Set(lcddev.width,lcddev.height); 
						sprintf((char*)msgbuf,"Scale");
					}
					break;
			}
			LCD_ShowString(30,50,210,16,16,msgbuf);//ÏÔÊ¾ÌáÊ¾ÄÚÈİ
			delay_ms(800); 
			DCMI_Start();//ÖØĞÂ¿ªÊ¼´«Êä
		} 
		delay_ms(10);		
	}    
} 


//void Esp8266Init(){
//		int key;
//		u8* p=mymalloc(SRAMIN,32);							//ÉêÇë32×Ö½ÚÄÚ´æ
//		atk_8266_send_cmd("AT+CWMODE=1","OK",50);		//ÉèÖÃWIFI STAÄ£Ê½
//		atk_8266_send_cmd("AT+RST","OK",20);		//DHCP·şÎñÆ÷¹Ø±Õ(½öAPÄ£Ê½ÓĞĞ§) 
//		delay_ms(1000);         //ÑÓÊ±3SµÈ´ıÖØÆô³É¹¦
//		delay_ms(1000);
//		delay_ms(1000);
//		delay_ms(1000);
//		atk_8266_send_cmd("AT+CWJAP=\"ALALA\",\"WANGTIEJU\"","WIFI GOT IP",300);
//		delay_ms(1000);
//		netpro = 1;	//Ñ¡ÔñÍøÂçÄ£Ê½
//		if(netpro&0X01)     //TCP Client    Í¸´«Ä£Ê½²âÊÔ
//		{
//			OLED_Clear();
//			OLED_ShowString(0,0,"ATK-ESP WIFI-STA"); 
//			atk_8266_send_cmd("AT+CIPMUX=0","OK",20);   //0£ºµ¥Á¬½Ó£¬1£º¶àÁ¬½Ó
//			delay_ms(1000);
//			sprintf((char*)p,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);    //ÅäÖÃÄ¿±êTCP·şÎñÆ÷
//			atk_8266_send_cmd(p,"OK",200);
//			delay_ms(1000);
///*			while(atk_8266_send_cmd(p,"OK",200))
//			{
//					OLED_Clear();
//					OLED_ShowString(0,0,"TCP CONNECT FAILED"); //Á¬½ÓÊ§°Ü	 
//			}	*/
//			atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //´«ÊäÄ£Ê½Îª£ºÍ¸´«		
//			atk_8266_send_cmd("AT+CIPSEND","OK",200);		
////			OLED_ShowString(3,0,USART3_RX_BUF);			
//		}
//		myfree(SRAMIN,p);
//}



void KeyBoardIni(){     //¾ØÕó¼üÅÌ³õÊ¼»¯
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 ;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_Init(GPIOF,&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin= GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 ;
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_Init(GPIOF,&GPIO_InitStructure);
}


int KeyBoardScan(){
	GPIOF->BSRRL=GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 ;
	if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_4) | GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_5) | GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_6) | GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_7)){
			delay_ms(100);
		if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_4) | GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_5) | GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_6) | GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_7)){
			GPIOF->BSRRL= GPIO_Pin_0;       //ÅĞÊÇ²»ÊÇµÚÒ»ĞĞ
			GPIOF->BSRRH= GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 ;
			if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_4)){
				return 0;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_5)){
				return 1;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_6)){
				return 2;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_7)){
				return 3;
			}
			GPIOF->BSRRL= GPIO_Pin_1;       //ÅĞÊÇ²»ÊÇµÚ¶şĞĞ
			GPIOF->BSRRH= GPIO_Pin_0 | GPIO_Pin_2 | GPIO_Pin_3 ;
			if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_4)){
				return 4;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_5)){
				return 5;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_6)){
				return 6;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_7)){
				return 7;
			}
			GPIOF->BSRRL= GPIO_Pin_2;       //ÅĞÊÇ²»ÊÇµÚÈıĞĞ
			GPIOF->BSRRH= GPIO_Pin_1 | GPIO_Pin_0 | GPIO_Pin_3 ;
			if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_4)){
				return 8;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_5)){
				return 9;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_6)){
				return 10;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_7)){
				return 11;
			}
			GPIOF->BSRRL= GPIO_Pin_3;       //ÅĞÊÇ²»ÊÇµÚËÄĞĞ
			GPIOF->BSRRH= GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_0 ;
			if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_4)){
				return 12;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_5)){
				return 13;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_6)){
				return 14;
			}
			else if(GPIO_ReadInputDataBit(GPIOF,GPIO_Pin_7)){
				return 15;
			}
		}
	}
	return -1;
}


void ShowErrMessage(u8 ensure)
{
	//LCD_Fill(0,120,lcddev.width,160,WHITE);
	LCD_ShowString(30,250,200,16,16,(u8*)EnsureMessage(ensure));
}


void Del_FR(void) //É¾³ıÖ¸ÎÆ
{
	u8  ensure;
	u16 num;
	LCD_Fill(0,100,lcddev.width,160,WHITE);
	LCD_Clear(WHITE);
  POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"É¾³ıÖ¸ÎÆ");
	LCD_ShowString(30,70,200,16,16,"ÇëÊäÈëÖ¸ÎÆID°´Enter·¢ËÍ");
	LCD_ShowString(30,90,200,16,16,"0=< ID <=299");
	delay_ms(50);
//	AS608_load_keyboard(0,170,(u8**)kbd_delFR);
//	num=GET_NUM();//»ñÈ¡·µ»ØµÄÊıÖµ
	if(num==0xFFFF)
		goto MENU ; //·µ»ØÖ÷Ò³Ãæ
	else if(num==0xFF00)
		ensure=PS_Empty();//Çå¿ÕÖ¸ÎÆ¿â
	else 
		ensure=PS_DeletChar(num,1);//É¾³ıµ¥¸öÖ¸ÎÆ
	if(ensure==0)
	{
		LCD_Fill(0,120,lcddev.width,160,WHITE);
		LCD_ShowString(30,110,200,16,16,"É¾³ıÖ¸ÎÆ³É¹¦");
	}
  else
		ShowErrMessage(ensure);	
	delay_ms(1200);
	ensure=PS_ValidTempleteNum(&ValidN);//¶Á¿âÖ¸ÎÆ¸öÊı È·ÈÏ×Ö
                                      //Êä³öÈ·ÈÏÇé¿ö
	LCD_ShowNum(56,80,AS608Para.PS_max-ValidN,3,16);
MENU:	
	LCD_Fill(0,100,lcddev.width,160,WHITE);
	delay_ms(50);
//	AS608_load_keyboard(0,170,(u8**)kbd_menu);
}



void Add_FR(void)
{
	u8 i,ensure ,processnum=0;
	u16 ID;
	while(1)
	{
		switch (processnum)
		{
			case 0:
				i++;
//				LCD_Fill(0,100,lcddev.width,160,WHITE);
				LCD_ShowString(30,50,200,16,16,"Press please");
				ensure=PS_GetImage();
				if(ensure==0x00) 
				{
					BEEP=1;
					ensure=PS_GenChar(CharBuffer1);//Éú³ÉÌØÕ÷
					BEEP=0;
					if(ensure==0x00)
					{
						LCD_Fill(0,120,lcddev.width,160,WHITE);
						LCD_ShowString(30,70,200,16,16,"The fingerprint is normal.");
						i=0;
						processnum=1;//Ìøµ½µÚ¶ş²½						
					}else ShowErrMessage(ensure);				
				}else ShowErrMessage(ensure);						
				break;
			
			case 1:
				i++;
				LCD_Clear(WHITE);
				LCD_ShowString(30,90,200,16,16,"Press again.");
				ensure=PS_GetImage();
				if(ensure==0x00) 
				{
					BEEP=1;
					ensure=PS_GenChar(CharBuffer2);//Éú³ÉÌØÕ÷
					BEEP=0;
					if(ensure==0x00)
					{
						LCD_Clear(WHITE);
						LCD_ShowString(30,90,200,16,16,"The fingerprint is normal.");
						i=0;
						processnum=2;//Ìøµ½µÚÈı²½
					}else ShowErrMessage(ensure);	
				}else ShowErrMessage(ensure);		
				break;

			case 2:
				LCD_Clear(WHITE);
				LCD_ShowString(30,90,200,16,16,"Compare the prints");
				ensure=PS_Match();
				if(ensure==0x00) 
				{
					LCD_Clear(WHITE);
					LCD_ShowString(30,90,200,16,16,"Sucess,they are the same.");
					processnum=3;//Ìøµ½µÚËÄ²½
				}
				else 
				{
					LCD_Clear(WHITE);
					LCD_ShowString(30,90,200,16,16,"Failed,try again.");
					ShowErrMessage(ensure);
					i=0;
					processnum=0;//Ìø»ØµÚÒ»²½		
				}
				delay_ms(1200);
				break;

			case 3:
				LCD_Clear(WHITE);
				LCD_ShowString(30,90,200,16,16,"Generating model.");
				ensure=PS_RegModel();
				if(ensure==0x00) 
				{
					LCD_Clear(WHITE);
					LCD_ShowString(30,90,200,16,16,"Generated sucessfully.");
					processnum=4;//Ìøµ½µÚÎå²½
				}else {processnum=0;ShowErrMessage(ensure);}
				delay_ms(1200);
				break;
				
			case 4:	
				LCD_Clear(WHITE);
				LCD_ShowString(30,90,200,16,16,"Input id please.");
				LCD_ShowString(30,110,200,16,16,"0=< ID <=299");
				do{
					ID=KeyBoardScan();
				}
				while(!(ID<AS608Para.PS_max));//ÊäÈëID±ØĞëĞ¡ÓÚÄ£¿éÈİÁ¿×î´óµÄÊıÖµ
				ensure=PS_StoreChar(CharBuffer2,ID);//´¢´æÄ£°å
				if(ensure==0x00) 
				{			
					LCD_Clear(WHITE);
					LCD_ShowString(30,130,200,16,16,"Sucess finally.");
					PS_ValidTempleteNum(&ValidN);//¶Á¿âÖ¸ÎÆ¸öÊı
					LCD_ShowNum(56,80,AS608Para.PS_max-ValidN,3,16);//ÏÔÊ¾Ê£ÓàÖ¸ÎÆÈİÁ¿
					delay_ms(1500);
					LCD_Fill(0,100,240,160,WHITE);
					return ;
				}else {processnum=0;ShowErrMessage(ensure);}					
				break;				
		}
		delay_ms(400);
		if(i==5)//³¬¹ı5´ÎÃ»ÓĞ°´ÊÖÖ¸ÔòÍË³ö
		{
			LCD_Fill(0,100,lcddev.width,160,WHITE);
			break;	
		}				
	}
	LCD_Clear(WHITE);
}

//Ë¢Ö¸ÎÆ
int press_FR(void)
{
	SearchResult seach;
	u8 ensure,i=0;
	int key=-1;
	char *str,pw[]={0,0,0,0,0};
	str=mymalloc(SRAMIN,100);
	ensure=PS_GetImage();
	LCD_Clear(WHITE);
	LCD_ShowString(30,50,210,16,16,"Input by keyboard please");
	LCD_ShowString(30,70,210,16,16,"1:check fingerprints to get in.");
	LCD_ShowString(30,90,210,16,16,"2:check password to get in");
	LCD_ShowString(30,110,210,16,16,"3:Quit");
	while(key!=1 && key!=2 && key!=3) key=KeyBoardScan();
	if(key == 1){
		if(ensure==0x00)//»ñÈ¡Í¼Ïñ³É¹¦ 
		{	
			BEEP=1;//´ò¿ª·äÃùÆ÷	
			ensure=PS_GenChar(CharBuffer1);
			if(ensure==0x00) //Éú³ÉÌØÕ÷³É¹¦
			{		
				BEEP=0;//¹Ø±Õ·äÃùÆ÷	
				ensure=PS_HighSpeedSearch(CharBuffer1,0,AS608Para.PS_max,&seach);
				if(ensure==0x00)//ËÑË÷³É¹¦
				{				
	//				LCD_Fill(0,100,lcddev.width,160,WHITE);
					LCD_ShowString(30,50,200,16,16,"Right fingerprint");
					sprintf(str,"Person,ID:%d  Score:%d",seach.pageID,seach.mathscore);
					LCD_ShowString(30,70,200,16,16,str);
					myfree(SRAMIN,str);
					return 1;
				}
				else 
					ShowErrMessage(ensure);
			}
			else
				ShowErrMessage(ensure);
		 BEEP=0;//¹Ø±Õ·äÃùÆ÷
		 delay_ms(600);
		 LCD_Fill(0,100,lcddev.width,160,WHITE);
		myfree(SRAMIN,str);
		return 0;
		}
		myfree(SRAMIN,str);
		return 0;
	}
	else if(key == 2){
		LCD_Clear(WHITE);
		while(i<4){
			delay_ms(600);
			LCD_ShowString(30,70,210,16,16,password);
			sprintf(str,"Please input the %dth number",i);
			LCD_ShowString(30,90,210,16,16,str);
			key = -1;
			i++;
			while(key==-1) key=KeyBoardScan();
			pw[i-1]=key;
		}
		if(pw[0]+48==password[0] && pw[0]+48==password[0] && pw[0]+48==password[0] && pw[0]+48==password[0]){
			LCD_ShowString(30,110,210,16,16,"The pw is right");
			return 1;
		}
		else{
			LCD_ShowString(30,110,210,16,16,"The pw is wrong");
			return 0;
		}
	}
	else if(key == 3){
		return 0;
	}
}

/***********************************************************/

int main(void)  
{  
/******************************/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//ÉèÖÃÏµÍ³ÖĞ¶ÏÓÅÏÈ¼¶·Ö×é2
	u8 ensure;
	u32 i,j;
	char *strr;
	SearchResult seach;
	int key;      //°´¼ü
	char str[30];
	u8 test[]={1,2,3,0,4,5};
	delay_init(168);  	//³õÊ¼»¯ÑÓÊ±º¯Êı
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOF|RCC_AHB1Periph_GPIOG|RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOD, ENABLE);	 //Ê¹ÄÜÏà¹Ø¶Ë¿ÚÊ±ÖÓ
	KeyBoardIni();
	uart_init(115200);	//³õÊ¼»¯´®¿Ú1²¨ÌØÂÊÎª115200£¬ÓÃÓÚÖ§³ÖUSMART
	LED_Init();					//³õÊ¼»¯LED 
 	LCD_Init();					//LCD³õÊ¼»¯  
 	KEY_Init();					//°´¼ü³õÊ¼»¯ 
 	POINT_COLOR=RED;//ÉèÖÃ×ÖÌåÎªºìÉ« 
	LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");	
	LCD_ShowString(30,70,200,16,16,"OV2640 TEST");	
	LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,110,200,16,16,"2014/5/14");
	usart2_init(usart2_baund);//³õÊ¼»¯´®¿Ú2,ÓÃÓÚÓëÖ¸ÎÆÄ£¿éÍ¨Ñ¶
	usart3_init(usart3_baund);
	PS_StaGPIO_Init();	//³õÊ¼»¯FR¶Á×´Ì¬Òı½Å
	usmart_dev.init(168);		//³õÊ¼»¯USMART
	my_mem_init(SRAMIN);		//³õÊ¼»¯ÄÚ²¿ÄÚ´æ³Ø 
	my_mem_init(SRAMCCM);		//³õÊ¼»¯CCMÄÚ´æ
	TIM14_PWM_Init(20000-1,84-1);
	TIM14->CCR1=1100;
	W25QXX_Init();				//³õÊ¼»¯W25Q128
//	Esp8266Init();
	delay_ms(2000);
	sprintf(str,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);
	atk_8266_send_cmd(str,"OK",200);
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //´«ÊäÄ£Ê½Îª£ºÍ¸´«		
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPSEND","OK",200);
	while(OV2640_Init())//³õÊ¼»¯OV2640
	{
		LCD_ShowString(30,130,240,16,16,"OV2640 ERR");
		delay_ms(200);
	    LCD_Fill(30,130,239,170,WHITE);
		delay_ms(200);
	}
	LCD_ShowString(30,130,200,16,16,"OV2640 OK");  
	ov2640_mode=1;
	TIM6_Int_Init(10000,7199);			//10Khz¼ÆÊıÆµÂÊ,1ÃëÖÓÖĞ¶Ï	
	TIM2_Int_Init(10000,7199);
	EXTIX_Init();						//Ê¹ÄÜ¶¨Ê±Æ÷²¶»ñ								  
/*****************************/
	while(PS_HandShake(&AS608Addr)){//ÓëAS608Ä£¿éÎÕÊÖ
		delay_ms(400);
		delay_ms(800);
	}
	delay_ms(1000);
	ensure=PS_ValidTempleteNum(&ValidN);//¶Á¿âÖ¸ÎÆ¸öÊı
	if(ensure!=0x00){
		ShowErrMessage(ensure);//ÏÔÊ¾È·ÈÏÂë´íÎóĞÅÏ¢
	}
	ensure=PS_ReadSysPara(&AS608Para);  //¶Á²ÎÊı
	if(ensure==0x00)
	{
		sprintf(str,"capacity:%d level:%d",AS608Para.PS_max-ValidN,AS608Para.PS_level);
		LCD_ShowString(30,130,200,16,16,str);
	}
	else{
		ShowErrMessage(ensure);
	}
	PS_Sta=0;
	delay_ms(100);
	for(j=0;j<6;j++){
		while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);  //µÈ´ıÉÏ´Î´«ÊäÍê³É 
		USART_SendData(USART3,test[j]); 	 //·¢ËÍÊı¾İµ½´®¿Ú3 
	}
	
	/****jepg_test****/
	u8 *p;
	u8 Mark;
	u8 size=1;		//Ä¬ÈÏÊÇQVGA 320*240³ß´ç
	u8 msgbuf[15];	//ÏûÏ¢»º´æÇø 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 JPEG Mode");
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//¶Ô±È¶È
	LCD_ShowString(30,120,200,16,16,"KEY1:Saturation"); 		//É«²Ê±¥ºÍ¶È
	LCD_ShowString(30,140,200,16,16,"KEY2:Effects"); 			//ÌØĞ§ 
	LCD_ShowString(30,160,200,16,16,"KEY_UP:Size");				//·Ö±æÂÊÉèÖÃ 
	sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
	LCD_ShowString(30,180,200,16,16,msgbuf);					//ÏÔÊ¾µ±Ç°JPEG·Ö±æÂÊ
	
 	OV2640_JPEG_Mode();		//JPEGÄ£Ê½
	My_DCMI_Init();			//DCMIÅäÖÃ
	DCMI_DMA_Init((u32)&jpeg_buf,jpeg_buf_size,DMA_MemoryDataSize_Word,DMA_MemoryInc_Enable);//DCMI DMAÅäÖÃ   
	OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//ÉèÖÃÊä³ö³ß´ç 
	DCMI_Start(); 		//Æô¶¯´«Êä
	LCD_Clear(WHITE);
	/****************/
	while(1){
		LCD_ShowString(30,50,210,16,16,"Press zero to add fingerprint");
		LCD_ShowString(30,70,210,16,16,"Press the checker to unlock");
		if(jpeg_data_ok==1)	//ÒÑ¾­²É¼¯ÍêÒ»Ö¡Í¼ÏñÁË
		{  
			Mark = 0;
			p=(u8*)jpeg_buf;
			LCD_ShowString(30,210,210,16,16,"Sending JPEG data..."); //ÌáÊ¾ÕıÔÚ´«ÊäÊı¾İ
			for(i=0;i<jpeg_data_len*4;i++)		//dma´«Êä1´            ÎµÈÓÚ4×Ö½Ú,ËùÒÔ³ËÒÔ4.
			{
				if(p[i]==0xFF && p[i+1]==0xD8){
					Mark = 1;
				}
				if(Mark){
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);	//Ñ­»··¢ËÍ,Ö±µ½·¢ËÍÍê±Ï  		
					USART_SendData(USART3,p[i]); 
				}
				if(p[i-1]==0xFF && p[i]==0xD9){
					Mark = 0;
				}
			} 
			LCD_ShowString(30,210,210,16,16,"Send data complete!!");//ÌáÊ¾´«Êä½áÊøÉèÖÃ 
			jpeg_data_ok=2;	//±ê¼ÇjpegÊı¾İ´¦ÀíÍêÁË,¿ÉÒÔÈÃDMAÈ¥²É¼¯ÏÂÒ»Ö¡ÁË.
		}
		
		key=-1;
		key=KeyBoardScan();
   	if(key==0){
			delay_ms(600);
			key=press_FR();
			LCD_Clear(WHITE);
			if(key){
				Add_FR();
			}
		}
		else if(key==2){
			LCD_ShowString(30,210,210,16,16,"You are here.");
		}
		if(PS_Sta){
				ensure=PS_GetImage();
				if(ensure==0x00)//»ñÈ¡Í¼Ïñ³É¹¦ 
				{	
				BEEP=1;//´ò¿ª·äÃùÆ÷	
				ensure=PS_GenChar(CharBuffer1);
				if(ensure==0x00) //Éú³ÉÌØÕ÷³É¹¦
				{		
					BEEP=0;//¹Ø±Õ·äÃùÆ÷	
					ensure=PS_HighSpeedSearch(CharBuffer1,0,AS608Para.PS_max,&seach);
					if(ensure==0x00)//ËÑË÷³É¹¦
					{				
		//				LCD_Fill(0,100,lcddev.width,160,WHITE);
						LCD_ShowString(30,50,200,16,16,"Right fingerprint");
						strr=mymalloc(SRAMIN,2000);
						sprintf(str,"Person,ID:%d  Score:%d",seach.pageID,seach.mathscore);
						LCD_ShowString(30,70,200,16,16,str);
						myfree(SRAMIN,str);
						LCD_Clear(WHITE);
						TIM14->CCR1=1900;
						delay_ms(3000);
						TIM14->CCR1=1100;
					}
					else 
						ShowErrMessage(ensure);
				}
				else
					ShowErrMessage(ensure);
			 BEEP=0;//¹Ø±Õ·äÃùÆ÷
			 delay_ms(600);
			 LCD_Fill(0,100,lcddev.width,160,WHITE);
			}
		}
	}
}
