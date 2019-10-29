/** 
 * @author SG Lee
 * @since 3/20/2010
 * @version 0.2
 * @description
 * This file includes two template classes. One is GTaskQue to register 
 * tasks, the other is GExecutorInterface that is the interface class 
 * to define a task. A task can be registered with synchronization support, 
 * and a set of tasks can be registered, too.  
 * The registered tasks are executed by a thread function continuously
 * by set the function of doAutoExecution true.
 * A registered task can be executed intermittently by calling 
 * doExecution(), but this method is not recommened because a thread 
 * functions is generated whenever this function is called. 
 */

#ifndef __GTASKQUE_H__
#define __GTASKQUE_H__

#include <iostream>
#include <atomic>
#include <list>
#include <vector>
#include <assert.h>

#ifdef __APPLE__
#define __linux__ __APPLE__
#endif

#ifdef __linux__
#include <unistd.h>
#include <pthread.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <process.h>
#endif

#ifdef _WIN32
typedef HANDLE			MUTEX_TYPE;
typedef HANDLE			THREADHANDLE_TYPE;
#elif __linux__
typedef pthread_mutex_t		MUTEX_TYPE;
typedef pthread_t		THREADHANDLE_TYPE;
#endif

#define USLEEP_SCALE_FACTOR	100

using namespace std;

/*
 * T : Task
 * E : Task Executor
 */
template <typename T, typename E>
class GExecutorInterface {
	template <typename S, typename Q>
	friend class GTaskQue;
protected:
	E *attribute;
private:
	bool automatic_attribute_deletion;
public:
	GExecutorInterface(
		E *_attri, 
		const bool _automatic_attribute_deletion) {
		attribute = _attri;
		automatic_attribute_deletion = 
			_automatic_attribute_deletion;
	}
	virtual ~GExecutorInterface() {
		if (automatic_attribute_deletion) {
			delete attribute;
		}
	}
	const E *getAttribute()const { return attribute; }
	bool isAttributeDeletionAutomatically()const { 
		return automatic_attribute_deletion; 
	}
protected:
	GExecutorInterface()
		:attribute(0), 
		automatic_attribute_deletion(true) {}
	GExecutorInterface(const GExecutorInterface &) {}
	GExecutorInterface &operator=(const GExecutorInterface &) { 
		return *this; }
	// blocking call
	virtual int execute(T &_arg) const { return 0; }
};

#define DEFAULT_SIZE_BACK_BUFFER 100

/*
 * T : Task
 * E : Task Executor
 */
template <typename T, typename E>
class GTaskQue {
private:
#ifdef _WIN32
	static unsigned WINAPI thread_function_execution(void *_arg) {
#elif __linux__
	static void * thread_function_execution(void *_arg) {
#endif
		GTaskQue<T,E> *que = static_cast<GTaskQue<T,E> *>(_arg);
		///////////////////
		// blocking method 
		que->executeTask();
		///////////////////
		return 0;
	}

#ifdef _WIN32
	static unsigned WINAPI thread_function_autoexecution(void *_arg) {
#elif __linux__
	static void * thread_function_autoexecution(void *_arg) {
#endif
		GTaskQue<T,E> *que = static_cast<GTaskQue<T,E> *>(_arg);
		que->is_autoexecution_thread_running = true;
		while (1) {
			////////////////////////////////////
			que->mutex_lock();
			if (que->isBackBufferExecuted()) {
				que->copyToBackBuffer();
			}
			que->mutex_unlock();
			////////////////////////////////////
		
			///////////////////
			// blocking method 
			que->executeBatch();
			///////////////////
			
			if (que->is_quit_requested || 
				que->autoexecution_command == false) {
				while(!que->areAllTasksExecuted()) {
					//////////////////////////////////
					que->mutex_lock();
					if (que->isBackBufferExecuted()) {
						que->copyToBackBuffer();
					}
					que->mutex_unlock();
					//////////////////////////////////
				
					//////////////////////////////////
					que->executeBatch();
					//////////////////////////////////

#ifdef _WIN32
					Sleep(1);
#elif __linux__
					usleep(1*USLEEP_SCALE_FACTOR);
#endif
				}
				
				que->is_autoexecution_thread_running=false;
				break;
			}
			
			if (que->delay_between_batch != 0) {
#ifdef _WIN32
				Sleep(que->delay_between_batch);
#elif __linux__
				usleep(que->delay_between_batch * 
					USLEEP_SCALE_FACTOR);
#endif
			}
		}
		return 0;
	}

private:
	// sleep between batch processes in autoexecution
	// unit : millisecond
	unsigned long		delay_between_batch;
	
	// sleep between jobs in the batch process
	// unit : millisecond
	unsigned long		delay_in_batch;
	
	// batch process is invoked by setAutoExecution
	std::atomic<bool>	autoexecution_command;

	// Autoexecution for batch process
	std::atomic<bool>	is_autoexecution_thread_running;

	// This flag will be changed after calling quitThread()
	std::atomic<bool>	is_quit_requested;

	const GExecutorInterface<T,E> *executor;
	size_t			size_back_buffer;

#ifdef _WIN32
	HANDLE			mutex;
	HANDLE			thread_handle_autoexecution;
#elif __linux__
	mutable 		pthread_mutex_t	mutex;
	pthread_t		thread_handle_autoexecution;
	int			thread_param_autoexecution;
#endif
	bool			is_mutex_created;

	size_t			index_executor;
	list<pair<bool,T> >	front_buffer;
	vector<T *>		back_buffer;

private:
	GTaskQue();
	GTaskQue(const GTaskQue<T,E> &);
	virtual GTaskQue<T,E> &operator=(const GTaskQue<T,E> &);
public:
	GTaskQue(
		const GExecutorInterface<T,E> *_executor,
		const size_t _size_back_buffer=DEFAULT_SIZE_BACK_BUFFER);
	virtual ~GTaskQue();
public:
	inline void setDalayBetweenBatch(const unsigned long _delay) {
		delay_between_batch = _delay;
	}
	inline void setDelayInBatch(const unsigned long _delay) {
		delay_in_batch = _delay;
	}
	void initialize();
	void createMutex();
	void destroyMutex();
	size_t getFrontBufferSize()const;
	size_t getBackBufferSize()const;
	void quitThread();
	bool isRunning()const;
	int pushBack(const T &_v);
	int pushBack(const vector<T> &_v);
	int pushBack(const list<T> &_v);
	int doAutoExecution(const bool &_v);
	int doExecution();
	bool areAllTasksExecuted()const;
	void mutex_lock();
	void mutex_lock()const;
	void mutex_unlock();
	void mutex_unlock()const;
private:
	int executeTask();
	int executeBatch();
	size_t copyToBackBuffer();
	bool isBackBufferExecuted()const;
};

template <typename T, typename E>
void GTaskQue<T,E>::initialize() {
#ifdef _WIN32
	thread_handle_autoexecution = 0;
#elif __linux__
	thread_handle_autoexecution = 0;
	thread_param_autoexecution = 0;
#endif
	index_executor = 0;
	autoexecution_command = false;
	is_autoexecution_thread_running = false;
	is_quit_requested = false;
}

template <typename T, typename E>
void GTaskQue<T,E>::createMutex() {
	if(is_mutex_created) {
		return;
	}
#ifdef _WIN32
	mutex = CreateMutex(NULL, FALSE, NULL);
#elif __linux__
	if (pthread_mutex_init(&mutex, NULL) != 0) {
		throw("pthread_mutex_init error");
	}
#endif
	is_mutex_created = true;
}

template <typename T, typename E>
void GTaskQue<T,E>::destroyMutex() {
	if (is_mutex_created) {
#ifdef _WIN32
		CloseHandle(mutex);
#elif __linux__
		pthread_mutex_destroy(&mutex);
#endif
	}

	is_mutex_created = false;
}

template <typename T, typename E>
GTaskQue<T,E>::GTaskQue(
	const GExecutorInterface<T,E> *_executor,
	const size_t _size_back_buffer) {
	assert(_executor);
	if (!_executor) {
		throw("Please, define executor, first");
	}

	initialize();

	delay_between_batch = 1;	// milliseconds
	delay_in_batch = 0; 	  	// milliseconds
	is_mutex_created = false;

	executor = _executor;
	size_back_buffer = _size_back_buffer;
	back_buffer.assign(size_back_buffer,nullptr);

	// create Mutex
	createMutex();
}

// private
template <typename T, typename E>
GTaskQue<T,E>::GTaskQue() {
	// do not use
}

// private
template <typename T, typename E>
GTaskQue<T,E>::GTaskQue(const GTaskQue<T,E> &) {
	// do not use
}

// private
template <typename T, typename E>
GTaskQue<T,E> &GTaskQue<T,E>::operator=(const GTaskQue<T,E> &) {
	// do not use
	return *this;
}

template <typename T, typename E>
GTaskQue<T,E>::~GTaskQue() {
	quitThread();

	assert(getFrontBufferSize()==0);
//	if(getFrontBufferSize()!=0) {
//		throw("check your code, quitThread()");
//	}
	// check the code, maybe some problems in quitThread()

	// if GExecutorInterface is not const, the below code is available
//	if (executor) {
//		delete executor;
//		executor = nullptr;
//	}
	
	destroyMutex();
}

template <typename T, typename E>
size_t GTaskQue<T,E>::getFrontBufferSize() const {
	mutex_lock();
	size_t size = front_buffer.size();
	mutex_unlock();
	return size;
}

template <typename T, typename E>
size_t GTaskQue<T,E>::getBackBufferSize() const {
	return back_buffer.size();
}

template <typename T, typename E>
void GTaskQue<T,E>::quitThread() {
	if (is_quit_requested) {
		cout<<"quit() is already requested"<<endl;
		return;
	}

	is_quit_requested = true;

	// quit and wait until autoexecution will be shutdown
	doAutoExecution(false);

	while (is_autoexecution_thread_running) {
#ifdef _WIN32
		Sleep(1);
#elif __linux__
		usleep(1*USLEEP_SCALE_FACTOR);
#endif
//		cout<<"waiting for completion of autoexecution..."<<endl;
	}
	
	is_quit_requested = false;
}

template <typename T, typename E>
bool GTaskQue<T,E>::isRunning() const {
	if(is_quit_requested || 
		!areAllTasksExecuted() || 
		is_autoexecution_thread_running) {
		return true;
	}
	else {
		return false;
	}
}

// return value
// 0  : normal
// -1 : The quit request is already called
template <typename T, typename E>
int GTaskQue<T,E>::pushBack(const T &_v) {
	if (is_quit_requested) {
		cout << "quitThread is requested" << endl;
		return -1;
	}

	/////////////
	// lock
	mutex_lock();
	/////////////

	front_buffer.push_back(pair<bool,T>(false,_v));

	///////////////
	// unlock
	mutex_unlock();
	///////////////
	
	return 0;
}

// return value
// 0  : normal
// -1 : The quit request is already called
template <typename T, typename E>
int GTaskQue<T,E>::pushBack(const vector<T> &_v) {
	if (is_quit_requested) {
		cout << "quitThread is requested" << endl;
		return -1;
	}

	//////////////
	// lock
	mutex_lock();
	//////////////
	
	typename vector<T>::const_iterator citr = _v.begin();
	while (citr != _v.end()) {
		front_buffer.push_back(pair<bool, T>(false, (*citr)));
		citr++;
	}

	///////////////
	// unlock
	mutex_unlock();
	///////////////
	
	return 0;
}

// return value
// 0  : normal
// -1 : The quit request is already called
template <typename T, typename E>
int GTaskQue<T,E>::pushBack(const list<T> &_v) {
	if (is_quit_requested) {
		cout << "quitThread is requested" << endl;
		return -1;
	}

	//////////////
	// lock
	mutex_lock();
	//////////////
	
	typename list<T>::const_iterator citr = _v.begin();
	while (citr != _v.end()) {
		front_buffer.push_back(pair<bool, T>(false, (*citr)));
		citr++;
	}

	///////////////
	// unlock
	mutex_unlock();
	///////////////
	
	return 0;
}

// return
// 0 : OK (true, false)
// 1 : Execution is already running, so this call does not effect on it
// 2 : quitThread() is already requested, so this call does not effect on it
//
template <typename T, typename E>
int GTaskQue<T,E>::doAutoExecution(const bool &_v) {
	if(is_quit_requested && _v) {
		cout << "quitThread() is already called" <<endl;
		return 2;
	}

	assert(executor);
	if (!executor) {
		throw("There is no task");
	}

	// autoexecution is already running
	if (autoexecution_command && _v) {
		cout << "autoexecution is already running" <<endl;
		return 1;
	}

	// atomic bool
	autoexecution_command = _v;

	// false => autoexecution thread will be shut down
	if (autoexecution_command == false) {
		return 0;
	}
	
#ifdef _WIN32
	thread_handle_autoexecution =
		(HANDLE)_beginthreadex(NULL, 0, thread_function_autoexecution, this, 0, NULL);
#elif __linux__
	if (pthread_create(&thread_handle_autoexecution, 
		NULL, thread_function_autoexecution, this) != 0) {
		throw("pthread_create error");
	}
#endif

	return 0;
}

// return
// 0 : OK 
// throw : autoexecution is running or other tasks are running
template <typename T, typename E>
int GTaskQue<T,E>::doExecution() {
	assert(executor);
	if (!executor) {
		throw("There is no task");
	}

	assert(!this->is_autoexecution_running);
	if (this->is_autoexecution_running) {
		throw("Auto-execution is running, stop auto-execution first");
		return -1;
	}

	if (!areAllTasksExecuted()) {
		throw("Execution is running, wait until finish or stop the execution");
		return -1;
	}

#ifdef _WIN32
	thread_handle_autoexecution =
		(HANDLE)_beginthreadex(NULL, 0, thread_function_execution, this, 0, NULL);
#elif __linux__
	if (pthread_create(&thread_handle_autoexecution,
		NULL, thread_function_execution, this) != 0) {
		throw("pthread_create error");
	}
#endif

	return 0;
}

template <typename T, typename E>
bool GTaskQue<T,E>::areAllTasksExecuted() const {
	return (isBackBufferExecuted() && getFrontBufferSize() == 0)? true : false;
}

template <typename T, typename E>
void GTaskQue<T,E>::mutex_lock() {
	if (is_mutex_created) {
#ifdef _WIN32
		WaitForSingleObject(mutex, INFINITE);
#elif __linux__
		pthread_mutex_lock(&mutex);
#endif
	}
}

template <typename T, typename E>
void GTaskQue<T,E>::mutex_lock() const {
	if (is_mutex_created) {
#ifdef _WIN32
		WaitForSingleObject(mutex, INFINITE);
#elif __linux__
		pthread_mutex_lock(&mutex);
#endif
	}
}

template <typename T, typename E>
void GTaskQue<T,E>::mutex_unlock() {
	if (is_mutex_created) {
#ifdef _WIN32
		ReleaseMutex(mutex);
#elif __linux__
		pthread_mutex_unlock(&mutex);
#endif
	}
}

template <typename T, typename E>
void GTaskQue<T,E>::mutex_unlock() const {
	if (is_mutex_created) {
#ifdef _WIN32
		ReleaseMutex(mutex);
#elif __linux__
		pthread_mutex_unlock(&mutex);
#endif
	}
}

// private /////////////////////////////////////////////////////////////////

template <typename T, typename E>
int GTaskQue<T,E>::executeTask() {
	if (areAllTasksExecuted()) {
		cout << "There is no task to do" << endl;
		return -1;
	}

	// blocking call
	executor->execute(*(back_buffer[index_executor]));
	back_buffer[index_executor] = nullptr;
	index_executor++;
	
	if (index_executor >= getBackBufferSize()) {
		index_executor = 0;
	}
	
	return 0;
}

template <typename T, typename E>
int GTaskQue<T,E>::executeBatch() {
	if (index_executor >= getBackBufferSize()) {
		throw("Execution index is bigger than buffer size");
	}

	for (size_t i = index_executor;
		i < this->getBackBufferSize();
		i++) {
		if (back_buffer[i] == nullptr) {
			break;
		}
		
		// blocking call
		executor->execute(*(back_buffer[i]));
		back_buffer[i] = nullptr;
		index_executor++;
		
		if (delay_in_batch != 0) {
#ifdef _WIN32
			Sleep(delay_in_batch);
#elif __linux__
			usleep(delay_in_batch*USLEEP_SCALE_FACTOR);
#endif
		}
	}
	
	index_executor = 0;

	return 0;
}

template <typename T, typename E>
size_t GTaskQue<T,E>::copyToBackBuffer() {
	assert(isBackBufferExecuted());
	if (!isBackBufferExecuted()) {
		throw("BackBuffer is not executed yet");
	}

	size_t copy_count = 0;
	
	std::fill(back_buffer.begin(), back_buffer.end(), nullptr);

	index_executor = 0;

	typename list<pair<bool,T> >::iterator itr = front_buffer.begin();
	while (itr != front_buffer.end()) {
		if (itr->first == true) {
			front_buffer.pop_front();
			itr = front_buffer.begin();
			continue;
		}
		else {
			if (copy_count >= getBackBufferSize()) {
				break;
			}
			itr->first = true;
			back_buffer[copy_count] = &(itr->second);
			copy_count++;
		}
		++itr;
	}

	return copy_count;
}

template <typename T, typename E>
bool GTaskQue<T,E>::isBackBufferExecuted()const {
	for (size_t i = 0; i < getBackBufferSize(); i++) {
		if (back_buffer[i] != nullptr) {
			return false;
		}
	}
	return true;
}

#endif
