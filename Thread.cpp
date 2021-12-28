#include "Thread.h"
#include <iostream>
#include <stdio.h>
#include <deque>
#define fun main
void CTask::SetConnFd(int data)
{
	connfd = data;
}

int CTask::GetConnFd()
{
	return connfd;
}

//Initialize
deque<CTask*> CThreadPool::m_deqTaskList;
bool CThreadPool::shutdown = false;

pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;

//Threadpool management
CThreadPool::CThreadPool(int threadNum)
{
	this->m_iThreadNum = threadNum;
	cout<<"Create "<<threadNum<<" threads"<<endl;
	Create();
}

void* CThreadPool::ThreadFunc(void* threadData)
{
	pthread_t tid = pthread_self();
	while(1)
	{
		//lock
		pthread_mutex_lock(&m_pthreadMutex);
		while(m_deqTaskList.size() == 0 && !shutdown)
		{
			//When there is no task, thread will wait for condition
			pthread_cond_wait(&m_pthreadCond, &m_pthreadMutex);
		}
		
		if(shutdown)
		{
			pthread_mutex_unlock(&m_pthreadMutex);
			cout<<"Thread "<<pthread_self()<<" will exit"<<endl;
			pthread_exit(NULL);
		}

		cout<<"tid "<<tid<<" run"<<endl;

		//Get task list, then execute it

		CTask* task = m_deqTaskList.front();
		m_deqTaskList.pop_front();
		//already got task list, unlock mutex
		pthread_mutex_unlock(&m_pthreadMutex);
		task->Run(); //execute it

	}
	return (void*)0;
}

//Add task and send sync signal
int CThreadPool::AddTask(CTask *task)
{
	pthread_mutex_lock(&m_pthreadMutex);
	this->m_deqTaskList.push_back(task);
	pthread_mutex_unlock(&m_pthreadMutex);

	//send signal
	pthread_cond_signal(&m_pthreadCond);
	return 0;
}

//Create new thread
int CThreadPool::Create()
{
	pthread_id = (pthread_t*)malloc(sizeof(pthread_t) * m_iThreadNum);
	for(int i = 0; i < m_iThreadNum; i++)
	{
		pthread_create(&pthread_id[i], NULL, ThreadFunc, NULL);
	}
	return 0;
}

//Stop all threads
int CThreadPool::StopAll()
{
	if(shutdown)
	{
		return -1;
	}
	cout<<"End all threads"<<endl;
	shutdown = true;
	pthread_cond_broadcast(&m_pthreadCond);

	for(int i = 0; i < m_iThreadNum; i++)
	{
		pthread_join(pthread_id[i], NULL);
	}

	free(pthread_id);
	pthread_id = NULL;

	//destroy all cond and mutex
	pthread_mutex_destroy(&m_pthreadMutex);
	pthread_cond_destroy(&m_pthreadCond);

	return 0;
}

//Get task size
int CThreadPool::getTaskSize()
{
	return m_deqTaskList.size();
}

