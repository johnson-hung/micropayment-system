#ifndef __THREAD_H
#define __THREAD_H

#include <deque>
#include <string>
#include <pthread.h>
using namespace std;

//Task type
class CTask 
{
	protected:
		string m_strTaskName;
		int connfd; //receive address
	public:
		CTask() = default;
		CTask(string &taskName): m_strTaskName(taskName), connfd(){}
		virtual int Run() = 0;
		void SetConnFd(int data);
		int GetConnFd();
		virtual ~CTask() {}
};

//Threadpool
class CThreadPool
{
	private:
		static deque<CTask*> m_deqTaskList;
		static bool shutdown;
		int m_iThreadNum; //number of activated threads in threadpool
		pthread_t *pthread_id;
		static pthread_mutex_t m_pthreadMutex; //mutual exclusion
		static pthread_cond_t m_pthreadCond; //condition var for sync
	protected:
		static void* ThreadFunc(void * threadData);
		static int MoveToIdle(pthread_t tid); //task finished, move thread back to idle
		static int MoveToBusy(pthread_t tid);
		int Create(); //create threads
	public:
		CThreadPool(int threadNum = 10);
		int AddTask(CTask *task); //add tasks to queue
		int StopAll(); //stop thread in threadpool
		int getTaskSize(); //thread number in queue

};

#endif

