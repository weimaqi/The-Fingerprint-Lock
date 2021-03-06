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

#define usart2_baund  57600//串口2波特率，根据指纹模块波特率更改
#define usart3_baund  19200//串口3
SysPara AS608Para;//指纹模块AS608参数
u16 ValidN;//模块内有效指纹个数
u8 netpro=1;	//网络模式
u8 ipbuf[20]="192.168.43.149";//IP地址
//u8 HEADER[] = {
//  0x42,0x4d,0x36,0x58,0x02,0,0,0,0,0,0x36,0,0,0,0x28,0,
//  0,0,0x28,0,0,0,0x28,0,0,0,0x01,0,0x10,0,0,0,
//  0,0,0,0x58,0x02,0,0x23,0x2e,0,0,0x23,0x2e,0,0,0,0,
//  0,0,0,0,0,0
// };
u8 ov2640_mode=0;						//工作模式:0,RGB565模式;1,JPEG模式

#define jpeg_buf_size 31*64  			//定义JPEG数据缓存jpeg_buf的大小(*4字节)
__align(4) u32 jpeg_buf[jpeg_buf_size];	//JPEG数据缓存buf
volatile u32 jpeg_data_len=0; 			//buf中的JPEG有效数据长度 
volatile u8 jpeg_data_ok=0;				//JPEG数据采集完成标志 
										//0,数据没有采集完;
										//1,数据采集完了,但是还没处理;
										//2,数据已经处理完成了,可以开始下一帧接收
//JPEG尺寸支持列表
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
const u8*EFFECTS_TBL[7]={"Normal","Negative","B&W","Redish","Greenish","Bluish","Antique"};	//7种特效 
const u8*JPEG_SIZE_TBL[9]={"QCIF","QQVGA","CIF","QVGA","VGA","SVGA","XGA","SXGA","UXGA"};	//JPEG图片 9种尺寸 


/***********************************************************/

/***********************************************************/


//处理JPEG数据
//当采集完一帧JPEG数据后,调用此函数,切换JPEG BUF.开始下一帧采集.
void jpeg_data_process(void)
{
	if(ov2640_mode)//只有在JPEG格式下,才需要做处理.
	{
		if(jpeg_data_ok==0)	//jpeg数据还未采集完?
		{	
			DMA_Cmd(DMA2_Stream1, DISABLE);//停止当前传输 
			while (DMA_GetCmdStatus(DMA2_Stream1) != DISABLE){}//等待DMA2_Stream1可配置  
			jpeg_data_len=jpeg_buf_size-DMA_GetCurrDataCounter(DMA2_Stream1);//得到此次数据传输的长度
				
			jpeg_data_ok=1; 				//标记JPEG数据采集完按成,等待其他函数处理
		}
		if(jpeg_data_ok==2)	//上一次的jpeg数据已经被处理了
		{
			DMA2_Stream1->NDTR=jpeg_buf_size;	
			DMA_SetCurrDataCounter(DMA2_Stream1,jpeg_buf_size);//传输长度为jpeg_buf_size*4字节
			DMA_Cmd(DMA2_Stream1, ENABLE);			//重新传输
			jpeg_data_ok=0;						//标记数据未采集
		}
	}
} 
//JPEG测试
//JPEG数据,通过串口2发送给电脑.
void jpeg_test(void)
{
	u32 i; 
	u8 *p;
	u8 key;
	u8 Mark;
	u8 effect=0,saturation=2,contrast=2;
	u8 size=3;		//默认是QVGA 320*240尺寸
	u8 msgbuf[15];	//消息缓存区 
	LCD_Clear(WHITE);
  POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 JPEG Mode");
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//对比度
	LCD_ShowString(30,120,200,16,16,"KEY1:Saturation"); 		//色彩饱和度
	LCD_ShowString(30,140,200,16,16,"KEY2:Effects"); 			//特效 
	LCD_ShowString(30,160,200,16,16,"KEY_UP:Size");				//分辨率设置 
	sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
	LCD_ShowString(30,180,200,16,16,msgbuf);					//显示当前JPEG分辨率
	
 	OV2640_JPEG_Mode();		//JPEG模式
	My_DCMI_Init();			//DCMI配置
	DCMI_DMA_Init((u32)&jpeg_buf,jpeg_buf_size,DMA_MemoryDataSize_Word,DMA_MemoryInc_Enable);//DCMI DMA配置   
	OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//设置输出尺寸 
	DCMI_Start(); 		//启动传输
	while(1)
	{
		if(jpeg_data_ok==1)	//已经采集完一帧图像了
		{  
			Mark = 0;
			p=(u8*)jpeg_buf;
			LCD_ShowString(30,210,210,16,16,"Sending JPEG data..."); //提示正在传输数据
			for(i=0;i<jpeg_data_len*4;i++)		//dma传输1次等于4字节,所以乘以4.
			{
				if(p[i]==0xFF && p[i+1]==0xD8){
					Mark = 1;
				}
				if(Mark){
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);	//循环发送,直到发送完毕  		
					USART_SendData(USART3,p[i]); 
				}
				key=KEY_Scan(0); 
				if(key)break;
			} 
			if(key)	//有按键按下,需要处理
			{  
				LCD_ShowString(30,210,210,16,16,"Quit Sending data   ");//提示退出数据传输
				switch(key)
				{				    
					case KEY0_PRES:	//对比度设置
						contrast++;
						if(contrast>4)contrast=0;
						OV2640_Contrast(contrast);
						sprintf((char*)msgbuf,"Contrast:%d",(signed char)contrast-2);
						break;
					case KEY1_PRES:	//饱和度Saturation
						saturation++;
						if(saturation>4)saturation=0;
						OV2640_Color_Saturation(saturation);
						sprintf((char*)msgbuf,"Saturation:%d",(signed char)saturation-2);
						break;
					case KEY2_PRES:	//特效设置				 
						effect++;
						if(effect>6)effect=0;
						OV2640_Special_Effects(effect);//设置特效
						sprintf((char*)msgbuf,"%s",EFFECTS_TBL[effect]);
						break;
					case WKUP_PRES:	//JPEG输出尺寸设置   
						size++;  
						if(size>8)size=0;   
						OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//设置输出尺寸  
						sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
						break;
				}
				LCD_Fill(30,180,239,190+16,WHITE);
				LCD_ShowString(30,180,210,16,16,msgbuf);//显示提示内容
				delay_ms(800); 				  
			}else LCD_ShowString(30,210,210,16,16,"Send data complete!!");//提示传输结束设置 
			jpeg_data_ok=2;	//标记jpeg数据处理完了,可以让DMA去采集下一帧了.
		}		
	}    
} 
//RGB565测试
//RGB数据直接显示在LCD上面
void rgb565_test(void)
{ 
	u8 key;
	u8 effect=0,saturation=2,contrast=2;
	u8 scale=1;		//默认是全尺寸缩放
	u8 msgbuf[15];	//消息缓存区 
	LCD_Clear(WHITE);
    POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 RGB565 Mode");
	
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//对比度
	LCD_ShowString(30,130,200,16,16,"KEY1:Saturation"); 		//色彩饱和度
	LCD_ShowString(30,150,200,16,16,"KEY2:Effects"); 			//特效 
	LCD_ShowString(30,170,200,16,16,"KEY_UP:FullSize/Scale");	//1:1尺寸(显示真实尺寸)/全尺寸缩放
	
	OV2640_RGB565_Mode();	//RGB565模式
	My_DCMI_Init();			//DCMI配置
	DCMI_DMA_Init((u32)&LCD->LCD_RAM,1,DMA_MemoryDataSize_HalfWord,DMA_MemoryInc_Disable);//DCMI DMA配置  
 	OV2640_OutSize_Set(lcddev.width,lcddev.height); 
	DCMI_Start(); 		//启动传输
	while(1)
	{ 
		key=KEY_Scan(0); 
		if(key)
		{ 
			DCMI_Stop(); //停止显示
			switch(key)
			{				    
				case KEY0_PRES:	//对比度设置
					contrast++;
					if(contrast>4)contrast=0;
					OV2640_Contrast(contrast);
					sprintf((char*)msgbuf,"Contrast:%d",(signed char)contrast-2);
					break;
				case KEY1_PRES:	//饱和度Saturation
					saturation++;
					if(saturation>4)saturation=0;
					OV2640_Color_Saturation(saturation);
					sprintf((char*)msgbuf,"Saturation:%d",(signed char)saturation-2);
					break;
				case KEY2_PRES:	//特效设置				 
					effect++;
					if(effect>6)effect=0;
					OV2640_Special_Effects(effect);//设置特效
					sprintf((char*)msgbuf,"%s",EFFECTS_TBL[effect]);
					break;
				case WKUP_PRES:	//1:1尺寸(显示真实尺寸)/缩放	    
					scale=!scale;  
					if(scale==0)
					{
						OV2640_ImageWin_Set((1600-lcddev.width)/2,(1200-lcddev.height)/2,lcddev.width,lcddev.height);//1:1真实尺寸
						OV2640_OutSize_Set(lcddev.width,lcddev.height); 
						sprintf((char*)msgbuf,"Full Size 1:1");
					}else 
					{
						OV2640_ImageWin_Set(0,0,1600,1200);				//全尺寸缩放
						OV2640_OutSize_Set(lcddev.width,lcddev.height); 
						sprintf((char*)msgbuf,"Scale");
					}
					break;
			}
			LCD_ShowString(30,50,210,16,16,msgbuf);//显示提示内容
			delay_ms(800); 
			DCMI_Start();//重新开始传输
		} 
		delay_ms(10);		
	}    
} 


//void Esp8266Init(){
//		int key;
//		u8* p=mymalloc(SRAMIN,32);							//申请32字节内存
//		atk_8266_send_cmd("AT+CWMODE=1","OK",50);		//设置WIFI STA模式
//		atk_8266_send_cmd("AT+RST","OK",20);		//DHCP服务器关闭(仅AP模式有效) 
//		delay_ms(1000);         //延时3S等待重启成功
//		delay_ms(1000);
//		delay_ms(1000);
//		delay_ms(1000);
//		atk_8266_send_cmd("AT+CWJAP=\"ALALA\",\"WANGTIEJU\"","WIFI GOT IP",300);
//		delay_ms(1000);
//		netpro = 1;	//选择网络模式
//		if(netpro&0X01)     //TCP Client    透传模式测试
//		{
//			OLED_Clear();
//			OLED_ShowString(0,0,"ATK-ESP WIFI-STA"); 
//			atk_8266_send_cmd("AT+CIPMUX=0","OK",20);   //0：单连接，1：多连接
//			delay_ms(1000);
//			sprintf((char*)p,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);    //配置目标TCP服务器
//			atk_8266_send_cmd(p,"OK",200);
//			delay_ms(1000);
///*			while(atk_8266_send_cmd(p,"OK",200))
//			{
//					OLED_Clear();
//					OLED_ShowString(0,0,"TCP CONNECT FAILED"); //连接失败	 
//			}	*/
//			atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //传输模式为：透传		
//			atk_8266_send_cmd("AT+CIPSEND","OK",200);		
////			OLED_ShowString(3,0,USART3_RX_BUF);			
//		}
//		myfree(SRAMIN,p);
//}



void KeyBoardIni(){     //矩阵键盘初始化
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
			GPIOF->BSRRL= GPIO_Pin_0;       //判是不是第一行
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
			GPIOF->BSRRL= GPIO_Pin_1;       //判是不是第二行
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
			GPIOF->BSRRL= GPIO_Pin_2;       //判是不是第三行
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
			GPIOF->BSRRL= GPIO_Pin_3;       //判是不是第四行
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


void Del_FR(void) //删除指纹
{
	u8  ensure;
	u16 num;
	LCD_Fill(0,100,lcddev.width,160,WHITE);
	LCD_Clear(WHITE);
  POINT_COLOR=RED; 
	LCD_ShowString(30,50,200,16,16,"删除指纹");
	LCD_ShowString(30,70,200,16,16,"请输入指纹ID按Enter发送");
	LCD_ShowString(30,90,200,16,16,"0=< ID <=299");
	delay_ms(50);
//	AS608_load_keyboard(0,170,(u8**)kbd_delFR);
//	num=GET_NUM();//获取返回的数值
	if(num==0xFFFF)
		goto MENU ; //返回主页面
	else if(num==0xFF00)
		ensure=PS_Empty();//清空指纹库
	else 
		ensure=PS_DeletChar(num,1);//删除单个指纹
	if(ensure==0)
	{
		LCD_Fill(0,120,lcddev.width,160,WHITE);
		LCD_ShowString(30,110,200,16,16,"删除指纹成功");
	}
  else
		ShowErrMessage(ensure);	
	delay_ms(1200);
	ensure=PS_ValidTempleteNum(&ValidN);//读库指纹个数 确认字
                                      //输出确认情况
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
					ensure=PS_GenChar(CharBuffer1);//生成特征
					BEEP=0;
					if(ensure==0x00)
					{
						LCD_Fill(0,120,lcddev.width,160,WHITE);
						LCD_ShowString(30,70,200,16,16,"The fingerprint is normal.");
						i=0;
						processnum=1;//跳到第二步						
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
					ensure=PS_GenChar(CharBuffer2);//生成特征
					BEEP=0;
					if(ensure==0x00)
					{
						LCD_Clear(WHITE);
						LCD_ShowString(30,90,200,16,16,"The fingerprint is normal.");
						i=0;
						processnum=2;//跳到第三步
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
					processnum=3;//跳到第四步
				}
				else 
				{
					LCD_Clear(WHITE);
					LCD_ShowString(30,90,200,16,16,"Failed,try again.");
					ShowErrMessage(ensure);
					i=0;
					processnum=0;//跳回第一步		
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
					processnum=4;//跳到第五步
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
				while(!(ID<AS608Para.PS_max));//输入ID必须小于模块容量最大的数值
				ensure=PS_StoreChar(CharBuffer2,ID);//储存模板
				if(ensure==0x00) 
				{			
					LCD_Clear(WHITE);
					LCD_ShowString(30,130,200,16,16,"Sucess finally.");
					PS_ValidTempleteNum(&ValidN);//读库指纹个数
					LCD_ShowNum(56,80,AS608Para.PS_max-ValidN,3,16);//显示剩余指纹容量
					delay_ms(1500);
					LCD_Fill(0,100,240,160,WHITE);
					return ;
				}else {processnum=0;ShowErrMessage(ensure);}					
				break;				
		}
		delay_ms(400);
		if(i==5)//超过5次没有按手指则退出
		{
			LCD_Fill(0,100,lcddev.width,160,WHITE);
			break;	
		}				
	}
	LCD_Clear(WHITE);
}

//刷指纹
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
		if(ensure==0x00)//获取图像成功 
		{	
			BEEP=1;//打开蜂鸣器	
			ensure=PS_GenChar(CharBuffer1);
			if(ensure==0x00) //生成特征成功
			{		
				BEEP=0;//关闭蜂鸣器	
				ensure=PS_HighSpeedSearch(CharBuffer1,0,AS608Para.PS_max,&seach);
				if(ensure==0x00)//搜索成功
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
		 BEEP=0;//关闭蜂鸣器
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
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	u8 ensure;
	u32 i,j;
	char *strr;
	SearchResult seach;
	int key;      //按键
	char str[30];
	u8 test[]={1,2,3,0,4,5};
	delay_init(168);  	//初始化延时函数
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOF|RCC_AHB1Periph_GPIOG|RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOD, ENABLE);	 //使能相关端口时钟
	KeyBoardIni();
	uart_init(115200);	//初始化串口1波特率为115200，用于支持USMART
	LED_Init();					//初始化LED 
 	LCD_Init();					//LCD初始化  
 	KEY_Init();					//按键初始化 
 	POINT_COLOR=RED;//设置字体为红色 
	LCD_ShowString(30,50,200,16,16,"Explorer STM32F4");	
	LCD_ShowString(30,70,200,16,16,"OV2640 TEST");	
	LCD_ShowString(30,90,200,16,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,110,200,16,16,"2014/5/14");
	usart2_init(usart2_baund);//初始化串口2,用于与指纹模块通讯
	usart3_init(usart3_baund);
	PS_StaGPIO_Init();	//初始化FR读状态引脚
	usmart_dev.init(168);		//初始化USMART
	my_mem_init(SRAMIN);		//初始化内部内存池 
	my_mem_init(SRAMCCM);		//初始化CCM内存
	TIM14_PWM_Init(20000-1,84-1);
	TIM14->CCR1=1100;
	W25QXX_Init();				//初始化W25Q128
//	Esp8266Init();
	delay_ms(2000);
	sprintf(str,"AT+CIPSTART=\"TCP\",\"%s\",%s",ipbuf,(u8*)portnum);
	atk_8266_send_cmd(str,"OK",200);
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPMODE=1","OK",200);      //传输模式为：透传		
	delay_ms(1000);
	atk_8266_send_cmd("AT+CIPSEND","OK",200);
	while(OV2640_Init())//初始化OV2640
	{
		LCD_ShowString(30,130,240,16,16,"OV2640 ERR");
		delay_ms(200);
	    LCD_Fill(30,130,239,170,WHITE);
		delay_ms(200);
	}
	LCD_ShowString(30,130,200,16,16,"OV2640 OK");  
	ov2640_mode=1;
	TIM6_Int_Init(10000,7199);			//10Khz计数频率,1秒钟中断	
	TIM2_Int_Init(10000,7199);
	EXTIX_Init();						//使能定时器捕获								  
/*****************************/
	while(PS_HandShake(&AS608Addr)){//与AS608模块握手
		delay_ms(400);
		delay_ms(800);
	}
	delay_ms(1000);
	ensure=PS_ValidTempleteNum(&ValidN);//读库指纹个数
	if(ensure!=0x00){
		ShowErrMessage(ensure);//显示确认码错误信息
	}
	ensure=PS_ReadSysPara(&AS608Para);  //读参数
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
		while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);  //等待上次传输完成 
		USART_SendData(USART3,test[j]); 	 //发送数据到串口3 
	}
	
	/****jepg_test****/
	u8 *p;
	u8 Mark;
	u8 size=1;		//默认是QVGA 320*240尺寸
	u8 msgbuf[15];	//消息缓存区 
	LCD_ShowString(30,50,200,16,16,"ALIENTEK STM32F4");
	LCD_ShowString(30,70,200,16,16,"OV2640 JPEG Mode");
	LCD_ShowString(30,100,200,16,16,"KEY0:Contrast");			//对比度
	LCD_ShowString(30,120,200,16,16,"KEY1:Saturation"); 		//色彩饱和度
	LCD_ShowString(30,140,200,16,16,"KEY2:Effects"); 			//特效 
	LCD_ShowString(30,160,200,16,16,"KEY_UP:Size");				//分辨率设置 
	sprintf((char*)msgbuf,"JPEG Size:%s",JPEG_SIZE_TBL[size]);
	LCD_ShowString(30,180,200,16,16,msgbuf);					//显示当前JPEG分辨率
	
 	OV2640_JPEG_Mode();		//JPEG模式
	My_DCMI_Init();			//DCMI配置
	DCMI_DMA_Init((u32)&jpeg_buf,jpeg_buf_size,DMA_MemoryDataSize_Word,DMA_MemoryInc_Enable);//DCMI DMA配置   
	OV2640_OutSize_Set(jpeg_img_size_tbl[size][0],jpeg_img_size_tbl[size][1]);//设置输出尺寸 
	DCMI_Start(); 		//启动传输
	LCD_Clear(WHITE);
	/****************/
	while(1){
		LCD_ShowString(30,50,210,16,16,"Press zero to add fingerprint");
		LCD_ShowString(30,70,210,16,16,"Press the checker to unlock");
		if(jpeg_data_ok==1)	//已经采集完一帧图像了
		{  
			Mark = 0;
			p=(u8*)jpeg_buf;
			LCD_ShowString(30,210,210,16,16,"Sending JPEG data..."); //提示正在传输数据
			for(i=0;i<jpeg_data_len*4;i++)		//dma传输1�            蔚扔�4字节,所以乘以4.
			{
				if(p[i]==0xFF && p[i+1]==0xD8){
					Mark = 1;
				}
				if(Mark){
					while(USART_GetFlagStatus(USART3,USART_FLAG_TC)==RESET);	//循环发送,直到发送完毕  		
					USART_SendData(USART3,p[i]); 
				}
				if(p[i-1]==0xFF && p[i]==0xD9){
					Mark = 0;
				}
			} 
			LCD_ShowString(30,210,210,16,16,"Send data complete!!");//提示传输结束设置 
			jpeg_data_ok=2;	//标记jpeg数据处理完了,可以让DMA去采集下一帧了.
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
				if(ensure==0x00)//获取图像成功 
				{	
				BEEP=1;//打开蜂鸣器	
				ensure=PS_GenChar(CharBuffer1);
				if(ensure==0x00) //生成特征成功
				{		
					BEEP=0;//关闭蜂鸣器	
					ensure=PS_HighSpeedSearch(CharBuffer1,0,AS608Para.PS_max,&seach);
					if(ensure==0x00)//搜索成功
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
			 BEEP=0;//关闭蜂鸣器
			 delay_ms(600);
			 LCD_Fill(0,100,lcddev.width,160,WHITE);
			}
		}
	}
}
