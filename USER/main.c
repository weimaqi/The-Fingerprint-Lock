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
#include "oled.h"
#include "esp8266.h"
#include "ov7670.h"
#include "exti.h"
/***********************************************************/

#define usart2_baund  57600//����2�����ʣ�����ָ��ģ�鲨���ʸ���
#define usart3_baund  19200//����3
SysPara AS608Para;//ָ��ģ��AS608����
u16 ValidN;//ģ������Чָ�Ƹ���
u8 netpro=1;	//����ģʽ
u8 ipbuf[20]="192.168.43.149";//IP��ַ
u8 HEADER[] = {
  66,0x4d,0x36,0x84,0x03,0,0,0,0,0,54,0,0,0,40,0,
  0,0,64,1,0,0,240,0,0,0,1,0,24,0,0,0,
  0,0,0,132,3,0,35,46,0,0,35,46,0,0,0,0,
  0,0,0,0,0,0
 };
/***********************************************************/

/***********************************************************/

void Esp8266Init(){
		int key;
		u8* p=mymalloc(SRAMIN,32);							//����32�ֽ��ڴ�
		atk_8266_send_cmd("AT+CWMODE=1","OK",50);		//����WIFI STAģʽ
		atk_8266_send_cmd("AT+RST","OK",20);		//DHCP�������ر�(��APģʽ��Ч) 
		delay_ms(1000);         //��ʱ3S�ȴ������ɹ�
		delay_ms(1000);
		delay_ms(1000);
		delay_ms(1000);
		atk_8266_send_cmd("AT+CWJAP=\"ALALA\",\"WANGTIEJU\"","WIFI GOT IP",300);
		delay_ms(1000);
		netpro = 1;	//ѡ������ģʽ
		if(netpro&0X01)     //TCP Client    ͸��ģʽ����
		{
			OLED_Clear();
			OLED_ShowString(0,0,"ATK-ESP WIFI-STA"); 
			atk_8266_send_cmd("AT+CIPMUX=0","OK",20);   //0�������ӣ�1��������
			delay_ms(1000);
			sprintf((char*)p,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);    //����Ŀ��TCP������
			atk_8266_send_cmd(p,"OK",200);
			delay_ms(1000);
/*			while(atk_8266_send_cmd(p,"OK",200))
			{
					OLED_Clear();
					OLED_ShowString(0,0,"TCP CONNECT FAILED"); //����ʧ��	 
			}	*/
			atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //����ģʽΪ��͸��		
			atk_8266_send_cmd("AT+CIPSEND","OK",200);		
//			OLED_ShowString(3,0,USART3_RX_BUF);			
		}
		myfree(SRAMIN,p);
}



void KeyBoardIni(){     //������̳�ʼ��
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 ;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_Init(GPIOD,&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin= GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 ;
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_DOWN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_Init(GPIOD,&GPIO_InitStructure);
}


int KeyBoardScan(){
	GPIOD->BSRRL=GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 ;
	if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_4) | GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5) | GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_6) | GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_7)){
			delay_ms(100);
		if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_4) | GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5) | GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_6) | GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_7)){
			GPIOD->BSRRL= GPIO_Pin_0;       //���ǲ��ǵ�һ��
			GPIOD->BSRRH= GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 ;
			if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_4)){
				return 0;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5)){
				return 1;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_6)){
				return 2;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_7)){
				return 3;
			}
			GPIOD->BSRRL= GPIO_Pin_1;       //���ǲ��ǵڶ���
			GPIOD->BSRRH= GPIO_Pin_0 | GPIO_Pin_2 | GPIO_Pin_3 ;
			if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_4)){
				return 4;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5)){
				return 5;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_6)){
				return 6;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_7)){
				return 7;
			}
			GPIOD->BSRRL= GPIO_Pin_2;       //���ǲ��ǵ�����
			GPIOD->BSRRH= GPIO_Pin_1 | GPIO_Pin_0 | GPIO_Pin_3 ;
			if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_4)){
				return 8;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5)){
				return 9;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_6)){
				return 10;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_7)){
				return 11;
			}
			GPIOD->BSRRL= GPIO_Pin_3;       //���ǲ��ǵ�����
			GPIOD->BSRRH= GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_0 ;
			if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_4)){
				return 12;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_5)){
				return 13;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_6)){
				return 14;
			}
			else if(GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_7)){
				return 15;
			}
		}
	}
	return -1;
}


void Led_Init(){    //LED��ʼ��
  GPIO_InitTypeDef  GPIO_InitStructure;
  //��ʼ����״̬����GPIOF
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9|GPIO_Pin_10;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//����ģʽ
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;//����
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_Init(GPIOF, &GPIO_InitStructure);//��ʼ��GPIO	
}



void Led_On(int choice){
	switch(choice){
		case 1:
			GPIOF->BSRRH=GPIO_Pin_9;
			break;
		case 2:
			GPIOF->BSRRH=GPIO_Pin_10;
			break;
		default:;
	}
}



void Led_Off(int choice){
	switch(choice){
		case 1:
			GPIOF->BSRRL=GPIO_Pin_9;
			break;
		case 2:
			GPIOF->BSRRL=GPIO_Pin_10;
			break;
		default:;
	}
}



void Key_Init(){     //������ʼ��
  GPIO_InitTypeDef  GPIO_InitStructure;
  //��ʼ����״̬����GPIOE
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_4;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;//����ģʽ
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//����ģʽ
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_Init(GPIOE, &GPIO_InitStructure);//��ʼ��GPIO
}



u8 Key_Scan(){
	delay_ms(100);
	if(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_4)==0 || GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_3)==0){
		delay_ms(100);
		if(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_4)==0){
			return 0;
		}
		else if(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_3)==0){
			return 1;
		}
	}
	return -1;
}




void Del_FR(void) //ɾ��ָ��
{
	u8  ensure;
	u16 num;
//	LCD_Fill(0,100,lcddev.width,160,WHITE);
//	Show_Str_Mid(0,100,"ɾ��ָ��",16,240);
//	Show_Str_Mid(0,120,"������ָ��ID��Enter����",16,240);
//	Show_Str_Mid(0,140,"0=< ID <=299",16,240);
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
//		LCD_Fill(0,120,lcddev.width,160,WHITE);
//		Show_Str_Mid(0,140,"ɾ��ָ�Ƴɹ�",16,240);		
	}
  else
//		ShowErrMessage(ensure);	
	delay_ms(1200);
	ensure=PS_ValidTempleteNum(&ValidN);//����ָ�Ƹ��� ȷ����
                                      //���ȷ�����
//	LCD_ShowNum(56,80,AS608Para.PS_max-ValidN,3,16);
MENU:	
//	LCD_Fill(0,100,lcddev.width,160,WHITE);
	delay_ms(50);
//	AS608_load_keyboard(0,170,(u8**)kbd_menu);
}



int press_FR(void)  //ˢָ��
{
	Led_On(2);
	SearchResult seach;
	u8 ensure;
	char *str;
	ensure=PS_GetImage();
	if(ensure==0x00)//��ȡͼ��ɹ� 
	{	
		ensure=PS_GenChar(CharBuffer1);
		if(ensure==0x00) //���������ɹ�
		{		
			ensure=PS_HighSpeedSearch(CharBuffer1,0,AS608Para.PS_max,&seach);
			if(ensure==0x00)//�����ɹ�
			{				
				OLED_ShowString(1,0,"FindSucess");				
				str=mymalloc(SRAMIN,2000);
				sprintf(str,"Exist:ID:%d  point:%d",seach.pageID,seach.mathscore);
				OLED_ShowString(2,0,str);
				myfree(SRAMIN,str);
				return 1;   //����
			}
			else {
				return 0;   //����
//				ShowErrMessage(ensure);	
			}				
	  }
		else{
//			ShowErrMessage(ensure);
		}
	delay_ms(600);
	OLED_Clear();
	}
	return 0;         //����
}



void Add_FR(void)   //¼ָ��
{
	u8 i,ensure ,processnum=0;
	int ID=-1;
	while(1)
	{
		switch (processnum)
		{
			case 0:
				i++;
				OLED_Clear();
				OLED_ShowString(0,0,"Please press");
				ensure=PS_GetImage();
				delay_ms(400);
				OLED_Clear();
				if(ensure==0x00) 
				{
					ensure=PS_GenChar(CharBuffer1);//��������
					if(ensure==0x00)
					{
						OLED_ShowString(1,0,"It's normal");
						delay_ms(400);
						OLED_Clear();
						i=0;
						processnum=1;//�����ڶ���						
					}else{
						//ShowErrMessage(ensure);				
					}
				}else{
					//ShowErrMessage(ensure);		
				}					
				break;
			
			case 1:
				i++;
				OLED_Clear();
				OLED_ShowString(0,0,"Press again");
				ensure=PS_GetImage();
				delay_ms(400);
				OLED_Clear();
				if(ensure==0x00) 
				{
					ensure=PS_GenChar(CharBuffer2);//��������
					if(ensure==0x00)
					{
						OLED_ShowString(3,0,"It's normal");
						delay_ms(400);
						OLED_Clear();
						i=0;
						processnum=2;//����������
					}else{
//						ShowErrMessage(ensure);	
					}
				}else{
//					ShowErrMessage(ensure);		
				}
				break;

			case 2:
				OLED_Clear();
				OLED_ShowString(1,0,"Contrast twice");
				ensure=PS_Match();
				delay_ms(400);
				OLED_Clear();
				if(ensure==0x00) 
				{
				OLED_Clear();
				OLED_ShowString(0,0,"They are normal");
				delay_ms(400);
				OLED_Clear();
					processnum=3;//�������Ĳ�
				}
				else 
				{
					OLED_Clear();
					OLED_ShowString(1,0,"Failed,try again");
					delay_ms(400);
					OLED_Clear();
//					ShowErrMessage(ensure);
					i=0;
					processnum=0;//���ص�һ��		
				}
				delay_ms(1200);
				break;

			case 3:
			OLED_Clear();
				OLED_ShowString(1,0,"generate model");
				ensure=PS_RegModel();
				delay_ms(400);
				OLED_Clear();
				if(ensure==0x00) 
				{
					OLED_Clear();
					OLED_ShowString(1,0,"Generate success");
					delay_ms(400);
					OLED_Clear();
					processnum=4;//�������岽
				}else {
					processnum=0;
//					ShowErrMessage(ensure);
				}
				delay_ms(1200);
				break;
				
			case 4:	
					OLED_Clear();
					OLED_ShowString(1,0,"Input id");
					delay_ms(400);
					OLED_Clear();
					OLED_ShowString(2,0,"0<=ID<=299");
				do{
						while(ID ==-1) ID=KeyBoardScan();
				}
				while(!(ID<AS608Para.PS_max));//����ID����С��ģ������������ֵ
				ensure=PS_StoreChar(CharBuffer2,ID);//����ģ��
				if(ensure==0x00) 
				{			
					OLED_Clear();
					OLED_ShowString(1,0,"ADD Success");
					delay_ms(400);
					OLED_Clear();
					PS_ValidTempleteNum(&ValidN);//����ָ�Ƹ���
					OLED_ShowNum(3,0,AS608Para.PS_max-ValidN,1+AS608Para.PS_max-ValidN/100,6*8);
//					LCD_ShowNum(56,80,AS608Para.PS_max-ValidN,3,16);//��ʾʣ��ָ������
					delay_ms(1500);
//					LCD_Fill(0,100,240,160,WHITE);
					return ;
				}else {
					processnum=0;
//					ShowErrMessage(ensure);
				}					
				break;				
		}
		delay_ms(400);
		if(i==5)//����5��û�а���ָ���˳�
		{
//			LCD_Fill(0,100,lcddev.width,160,WHITE);
			break;	
		}				
	}
}

//void RGB565_TO_BMP(u16 RGB565[240][320]) //230454
//{
// //BMP???
// u8 HEADER[] = {
//  0x42,0x4d,0x36,0x84,0x03,0,0,0,0,0,54,0,0,0,40,0,
//  0,0,64,1,0,0,240,0,0,0,1,0,24,0,0,0,
//  0,0,0,132,3,0,35,46,0,0,35,46,0,0,0,0,
//  0,0,0,0,0,0
// };
// u32 i = 0;
// u8 RRED,RBLUE,RGREEN;
// u16 COLOR;
// 
// //BMP?????
// for (i = 0;i < 0X36;i++)
// {
////  *BMP++ = *(HEADER + i);
//	 atk_8266_send_data(HEADER+i,0,0);
// }
// 
// //????
// for (i = 0;i < 76800;i++)
// {
//  COLOR = *(*(RGB565 + i / 320) + i % 320);
//  RRED = ((COLOR >> 8)) & 0xF8 + 3;
//  RBLUE = ((COLOR >> 3) & 0xFC) + 1;
//  RGREEN = ((COLOR << 3) & 0xF8) + 3;
//	 atk_8266_send_data(&RBLUE,0,0);
//	 atk_8266_send_data(&RGREEN,0,0);
//	 atk_8266_send_data(&RRED,0,0);
////  *BMP++ = RBLUE;
////  *BMP++ = RGREEN;
////  *BMP++ = RRED;  
// }
//}


/***********************************************************/

int main(void)  
{  
/******************************/
	Led_Init();  //LED��ʼ��
	Key_Init();  //KEY��ʼ��
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//����ϵͳ�ж����ȼ�����2
	u8 RRED,RBLUE,RGREEN;
	u8 ensure;
	u16 color,ColorValue[10];
	u32 i,j,k;
	int key;      //����
	char str[30];
	u8 test[]={1,2,3,0,4,5};
//	u16 ** bmp=(u16 **)mymalloc(SRAMIN,sizeof(u16 *) * 240); //���bmp color
//	for(i = 0;i < 240;i++){
//		bmp[i]=(u16 *)mymalloc(SRAMIN,sizeof(u16) * 320);
//	}
	delay_init(168);  	//��ʼ����ʱ����
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOF|RCC_AHB1Periph_GPIOG|RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOD, ENABLE);	 //ʹ����ض˿�ʱ��
	KeyBoardIni();
	OLED_Init();			//��ʼ��OLED  
	OLED_Clear(); 
	uart_init(115200);	//��ʼ������1������Ϊ115200������֧��USMART
	usart2_init(usart2_baund);//��ʼ������2,������ָ��ģ��ͨѶ
	usart3_init(usart3_baund);
	PS_StaGPIO_Init();	//��ʼ��FR��״̬����
	usmart_dev.init(168);		//��ʼ��USMART
	my_mem_init(SRAMIN);		//��ʼ���ڲ��ڴ�� 
	my_mem_init(SRAMCCM);		//��ʼ��CCM�ڴ�
	W25QXX_Init();				//��ʼ��W25Q128
	OLED_ShowString(0,0,"1");
	Esp8266Init();
	delay_ms(2000);
	sprintf(str,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);
	atk_8266_send_cmd(str,"OK",200);
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //����ģʽΪ��͸��		
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPSEND","OK",200);
	OLED_ShowString(0,0,"2");
	while(OV7670_Init());
	TIM6_Int_Init(10000,7199);			//10Khz����Ƶ��,1�����ж�									  
	EXTIX_Init();						//ʹ�ܶ�ʱ������								  
	OV7670_CS = 0;
/*****************************/

	Led_Off(2);
	Led_Off(1);
//	while(PS_HandShake(&AS608Addr)){//��AS608ģ������
//		delay_ms(400);
//		delay_ms(800);
//	}
	delay_ms(1000);
	Led_Off(2);
	Led_Off(1);
//	ensure=PS_ValidTempleteNum(&ValidN);//����ָ�Ƹ���
	if(ensure!=0x00){
		//ShowErrMessage(ensure);//��ʾȷ���������Ϣ
	}
//	ensure=PS_ReadSysPara(&AS608Para);  //������
	if(ensure==0x00)
	{
		sprintf(str,"capacity:%d level:%d",AS608Para.PS_max-ValidN,AS608Para.PS_level);
		OLED_ShowString(0,0, str);
	}
	else{
		//ShowErrMessage(ensure);
	}
	char t=' ';
	PS_Sta=0;
	delay_ms(100);
	for(j=0;j<6;j++){
		while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);  //�ȴ��ϴδ������ 
		USART_SendData(USART3,test[j]); 	 //�������ݵ�����3 
	}
	while(1){
		if(ov_sta){
			OV7670_RRST=0;				//��ʼ��λ��ָ�� 
			OV7670_RCK_L;
			OV7670_RCK_H;
			OV7670_RCK_L;
			OV7670_RRST=1;				//��λ��ָ����� 
			OV7670_RCK_H;
			 for (i = 0;i < 54;i++)
			 {
			//  *BMP++ = *(HEADER + i);
				 while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);
				 USART_SendData(USART3,HEADER[i]);
			 }
			for(i = 0;i < 240;i++){
				delay_ms(20);
				for(j = 0;j < 320;j++){
					OV7670_RCK_L;
					color = OV7670_DATA ;	//������
					OV7670_RCK_H;
					color<<=8;
					OV7670_RCK_L;
					color|= OV7670_DATA ;	//������
					OV7670_RCK_H;
					ColorValue[k=(k+1)%10] = color;
					RRED = ((color >> 8)) & 0xF8 + 3;
					RBLUE = ((color >> 3) & 0xFC) + 1;
					RGREEN = ((color << 3) & 0xF8) + 3;

					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);
					USART_SendData(USART3,RRED);
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);
					USART_SendData(USART3,RBLUE);
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);
					USART_SendData(USART3,RGREEN);
				}
			}
			ov_sta=0;					//����֡�жϱ��
			ov_frame++; 
		}
		key=-1;
		key=KeyBoardScan();
   	if(key==0){
//			Led_On(2);
//			while(PS_Sta!=1) Led_On(1);
//			delay_ms(600);
//			Led_Off(1);
//			delay_ms(600);
//			key=press_FR();
//			if(key){
//				Led_Off(2);
//			}
//			else{
//				Add_FR();
//				Led_On(1);
//			}
		}
		else if(key==2){
			Led_On(1);
		}
		else if(key==1){
			Led_Off(2);
			Led_Off(1);
		}
		else if(key==4){
			atk_8266_send_cmd("123",0,100);
		}
		else if(key==5){
			atk_8266_send_data("+++",0,100);
		}
		else if(key==6){
			atk_8266_send_cmd("AT+CIPSEND",0,100);
		}
	}
}
