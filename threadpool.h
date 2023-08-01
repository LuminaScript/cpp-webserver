#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// Thread pool class, defined as a template class to achieve code reusability, with template parameter T as the task class.
template<typename T>
class threadpool {
public:
    /* thread_number is the number of threads in the thread pool, max_requests is the maximum number of requests allowed in the waiting queue for processing. */
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    /* Function run by the working threads, it continuously takes tasks from the work queue and executes them. */
    static void* worker(void* arg);
    void run();

private:
    // Number of threads in the pool.
    int m_thread_number;  
    
    // Array to describe the thread pool, size is m_thread_number.
    pthread_t * m_threads;

    // Maximum number of requests allowed in the waiting queue for processing.
    int m_max_requests; 
    
    // Request queue.
    std::list< T* > m_workqueue;  

    // Mutex lock to protect the request queue.
    locker m_queuelocker;   

    // Semaphore to indicate if there are tasks to be processed.
    sem m_queuestat;

    // Flag to indicate whether the thread pool is being stopped.
    bool m_stop;                    
};

template< typename T >
threadpool< T >::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests), 
        m_stop(false), m_threads(NULL) {

    if((thread_number <= 0) || (max_requests <= 0) ) {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) {
        throw std::exception();
    }

    // Create thread_number threads and set them as detached threads.
    for ( int i = 0; i < thread_number; ++i ) {
        printf( "create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        
        if(pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template< typename T >
bool threadpool< T >::append( T* request )
{
    // When operating on the work queue, we must lock it because it is shared by all threads.
    m_queuelocker.lock();
    if ( m_workqueue.size() > m_max_requests ) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template< typename T >
void* threadpool< T >::worker(void* arg)
{
    threadpool* pool = (threadpool*)arg; // Worker cannot access static member
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run() {

    while (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if ( m_workqueue.empty() ) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( !request ) {
            continue;
        }
        request->process();
    }

}

#endif
