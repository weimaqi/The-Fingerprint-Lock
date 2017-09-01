#ifndef _OV7670_H
#define _OV7670_H
#include "sys.h"
#include "sccb.h"
//////////////////////////////////////////////////////////////////////////////////	 
//������ο�������guanfu_wang���롣
//ALIENTEKս��STM32������V3
//OV7670 ��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2015/1/18
//�汾��V1.0		    							    							  
//////////////////////////////////////////////////////////////////////////////////
 
#define OV7670_VSYNC  	PEin(3)			//ͬ���źż��IO
#define OV7670_WRST		PEout(0)		//дָ�븴λ
#define OV7670_WREN		PEout(4)		//д��FIFOʹ��
#define OV7670_RCK_H	GPIOE->BSRR=1<<6//���ö�����ʱ�Ӹߵ�ƽ
#define OV7670_RCK_L	GPIOE->BRR=1<<6	//���ö�����ʱ�ӵ͵�ƽ
#define OV7670_RRST		PEout(2)  		//��ָ�븴λ
#define OV7670_CS		PEout(5)  		//Ƭѡ�ź�(OE)


#define OV7670_DATA   GPIOF->IDR&0x00FF  					//��������˿�
/////////////////////////////////////////									
	    				 
u8   OV7670_Init(void);		  	   		 
void OV7670_Light_Mode(u8 mode);
void OV7670_Color_Saturation(u8 sat);
void OV7670_Brightness(u8 bright);
void OV7670_Contrast(u8 contrast);
void OV7670_Special_Effects(u8 eft);
void OV7670_Window_Set(u16 sx,u16 sy,u16 width,u16 height);


#endif





















