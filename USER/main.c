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

#define usart2_baund  57600//����2�����ʣ�����ָ��ģ�鲨���ʸ���
#define usart3_baund  19200//����3
SysPara AS608Para;//ָ��ģ��AS608����
u16 ValidN;//ģ������Чָ�Ƹ���
u8 netpro=1;	//����ģʽ
u8 ipbuf[20]="192.168.43.149";//IP��ַ
//u8 HEADER[] = {
//  0x42,0x4d,0x36,0x58,0x02,0,0,0,0,0,0x36,0,0,0,0x28,0,
//  0,0,0x28,0,0,0,0x28,0,0,0,0x01,0,0x10,0,0,0,
//  0,0,0,0x58,0x02,0,0x23,0x2e,0,0,0x23,0x2e,0,0,0,0,
//  0,0,0,0,0,0
// };
u8 ov2640_mode=0;						//����ģʽ:0,RGB565ģʽ;1,JPEGģʽ

#define jpeg_buf_size 31*64  			//����JPEG���ݻ���jpeg_buf�Ĵ�С(*4�ֽ�)
__align(4) u32 jpeg_buf[jpeg_buf_size];	//JPEG���ݻ���buf
volatile u32 jpeg_data_len=0; 			//buf�е�JPEG��Ч���ݳ��� 
volatile u8 jpeg_data_ok=0;				//JPEG���ݲɼ���ɱ�־ 
										//0,����û�вɼ���;
										//1,���ݲɼ�����,���ǻ�û����;
										//2,�����Ѿ����������,���Կ�ʼ��һ֡����
//JPEG�ߴ�֧���б�
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
const u8*EFFECTS_TBL[7]={"Normal","Negative","B&W","Redish","Greenish","Bluish","Antique"};	//7����Ч 
const u8*JPEG_SIZE_TBL[9]={"QCIF","QQVGA","CIF","QVGA","VGA","SVGA","XGA","SXGA","UXGA"};	//JPEGͼƬ 9�ֳߴ� 


/***********************************************************/

/***********************************************************/


//����JPEG����
//���ɼ���һ֡JPEG���ݺ�,���ô˺���,�л�JPEG BUF.��ʼ��һ֡�ɼ�.
void jpeg_data_process(void)
{
	if(ov2640_mode)//ֻ����JPEG��ʽ��,����Ҫ������.
	{
		if(jpeg_data_ok==0)	//jpeg���ݻ�δ�ɼ���?
		{	
			DMA_Cmd(DMA2_Stream1, DISABLE);//ֹͣ��ǰ���� 
			while (DMA_GetCmdStatus(DMA2_Stream1) != DISABLE){}//�ȴ�DMA2_Stream1������  
			jpeg_data_len=jpeg_buf_size-DMA_GetCurrDataCounter(DMA2_Stream1);//�õ��˴����ݴ���ĳ���
				
			jpeg_data_ok=1; 				//���JPEG���ݲɼ��갴��,�ȴ�������������
		}
		if(jpeg_data_ok==2)	//��һ�ε�jpeg�����Ѿ���������
		{
			DMA2_Stream1->NDTR=jpeg_buf_size;	
			DMA_SetCurrDataCounter(DMA2_Stream1,jpeg_buf_size);//���䳤��Ϊjpeg_buf_size*4�ֽ�
			DMA_Cmd(DMA2_Stream1, ENABLE);			//���´���
			jpeg_data_ok=0;						//�������δ�ɼ�
		}
	}
} 
//JPEG����
//JPEG����,ͨ������2���͸�����.
void jpeg_test(void)
{
	u32 i; 
	u8 *p;
	u8 key;
	u8 Mark;
	u8 effect=0,saturation=2,contrast=2;
	u8 size=3;		//Ĭ����QVGA 320*240�ߴ�
	u8 msgbuf[15];	//��Ϣ������ 
	LCD_Clear(WHITE);
  POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 JPEG Mode");
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//�Աȶ�
	LCD_ShowString(30,120,200,16,16,"KEY1:Saturation"); 		//ɫ�ʱ��Ͷ�
	LCD_ShowString(30,140,200,16,16,"KEY2:Effects"); 			//��Ч 
	LCD_ShowString(30,160,200,16,16,"KEY_UP:Size");				//�ֱ������� 
	sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
	LCD_ShowString(30,180,200,16,16,msgbuf);					//��ʾ��ǰJPEG�ֱ���
	
 	OV2640_JPEG_Mode();		//JPEGģʽ
	My_DCMI_Init();			//DCMI����
	DCMI_DMA_Init((u32)&jpeg_buf,jpeg_buf_size,DMA_MemoryDataSize_Word,DMA_MemoryInc_Enable);//DCMI DMA����   
	OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//��������ߴ� 
	DCMI_Start(); 		//��������
	while(1)
	{
		if(jpeg_data_ok==1)	//�Ѿ��ɼ���һ֡ͼ����
		{  
			Mark = 0;
			p=(u8*)jpeg_buf;
			LCD_ShowString(30,210,210,16,16,"Sending JPEG data..."); //��ʾ���ڴ�������
			for(i=0;i<jpeg_data_len*4;i++)		//dma����1�ε���4�ֽ�,���Գ���4.
			{
				if(p[i]==0xFF && p[i+1]==0xD8){
					Mark = 1;
				}
				if(Mark){
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);	//ѭ������,ֱ���������  		
					USART_SendData(USART3,p[i]); 
				}
				key=KEY_Scan(0); 
				if(key)break;
			} 
			if(key)	//�а�������,��Ҫ����
			{  
				LCD_ShowString(30,210,210,16,16,"Quit Sending data   ");//��ʾ�˳����ݴ���
				switch(key)
				{				    
					case KEY0_PRES:	//�Աȶ�����
						contrast++;
						if(contrast>4)contrast=0;
						OV2640_Contrast(contrast);
						sprintf((char*)msgbuf,"Contrast:%d",(signed char)contrast-2);
						break;
					case KEY1_PRES:	//���Ͷ�Saturation
						saturation++;
						if(saturation>4)saturation=0;
						OV2640_Color_Saturation(saturation);
						sprintf((char*)msgbuf,"Saturation:%d",(signed char)saturation-2);
						break;
					case KEY2_PRES:	//��Ч����				 
						effect++;
						if(effect>6)effect=0;
						OV2640_Special_Effects(effect);//������Ч
						sprintf((char*)msgbuf,"%s",EFFECTS_TBL[effect]);
						break;
					case WKUP_PRES:	//JPEG����ߴ�����   
						size++;  
						if(size>8)size=0;   
						OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//��������ߴ�  
						sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
						break;
				}
				LCD_Fill(30,180,239,190+16,WHITE);
				LCD_ShowString(30,180,210,16,16,msgbuf);//��ʾ��ʾ����
				delay_ms(800); 				  
			}else LCD_ShowString(30,210,210,16,16,"Send data complete!!");//��ʾ����������� 
			jpeg_data_ok=2;	//���jpeg���ݴ�������,������DMAȥ�ɼ���һ֡��.
		}		
	}    
} 
//RGB565����
//RGB����ֱ����ʾ��LCD����
void rgb565_test(void)
{ 
	u8 key;
	u8 effect=0,saturation=2,contrast=2;
	u8 scale=1;		//Ĭ����ȫ�ߴ�����
	u8 msgbuf[15];	//��Ϣ������ 
	LCD_Clear(WHITE);
    POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 RGB565 Mode");
	
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//�Աȶ�
	LCD_ShowString(30,130,200,16,16,"KEY1:Saturation"); 		//ɫ�ʱ��Ͷ�
	LCD_ShowString(30,150,200,16,16,"KEY2:Effects"); 			//��Ч 
	LCD_ShowString(30,170,200,16,16,"KEY_UP:FullSize/Scale");	//1:1�ߴ�(��ʾ��ʵ�ߴ�)/ȫ�ߴ�����
	
	OV2640_RGB565_Mode();	//RGB565ģʽ
	My_DCMI_Init();			//DCMI����
	DCMI_DMA_Init((u32)&LCD->LCD_RAM,1,DMA_MemoryDataSize_HalfWord,DMA_MemoryInc_Disable);//DCMI DMA����  
 	OV2640_OutSize_Set(lcddev.width,lcddev.height); 
	DCMI_Start(); 		//��������
	while(1)
	{ 
		key=KEY_Scan(0); 
		if(key)
		{ 
			DCMI_Stop(); //ֹͣ��ʾ
			switch(key)
			{				    
				case KEY0_PRES:	//�Աȶ�����
					contrast++;
					if(contrast>4)contrast=0;
					OV2640_Contrast(contrast);
					sprintf((char*)msgbuf,"Contrast:%d",(signed char)contrast-2);
					break;
				case KEY1_PRES:	//���Ͷ�Saturation
					saturation++;
					if(saturation>4)saturation=0;
					OV2640_Color_Saturation(saturation);
					sprintf((char*)msgbuf,"Saturation:%d",(signed char)saturation-2);
					break;
				case KEY2_PRES:	//��Ч����				 
					effect++;
					if(effect>6)effect=0;
					OV2640_Special_Effects(effect);//������Ч
					sprintf((char*)msgbuf,"%s",EFFECTS_TBL[effect]);
					break;
				case WKUP_PRES:	//1:1�ߴ�(��ʾ��ʵ�ߴ�)/����	    
					scale=!scale;  
					if(scale==0)
					{
						OV2640_ImageWin_Set((1600-lcddev.width)/2,(1200-lcddev.height)/2,lcddev.width,lcddev.height);//1:1��ʵ�ߴ�
						OV2640_OutSize_Set(lcddev.width,lcddev.height); 
						sprintf((char*)msgbuf,"Full Size 1:1");
					}else 
					{
						OV2640_ImageWin_Set(0,0,1600,1200);				//ȫ�ߴ�����
						OV2640_OutSize_Set(lcddev.width,lcddev.height); 
						sprintf((char*)msgbuf,"Scale");
					}
					break;
			}
			LCD_ShowString(30,50,210,16,16,msgbuf);//��ʾ��ʾ����
			delay_ms(800); 
			DCMI_Start();//���¿�ʼ����
		} 
		delay_ms(10);		
	}    
} 


//void Esp8266Init(){
//		int key;
//		u8* p=mymalloc(SRAMIN,32);							//����32�ֽ��ڴ�
//		atk_8266_send_cmd("AT+CWMODE=1","OK",50);		//����WIFI STAģʽ
//		atk_8266_send_cmd("AT+RST","OK",20);		//DHCP�������ر�(��APģʽ��Ч) 
//		delay_ms(1000);         //��ʱ3S�ȴ������ɹ�
//		delay_ms(1000);
//		delay_ms(1000);
//		delay_ms(1000);
//		atk_8266_send_cmd("AT+CWJAP=\"ALALA\",\"WANGTIEJU\"","WIFI GOT IP",300);
//		delay_ms(1000);
//		netpro = 1;	//ѡ������ģʽ
//		if(netpro&0X01)     //TCP Client    ͸��ģʽ����
//		{
//			OLED_Clear();
//			OLED_ShowString(0,0,"ATK-ESP WIFI-STA"); 
//			atk_8266_send_cmd("AT+CIPMUX=0","OK",20);   //0�������ӣ�1��������
//			delay_ms(1000);
//			sprintf((char*)p,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);    //����Ŀ��TCP������
//			atk_8266_send_cmd(p,"OK",200);
//			delay_ms(1000);
///*			while(atk_8266_send_cmd(p,"OK",200))
//			{
//					OLED_Clear();
//					OLED_ShowString(0,0,"TCP CONNECT FAILED"); //����ʧ��	 
//			}	*/
//			atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //����ģʽΪ��͸��		
//			atk_8266_send_cmd("AT+CIPSEND","OK",200);		
////			OLED_ShowString(3,0,USART3_RX_BUF);			
//		}
//		myfree(SRAMIN,p);
//}



void KeyBoardIni(){     //������̳�ʼ��
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
			GPIOF->BSRRL= GPIO_Pin_0;       //���ǲ��ǵ�һ��
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
			GPIOF->BSRRL= GPIO_Pin_1;       //���ǲ��ǵڶ���
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
			GPIOF->BSRRL= GPIO_Pin_2;       //���ǲ��ǵ�����
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
			GPIOF->BSRRL= GPIO_Pin_3;       //���ǲ��ǵ�����
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


void Del_FR(void) //ɾ��ָ��
{
	u8  ensure;
	u16 num;
	LCD_Fill(0,100,lcddev.width,160,WHITE);
	LCD_Clear(WHITE);
  POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"ɾ��ָ��");
	LCD_ShowString(30,70,200,16,16,"������ָ��ID��Enter����");
	LCD_ShowString(30,90,200,16,16,"0=< ID <=299");
	delay_ms(50);
//	AS608_load_keyboard(0,170,(u8**)kbd_delFR);
//	num=GET_NUM();//��ȡ���ص���ֵ
	if(num==0xFFFF)
		goto MENU ; //������ҳ��
	else if(num==0xFF00)
		ensure=PS_Empty();//���ָ�ƿ�
	else 
		ensure=PS_DeletChar(num,1);//ɾ������ָ��
	if(ensure==0)
	{
		LCD_Fill(0,120,lcddev.width,160,WHITE);
		LCD_ShowString(30,110,200,16,16,"ɾ��ָ�Ƴɹ�");
	}
  else
		ShowErrMessage(ensure);	
	delay_ms(1200);
	ensure=PS_ValidTempleteNum(&ValidN);//����ָ�Ƹ��� ȷ����
                                      //���ȷ�����
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
					ensure=PS_GenChar(CharBuffer1);//��������
					BEEP=0;
					if(ensure==0x00)
					{
						LCD_Fill(0,120,lcddev.width,160,WHITE);
						LCD_ShowString(30,70,200,16,16,"The fingerprint is normal.");
						i=0;
						processnum=1;//�����ڶ���						
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
					ensure=PS_GenChar(CharBuffer2);//��������
					BEEP=0;
					if(ensure==0x00)
					{
						LCD_Clear(WHITE);
						LCD_ShowString(30,90,200,16,16,"The fingerprint is normal.");
						i=0;
						processnum=2;//����������
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
					processnum=3;//�������Ĳ�
				}
				else 
				{
					LCD_Clear(WHITE);
					LCD_ShowString(30,90,200,16,16,"Failed,try again.");
					ShowErrMessage(ensure);
					i=0;
					processnum=0;//���ص�һ��		
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
					processnum=4;//�������岽
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
				while(!(ID<AS608Para.PS_max));//����ID����С��ģ������������ֵ
				ensure=PS_StoreChar(CharBuffer2,ID);//����ģ��
				if(ensure==0x00) 
				{			
					LCD_Clear(WHITE);
					LCD_ShowString(30,130,200,16,16,"Sucess finally.");
					PS_ValidTempleteNum(&ValidN);//����ָ�Ƹ���
					LCD_ShowNum(56,80,AS608Para.PS_max-ValidN,3,16);//��ʾʣ��ָ������
					delay_ms(1500);
					LCD_Fill(0,100,240,160,WHITE);
					return ;
				}else {processnum=0;ShowErrMessage(ensure);}					
				break;				
		}
		delay_ms(400);
		if(i==5)//����5��û�а���ָ���˳�
		{
			LCD_Fill(0,100,lcddev.width,160,WHITE);
			break;	
		}				
	}
	LCD_Clear(WHITE);
}

//ˢָ��
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
		if(ensure==0x00)//��ȡͼ��ɹ� 
		{	
			BEEP=1;//�򿪷�����	
			ensure=PS_GenChar(CharBuffer1);
			if(ensure==0x00) //���������ɹ�
			{		
				BEEP=0;//�رշ�����	
				ensure=PS_HighSpeedSearch(CharBuffer1,0,AS608Para.PS_max,&seach);
				if(ensure==0x00)//�����ɹ�
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
		 BEEP=0;//�رշ�����
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
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//����ϵͳ�ж����ȼ�����2
	u8 ensure;
	u32 i,j;
	char *strr;
	SearchResult seach;
	int key;      //����
	char str[30];
	u8 test[]={1,2,3,0,4,5};
	delay_init(168);  	//��ʼ����ʱ����
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOF|RCC_AHB1Periph_GPIOG|RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOD, ENABLE);	 //ʹ����ض˿�ʱ��
	KeyBoardIni();
	uart_init(115200);	//��ʼ������1������Ϊ115200������֧��USMART
	LED_Init();					//��ʼ��LED 
 	LCD_Init();					//LCD��ʼ��  
 	KEY_Init();					//������ʼ�� 
 	POINT_COLOR=RED;//��������Ϊ��ɫ 
	LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");	
	LCD_ShowString(30,70,200,16,16,"OV2640 TEST");	
	LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,110,200,16,16,"2014/5/14");
	usart2_init(usart2_baund);//��ʼ������2,������ָ��ģ��ͨѶ
	usart3_init(usart3_baund);
	PS_StaGPIO_Init();	//��ʼ��FR��״̬����
	usmart_dev.init(168);		//��ʼ��USMART
	my_mem_init(SRAMIN);		//��ʼ���ڲ��ڴ�� 
	my_mem_init(SRAMCCM);		//��ʼ��CCM�ڴ�
	TIM14_PWM_Init(20000-1,84-1);
	TIM14->CCR1=1100;
	W25QXX_Init();				//��ʼ��W25Q128
//	Esp8266Init();
	delay_ms(2000);
	sprintf(str,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);
	atk_8266_send_cmd(str,"OK",200);
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //����ģʽΪ��͸��		
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPSEND","OK",200);
	while(OV2640_Init())//��ʼ��OV2640
	{
		LCD_ShowString(30,130,240,16,16,"OV2640 ERR");
		delay_ms(200);
	    LCD_Fill(30,130,239,170,WHITE);
		delay_ms(200);
	}
	LCD_ShowString(30,130,200,16,16,"OV2640 OK");  
	ov2640_mode=1;
	TIM6_Int_Init(10000,7199);			//10Khz����Ƶ��,1�����ж�	
	TIM2_Int_Init(10000,7199);
	EXTIX_Init();						//ʹ�ܶ�ʱ������								  
/*****************************/
	while(PS_HandShake(&AS608Addr)){//��AS608ģ������
		delay_ms(400);
		delay_ms(800);
	}
	delay_ms(1000);
	ensure=PS_ValidTempleteNum(&ValidN);//����ָ�Ƹ���
	if(ensure!=0x00){
		ShowErrMessage(ensure);//��ʾȷ���������Ϣ
	}
	ensure=PS_ReadSysPara(&AS608Para);  //������
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
		while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);  //�ȴ��ϴδ������ 
		USART_SendData(USART3,test[j]); 	 //�������ݵ�����3 
	}
	
	/****jepg_test****/
	u8 *p;
	u8 Mark;
	u8 size=1;		//Ĭ����QVGA 320*240�ߴ�
	u8 msgbuf[15];	//��Ϣ������ 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 JPEG Mode");
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//�Աȶ�
	LCD_ShowString(30,120,200,16,16,"KEY1:Saturation"); 		//ɫ�ʱ��Ͷ�
	LCD_ShowString(30,140,200,16,16,"KEY2:Effects"); 			//��Ч 
	LCD_ShowString(30,160,200,16,16,"KEY_UP:Size");				//�ֱ������� 
	sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
	LCD_ShowString(30,180,200,16,16,msgbuf);					//��ʾ��ǰJPEG�ֱ���
	
 	OV2640_JPEG_Mode();		//JPEGģʽ
	My_DCMI_Init();			//DCMI����
	DCMI_DMA_Init((u32)&jpeg_buf,jpeg_buf_size,DMA_MemoryDataSize_Word,DMA_MemoryInc_Enable);//DCMI DMA����   
	OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//��������ߴ� 
	DCMI_Start(); 		//��������
	LCD_Clear(WHITE);
	/****************/
	while(1){
		LCD_ShowString(30,50,210,16,16,"Press zero to add fingerprint");
		LCD_ShowString(30,70,210,16,16,"Press the checker to unlock");
		if(jpeg_data_ok==1)	//�Ѿ��ɼ���һ֡ͼ����
		{  
			Mark = 0;
			p=(u8*)jpeg_buf;
			LCD_ShowString(30,210,210,16,16,"Sending JPEG data..."); //��ʾ���ڴ�������
			for(i=0;i<jpeg_data_len*4;i++)		//dma����1�            ε���4�ֽ�,���Գ���4.
			{
				if(p[i]==0xFF && p[i+1]==0xD8){
					Mark = 1;
				}
				if(Mark){
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);	//ѭ������,ֱ���������  		
					USART_SendData(USART3,p[i]); 
				}
				if(p[i-1]==0xFF && p[i]==0xD9){
					Mark = 0;
				}
			} 
			LCD_ShowString(30,210,210,16,16,"Send data complete!!");//��ʾ����������� 
			jpeg_data_ok=2;	//���jpeg���ݴ�������,������DMAȥ�ɼ���һ֡��.
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
				if(ensure==0x00)//��ȡͼ��ɹ� 
				{	
				BEEP=1;//�򿪷�����	
				ensure=PS_GenChar(CharBuffer1);
				if(ensure==0x00) //���������ɹ�
				{		
					BEEP=0;//�رշ�����	
					ensure=PS_HighSpeedSearch(CharBuffer1,0,AS608Para.PS_max,&seach);
					if(ensure==0x00)//�����ɹ�
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
			 BEEP=0;//�رշ�����
			 delay_ms(600);
			 LCD_Fill(0,100,lcddev.width,160,WHITE);
			}
		}
	}
}
