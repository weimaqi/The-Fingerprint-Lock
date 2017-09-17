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
 
#define OV7670_VSYNC  	PAin(8)			//ͬ���źż��IO
#define OV7670_WRST		PBout(7)		//дָ�븴λ
#define OV7670_WREN		PGout(9)		//д��FIFOʹ��
#define OV7670_RCK_H	GPIOA->BSRRL=GPIO_Pin_6//���ö�����ʱ�Ӹߵ�ƽ
#define OV7670_RCK_L	GPIOA->BSRRH=GPIO_Pin_6	//���ö�����ʱ�ӵ͵�ƽ
#define OV7670_RRST		PAout(4)  		//��ָ�븴λ
#define OV7670_CS		PGout(15)  		//Ƭѡ�ź�(OE)


#define OV7670_DATA   ((GPIOC->IDR&0x0040)>>6)&0x01 | ((GPIOC->IDR&0x0080)>>6)&0x02 | ((GPIOC->IDR&0x0100)>>6)&0x04 | ((GPIOC->IDR&0x0200)>>6)&0x08 | ((GPIOC->IDR&0x0800)>>7)&0x10 | ((GPIOB->IDR&0x0100)>>3)&0x20 | ((GPIOE->IDR&0x0020)<<1)&0x40 | ((GPIOE->IDR&0x0040)<<1)&0x80
/////////////////////////////////////////									
	    				 
u8   OV7670_Init(void);		  	   		 
void OV7670_Light_Mode(u8 mode);
void OV7670_Color_Saturation(u8 sat);
void OV7670_Brightness(u8 bright);
void OV7670_Contrast(u8 contrast);
void OV7670_Special_Effects(u8 eft);
void OV7670_Window_Set(u16 sx,u16 sy,u16 width,u16 height);


#endif





















