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

//����Ϣָ��Ϊ��ʱ,ָ��һ������pvNullPointer��ָ���ֵ.
//��UCOS�����OSQPost()�е�msg==NULL�᷵��һ��OS_ERR_POST_NULL
//����,����lwip�л����sys_mbox_post(mbox,NULL)����һ������Ϣ,����
//�ڱ������а�NULL���һ������ָ��0Xffffffff
const void * const pvNullPointer = (mem_ptr_t*)0xffffffff;
 

//����һ����Ϣ����
//*mbox:��Ϣ����
//size:�����С
//����ֵ:ERR_OK,�����ɹ�
//         ����,����ʧ��
err_t sys_mbox_new( sys_mbox_t *mbox, int size)
{
	OS_ERR err;

	(*mbox)=mymalloc(SRAMIN,sizeof(TQ_DESCR));	//Ϊ��Ϣ���������ڴ�
	mymemset((*mbox),0,sizeof(TQ_DESCR)); 		//���mbox���ڴ�

	(*mbox)->pQ=mymalloc(SRAMIN,sizeof(OS_Q));
	mymemset((*mbox)->pQ,0,sizeof(OS_Q));

	if(*mbox&&(*mbox)->pQ)//�ڴ����ɹ�
	{
		if(size>MAX_QUEUE_ENTRIES)
			size=MAX_QUEUE_ENTRIES;		//��Ϣ�����������MAX_QUEUE_ENTRIES��Ϣ��Ŀ 

		OSQCreate((*mbox)->pQ,"lwip_mbox",size,&err);  //ʹ��UCOS����һ����Ϣ����
		LWIP_ASSERT("OSQCreate",err==OS_ERR_NONE);
		if(err==OS_ERR_NONE)
			return ERR_OK;  //����ERR_OK,��ʾ��Ϣ���д����ɹ� ERR_OK=0
		else
		{ 
			myfree(SRAMIN,(*mbox));
			return ERR_MEM;  		//��Ϣ���д�������
		}
	}else return ERR_MEM; 			//��Ϣ���д������� 
} 
//�ͷŲ�ɾ��һ����Ϣ����
//*mbox:Ҫɾ������Ϣ����
void sys_mbox_free(sys_mbox_t * mbox)
{
	OS_ERR err;
	sys_mbox_t m_box=*mbox;   
	OSQDel(m_box->pQ,OS_OPT_DEL_ALWAYS,&err);
	LWIP_ASSERT( "OSQDel ",err == OS_ERR_NONE );
	myfree(SRAMIN,m_box);
	*mbox=NULL;
}
//����Ϣ�����з���һ����Ϣ(���뷢�ͳɹ�)
//*mbox:��Ϣ����
//*msg:Ҫ���͵���Ϣ
void sys_mbox_post(sys_mbox_t *mbox,void *msg)
{
	OS_ERR err;
	OS_MSG_SIZE msg_size;
	if(msg==NULL)
		msg=(void*)&pvNullPointer;//��msgΪ��ʱ msg����pvNullPointerָ���ֵ 

	msg_size = (OS_MSG_SIZE)sizeof(msg);

	do {
		OSQPost((*mbox)->pQ,(void*)msg,msg_size,OS_OPT_POST_ALL,&err);
	} while(err!=OS_ERR_NONE);//��ѭ���ȴ���Ϣ���ͳɹ�
}
//������һ����Ϣ���䷢����Ϣ
//�˺��������sys_mbox_post����ֻ����һ����Ϣ��
//����ʧ�ܺ󲻻᳢�Եڶ��η���
//*mbox:��Ϣ����
//*msg:Ҫ���͵���Ϣ
//����ֵ:ERR_OK,����OK
// 	     ERR_MEM,����ʧ��
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{ 
	OS_ERR err;
	OS_MSG_SIZE msg_size;
	if(msg==NULL)
		msg=(void*)&pvNullPointer;//��msgΪ��ʱ msg����pvNullPointerָ���ֵ 

	msg_size = (OS_MSG_SIZE)sizeof(msg);

	OSQPost((*mbox)->pQ,(void*)msg,msg_size,OS_OPT_POST_ALL,&err);
	if(err!=OS_ERR_NONE)
		return ERR_MEM;

	return ERR_OK;
}

//�ȴ������е���Ϣ
//*mbox:��Ϣ����
//*msg:��Ϣ
//timeout:��ʱʱ�䣬���timeoutΪ0�Ļ�,��һֱ�ȴ�
//����ֵ:��timeout��Ϊ0ʱ����ɹ��Ļ��ͷ��صȴ���ʱ�䣬
//		ʧ�ܵĻ��ͷ��س�ʱSYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{ 
	OS_ERR err;
	OS_MSG_SIZE msg_size;
	u32_t ucos_timeout,timeout_new;
	void *temp;
	sys_mbox_t m_box=*mbox;

	if(timeout!=0)
	{
		ucos_timeout=(timeout*OS_CFG_TICK_RATE_HZ)/1000; //ת��Ϊ������,��ΪUCOS��ʱʹ�õ��ǽ�����,��LWIP����ms
		if(ucos_timeout<1)
			ucos_timeout=1;//����1������
	}
	else
		ucos_timeout = 0; 

	timeout = OSTimeGet(&err); //��ȡϵͳʱ�� 

	temp=OSQPend(m_box->pQ,
				 (OS_TICK)ucos_timeout,
				 OS_OPT_PEND_BLOCKING,
				 &msg_size,
				 (CPU_TS *)0,
				 &err); //������Ϣ����,�ȴ�ʱ��Ϊucos_timeout

	if(msg!=NULL)
	{	
		if(temp==(void*)&pvNullPointer)*msg = NULL;   	//��Ϊlwip���Ϳ���Ϣ��ʱ������ʹ����pvNullPointerָ��,�����ж�pvNullPointerָ���ֵ
 		else *msg=temp;									//�Ϳ�֪�����󵽵���Ϣ�Ƿ���Ч
	}    
	if(err==OS_ERR_TIMEOUT)timeout=SYS_ARCH_TIMEOUT;  //����ʱ
	else
	{
		LWIP_ASSERT("OSQPend ",err==OS_ERR_NONE); 
		timeout_new=OSTimeGet(&err);
		if (timeout_new>timeout) timeout_new = timeout_new - timeout;//���������Ϣ��ʹ�õ�ʱ��
		else timeout_new = 0xffffffff - timeout + timeout_new; 
		timeout=timeout_new*1000/OS_CFG_TICK_RATE_HZ + 1;
	}
	return timeout; 
}
//���Ի�ȡ��Ϣ
//*mbox:��Ϣ����
//*msg:��Ϣ
//����ֵ:�ȴ���Ϣ���õ�ʱ��/SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	return sys_arch_mbox_fetch(mbox,msg,1);//���Ի�ȡһ����Ϣ
}
//���һ����Ϣ�����Ƿ���Ч
//*mbox:��Ϣ����
//����ֵ:1,��Ч.
//      0,��Ч
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
//����һ����Ϣ����Ϊ��Ч
//*mbox:��Ϣ����
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	*mbox=NULL;
} 
//����һ���ź���
//*sem:�������ź���
//count:�ź���ֵ
//����ֵ:ERR_OK,����OK
// 	     ERR_MEM,����ʧ��
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
//�ȴ�һ���ź���
//*sem:Ҫ�ȴ����ź���
//timeout:��ʱʱ��
//����ֵ:��timeout��Ϊ0ʱ����ɹ��Ļ��ͷ��صȴ���ʱ�䣬
//		ʧ�ܵĻ��ͷ��س�ʱSYS_ARCH_TIMEOUT
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{ 
	OS_ERR err;
	u32_t ucos_timeout, timeout_new; 
	if(timeout!=0) 
	{
		ucos_timeout = (timeout * OS_CFG_TICK_RATE_HZ) / 1000;//ת��Ϊ������,��ΪUCOS��ʱʹ�õ��ǽ�����,��LWIP����ms
		if(ucos_timeout < 1)
			ucos_timeout = 1;
	}else
		ucos_timeout = 0; 

	timeout = OSTimeGet(&err);
	OSSemPend (*sem,(u16_t)ucos_timeout,OS_OPT_PEND_BLOCKING,NULL,&err);
 	if(err == OS_ERR_TIMEOUT)
		timeout=SYS_ARCH_TIMEOUT;//����ʱ	
	else
	{     
 		timeout_new = OSTimeGet(&err);
		if (timeout_new>=timeout)
			timeout_new = timeout_new - timeout;
		else
			timeout_new = 0xffffffff - timeout + timeout_new;

		timeout = (timeout_new*1000/OS_CFG_TICK_RATE_HZ + 1);//���������Ϣ��ʹ�õ�ʱ��(ms)
	}
	return timeout;
}
//����һ���ź���
//sem:�ź���ָ��
void sys_sem_signal(sys_sem_t *sem)
{
	OS_ERR err;
	OSSemPost(*sem, OS_OPT_POST_ALL, &err);
}
//�ͷŲ�ɾ��һ���ź���
//sem:�ź���ָ��
void sys_sem_free(sys_sem_t *sem)
{
	OS_ERR err;
	(void)OSSemDel(*sem,OS_OPT_DEL_ALWAYS,&err);
	if(err!=OS_ERR_NONE)LWIP_ASSERT("OSSemDel ",err==OS_ERR_NONE);
	*sem = NULL;
} 
//��ѯһ���ź�����״̬,��Ч����Ч
//sem:�ź���ָ��
//����ֵ:1,��Ч.
//      0,��Ч
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
//����һ���ź�����Ч
//sem:�ź���ָ��
void sys_sem_set_invalid(sys_sem_t *sem)
{
	*sem=NULL;
} 
//arch��ʼ��
void sys_init(void)
{ 
    //����,�����ڸú���,�����κ�����
} 
extern CPU_STK * TCPIP_THREAD_TASK_STK;//TCP IP�ں������ջ,��lwip_comm��������
extern OS_TCB tcpip_thread_tcb;
//����һ���½���
//*name:��������
//thred:����������
//*arg:�����������Ĳ���
//stacksize:��������Ķ�ջ��С
//prio:������������ȼ�
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
	OS_ERR err;
	CPU_SR_ALLOC();
	if(strcmp(name,TCPIP_THREAD_NAME)==0)//����TCP IP�ں�����
	{
		OS_CRITICAL_ENTER();  //�����ٽ��� 
		//����TCP IP�ں����� 
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
		OS_CRITICAL_EXIT();  //�˳��ٽ���
	} 
	return 0;
} 
//lwip��ʱ����
//ms:Ҫ��ʱ��ms��
//void sys_msleep(u32_t ms)
//{
//	delay_ms(ms);
//}
//��ȡϵͳʱ��,LWIP1.4.1���ӵĺ���
//����ֵ:��ǰϵͳʱ��(��λ:����)
u32_t sys_now(void)
{
	OS_ERR err;
	u32_t ucos_time, lwip_time;
	ucos_time=OSTimeGet(&err);	//��ȡ��ǰϵͳʱ�� �õ�����UCSO�Ľ�����
	lwip_time=(ucos_time*1000/OS_CFG_TICK_RATE_HZ+1);//��������ת��ΪLWIP��ʱ��MS
	return lwip_time; 		//����lwip_time;
}

