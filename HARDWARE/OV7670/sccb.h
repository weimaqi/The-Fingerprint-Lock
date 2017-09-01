#ifndef __SCCB_H
#define __SCCB_H
#include "sys.h"
//////////////////////////////////////////////////////////////////////////////////	 
//������ο�������guanfu_wang���롣
//ALIENTEKս��STM32������V3
//SCCB ��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2015/1/18
//�汾��V1.0
//////////////////////////////////////////////////////////////////////////////////

#define SCCB_SDA_IN()  {GPIOC->MODER&=0XF3FFFFFF;}
#define SCCB_SDA_OUT() {GPIOC->MODER&=0XF3FFFFFF;GPIOE->MODER|=0X04000000;}

//IO��������
#define SCCB_SCL    		PEout(1)	 	//SCL
#define SCCB_SDA    		PCout(13) 		//SDA

#define SCCB_READ_SDA    	PCin(13)  		//����SDA    
#define SCCB_ID   			0X42  			//OV7670��ID

///////////////////////////////////////////
void SCCB_Init(void);
void SCCB_Start(void);
void SCCB_Stop(void);
void SCCB_No_Ack(void);
u8 SCCB_WR_Byte(u8 dat);
u8 SCCB_RD_Byte(void);
u8 SCCB_WR_Reg(u8 reg,u8 data);
u8 SCCB_RD_Reg(u8 reg);
#endif