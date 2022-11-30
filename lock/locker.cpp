#include "locker.h"

Sem::Sem(int num){ 
    if (sem_init(&s_id, 0, num) != 0)
        throw std::exception();
}
Sem::~Sem(){
    sem_destroy(&s_id);
}
bool Sem::wait(){
    return sem_wait(&s_id) == 0;
}
bool Sem::post(){
    return sem_post(&s_id) == 0;
}

Locker::Locker(){
    if (pthread_mutex_init(&mutex_id, NULL) != 0){
        throw std::exception();
    }
}
Locker::~Locker(){
    pthread_mutex_destroy(&mutex_id);
}
bool Locker::lock(){
    return pthread_mutex_lock(&mutex_id) == 0;
}
bool Locker::unlock(){
    return pthread_mutex_unlock(&mutex_id) == 0;
}
pthread_mutex_t* Locker::get(){
    return &mutex_id;
}

Cond::Cond(){
    if (pthread_cond_init(&c_id, NULL) != 0)
        throw std::exception();
}
Cond::~Cond(){
    pthread_cond_destroy(&c_id);
}
bool Cond::wait(pthread_mutex_t* m_id){
    return pthread_cond_wait(&c_id, m_id) == 0;
}
bool Cond::timewait(pthread_mutex_t* m_id, struct timespec t){
    return pthread_cond_timedwait(&c_id, m_id, &t);
}
bool Cond::singal(){
    return pthread_cond_signal(&c_id) == 0;
}
bool Cond::broadcast(){
    return pthread_cond_broadcast(&c_id) == 0;
}