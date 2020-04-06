/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
/*  Porting by Michael Vysotsky <michaelvy@hotmail.com> August 2011   */

#define SYS_ARCH_GLOBALS

/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/lwip_sys.h"
#include "lwip/mem.h"
#include "includes.h"
#include "delay.h"
#include "arch/sys_arch.h"
#include "malloc.h"
#include <os_cfg_app.h>

//当消息指针为空时,指向一个常量pvNullPointer所指向的值.
//在UCOS中如果OSQPost()中的msg==NULL会返回一条OS_ERR_POST_NULL
//错误,而在lwip中会调用sys_mbox_post(mbox,NULL)发送一条空消息,我们
//在本函数中把NULL变成一个常量指针0Xffffffff
const void * const pvNullPointer = (mem_ptr_t*)0xffffffff;
 

//创建一个消息邮箱
//*mbox:消息邮箱
//size:邮箱大小
//返回值:ERR_OK,创建成功
//         其他,创建失败
err_t sys_mbox_new( sys_mbox_t *mbox, int size)
{
	OS_ERR err;

	(*mbox)=mymalloc(SRAMIN,sizeof(TQ_DESCR));	//为消息邮箱申请内存
	mymemset((*mbox),0,sizeof(TQ_DESCR)); 		//清除mbox的内存

	(*mbox)->pQ=mymalloc(SRAMIN,sizeof(OS_Q));
	mymemset((*mbox)->pQ,0,sizeof(OS_Q));

	if(*mbox&&(*mbox)->pQ)//内存分配成功
	{
		if(size>MAX_QUEUE_ENTRIES)
			size=MAX_QUEUE_ENTRIES;		//消息队列最多容纳MAX_QUEUE_ENTRIES消息数目 

		OSQCreate((*mbox)->pQ,"lwip_mbox",size,&err);  //使用UCOS创建一个消息队列
		LWIP_ASSERT("OSQCreate",err==OS_ERR_NONE);
		if(err==OS_ERR_NONE)
			return ERR_OK;  //返回ERR_OK,表示消息队列创建成功 ERR_OK=0
		else
		{ 
			myfree(SRAMIN,(*mbox));
			return ERR_MEM;  		//消息队列创建错误
		}
	}else return ERR_MEM; 			//消息队列创建错误 
} 
//释放并删除一个消息邮箱
//*mbox:要删除的消息邮箱
void sys_mbox_free(sys_mbox_t * mbox)
{
	OS_ERR err;
	sys_mbox_t m_box=*mbox;   
	OSQDel(m_box->pQ,OS_OPT_DEL_ALWAYS,&err);
	LWIP_ASSERT( "OSQDel ",err == OS_ERR_NONE );
	myfree(SRAMIN,m_box);
	*mbox=NULL;
}
//向消息邮箱中发送一条消息(必须发送成功)
//*mbox:消息邮箱
//*msg:要发送的消息
void sys_mbox_post(sys_mbox_t *mbox,void *msg)
{
	OS_ERR err;
	OS_MSG_SIZE msg_size;
	if(msg==NULL)
		msg=(void*)&pvNullPointer;//当msg为空时 msg等于pvNullPointer指向的值 

	msg_size = (OS_MSG_SIZE)sizeof(msg);

	do {
		OSQPost((*mbox)->pQ,(void*)msg,msg_size,OS_OPT_POST_ALL,&err);
	} while(err!=OS_ERR_NONE);//死循环等待消息发送成功
}
//尝试向一个消息邮箱发送消息
//此函数相对于sys_mbox_post函数只发送一次消息，
//发送失败后不会尝试第二次发送
//*mbox:消息邮箱
//*msg:要发送的消息
//返回值:ERR_OK,发送OK
// 	     ERR_MEM,发送失败
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{ 
	OS_ERR err;
	OS_MSG_SIZE msg_size;
	if(msg==NULL)
		msg=(void*)&pvNullPointer;//当msg为空时 msg等于pvNullPointer指向的值 

	msg_size = (OS_MSG_SIZE)sizeof(msg);

	OSQPost((*mbox)->pQ,(void*)msg,msg_size,OS_OPT_POST_ALL,&err);
	if(err!=OS_ERR_NONE)
		return ERR_MEM;

	return ERR_OK;
}

//等待邮箱中的消息
//*mbox:消息邮箱
//*msg:消息
//timeout:超时时间，如果timeout为0的话,就一直等待
//返回值:当timeout不为0时如果成功的话就返回等待的时间，
//		失败的话就返回超时SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{ 
	OS_ERR err;
	OS_MSG_SIZE msg_size;
	u32_t ucos_timeout,timeout_new;
	void *temp;
	sys_mbox_t m_box=*mbox;

	if(timeout!=0)
	{
		ucos_timeout=(timeout*OS_CFG_TICK_RATE_HZ)/1000; //转换为节拍数,因为UCOS延时使用的是节拍数,而LWIP是用ms
		if(ucos_timeout<1)
			ucos_timeout=1;//至少1个节拍
	}
	else
		ucos_timeout = 0; 

	timeout = OSTimeGet(&err); //获取系统时间 

	temp=OSQPend(m_box->pQ,
				 (OS_TICK)ucos_timeout,
				 OS_OPT_PEND_BLOCKING,
				 &msg_size,
				 (CPU_TS *)0,
				 &err); //请求消息队列,等待时限为ucos_timeout

	if(msg!=NULL)
	{	
		if(temp==(void*)&pvNullPointer)*msg = NULL;   	//因为lwip发送空消息的时候我们使用了pvNullPointer指针,所以判断pvNullPointer指向的值
 		else *msg=temp;									//就可知道请求到的消息是否有效
	}    
	if(err==OS_ERR_TIMEOUT)timeout=SYS_ARCH_TIMEOUT;  //请求超时
	else
	{
		LWIP_ASSERT("OSQPend ",err==OS_ERR_NONE); 
		timeout_new=OSTimeGet(&err);
		if (timeout_new>timeout) timeout_new = timeout_new - timeout;//算出请求消息或使用的时间
		else timeout_new = 0xffffffff - timeout + timeout_new; 
		timeout=timeout_new*1000/OS_CFG_TICK_RATE_HZ + 1;
	}
	return timeout; 
}
//尝试获取消息
//*mbox:消息邮箱
//*msg:消息
//返回值:等待消息所用的时间/SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	return sys_arch_mbox_fetch(mbox,msg,1);//尝试获取一个消息
}
//检查一个消息邮箱是否有效
//*mbox:消息邮箱
//返回值:1,有效.
//      0,无效
int sys_mbox_valid(sys_mbox_t *mbox)
{  
	sys_mbox_t m_box=*mbox;
	//u8_t ucErr;
	int ret;
	//OS_Q_DATA q_data;
	//memset(&q_data,0,sizeof(OS_Q_DATA));
	//ucErr=OSQQuery (m_box->pQ,&q_data);
	ret=(/*ucErr<2&&*/(m_box->pQ->MsgQ.NbrEntries<m_box->pQ->MsgQ.NbrEntriesSize))?1:0;
	return ret; 
} 
//设置一个消息邮箱为无效
//*mbox:消息邮箱
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	*mbox=NULL;
} 
//创建一个信号量
//*sem:创建的信号量
//count:信号量值
//返回值:ERR_OK,创建OK
// 	     ERR_MEM,创建失败
err_t sys_sem_new(sys_sem_t * sem, u8_t count)
{  
	OS_ERR err;

	(*sem)=mymalloc(SRAMIN,sizeof(OS_SEM));
	mymemset(*sem,0,sizeof(OS_SEM));
	LWIP_ASSERT("OSSemCreate ",*sem != NULL);

	if(*sem)
	{
		OSSemCreate(*sem,"LWIP Sem",(u16_t)count,&err);
		if(err==OS_ERR_NONE)
			return ERR_OK;
		else
			return ERR_MEM;
	}
	else
		return ERR_MEM;
} 
//等待一个信号量
//*sem:要等待的信号量
//timeout:超时时间
//返回值:当timeout不为0时如果成功的话就返回等待的时间，
//		失败的话就返回超时SYS_ARCH_TIMEOUT
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{ 
	OS_ERR err;
	u32_t ucos_timeout, timeout_new; 
	if(timeout!=0) 
	{
		ucos_timeout = (timeout * OS_CFG_TICK_RATE_HZ) / 1000;//转换为节拍数,因为UCOS延时使用的是节拍数,而LWIP是用ms
		if(ucos_timeout < 1)
			ucos_timeout = 1;
	}else
		ucos_timeout = 0; 

	timeout = OSTimeGet(&err);
	OSSemPend (*sem,(u16_t)ucos_timeout,OS_OPT_PEND_BLOCKING,NULL,&err);
 	if(err == OS_ERR_TIMEOUT)
		timeout=SYS_ARCH_TIMEOUT;//请求超时	
	else
	{     
 		timeout_new = OSTimeGet(&err);
		if (timeout_new>=timeout)
			timeout_new = timeout_new - timeout;
		else
			timeout_new = 0xffffffff - timeout + timeout_new;

		timeout = (timeout_new*1000/OS_CFG_TICK_RATE_HZ + 1);//算出请求消息或使用的时间(ms)
	}
	return timeout;
}
//发送一个信号量
//sem:信号量指针
void sys_sem_signal(sys_sem_t *sem)
{
	OS_ERR err;
	OSSemPost(*sem, OS_OPT_POST_ALL, &err);
}
//释放并删除一个信号量
//sem:信号量指针
void sys_sem_free(sys_sem_t *sem)
{
	OS_ERR err;
	(void)OSSemDel(*sem,OS_OPT_DEL_ALWAYS,&err);
	if(err!=OS_ERR_NONE)LWIP_ASSERT("OSSemDel ",err==OS_ERR_NONE);
	*sem = NULL;
} 
//查询一个信号量的状态,无效或有效
//sem:信号量指针
//返回值:1,有效.
//      0,无效
int sys_sem_valid(sys_sem_t *sem)
{
	//OS_SEM_DATA  sem_data;
	//return (OSSemQuery (*sem,&sem_data) == OS_ERR_NONE )? 1:0;              

	sys_sem_t sem_t = *sem;
	if (sem_t == NULL) {
		return 0;
	}

	if (sem_t->Type != OS_OBJ_TYPE_SEM) { 	/* Validate event block type 			   */
		return 0;
	}

	return 1;
}
//设置一个信号量无效
//sem:信号量指针
void sys_sem_set_invalid(sys_sem_t *sem)
{
	*sem=NULL;
} 
//arch初始化
void sys_init(void)
{ 
    //这里,我们在该函数,不做任何事情
} 
extern CPU_STK * TCPIP_THREAD_TASK_STK;//TCP IP内核任务堆栈,在lwip_comm函数定义
extern OS_TCB tcpip_thread_tcb;
//创建一个新进程
//*name:进程名称
//thred:进程任务函数
//*arg:进程任务函数的参数
//stacksize:进程任务的堆栈大小
//prio:进程任务的优先级
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	if(strcmp(name,TCPIP_THREAD_NAME)==0)//创建TCP IP内核任务
	{
		OS_CRITICAL_ENTER();  //进入临界区 
		//创建TCP IP内核任务 
		OSTaskCreate((OS_TCB	* )&tcpip_thread_tcb, 	
					 (CPU_CHAR	* )name,		
					 (OS_TASK_PTR )thread, 			
					 (void		* )arg,
					 (OS_PRIO	  )prio,
					 (CPU_STK	* )&TCPIP_THREAD_TASK_STK[0], 
					 (CPU_STK_SIZE)stacksize/10,	
					 (CPU_STK_SIZE)stacksize, 	
					 (OS_MSG_QTY  )0,
					 (OS_TICK	  )0,					
					 (void		* )0,					
					 (OS_OPT	  )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR,
					 (OS_ERR	* )&err);
		OS_CRITICAL_EXIT();  //退出临界区
	} 
	return 0;
} 
//lwip延时函数
//ms:要延时的ms数
//void sys_msleep(u32_t ms)
//{
//	delay_ms(ms);
//}
//获取系统时间,LWIP1.4.1增加的函数
//返回值:当前系统时间(单位:毫秒)
u32_t sys_now(void)
{
	OS_ERR err;
	u32_t ucos_time, lwip_time;
	ucos_time=OSTimeGet(&err);	//获取当前系统时间 得到的是UCSO的节拍数
	lwip_time=(ucos_time*1000/OS_CFG_TICK_RATE_HZ+1);//将节拍数转换为LWIP的时间MS
	return lwip_time; 		//返回lwip_time;
}

