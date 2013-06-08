#include <pthread.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <unistd.h>
#include <sys/time.h>

using boost::function;
using boost::bind;
using namespace std;

/**
 * Wrapper for interfacing with C-style pthreads library (so C linkage may or
 * may not be required for callbacks)
 *
 * The parameter arg should always be of type boost::function<void*()>* .
 */
extern "C" void* pthreadWrapper(void* arg);

/**
 * Template for supporting asynchronous sensor updating with arbitrary sensor
 * and packet classes. Provides separate threads for sensor communication, data
 * retrieval, and processing operations. Ensures thread-safety.
 *
 * There are two template parameters. The first one is the packet that contains
 * the sensor data, the second is a sensor interface that must provide a
 * function with signature `Packet getPacket()` that communicates with the
 * sensor and returns the resulting data in a Packet. The Packet class must
 * have a sensible copy-constructor and operator= defined.
 */
template <class Packet, class Interface>
class BufferThread {

    private:
    pthread_mutex_t upfl_mtx;
    pthread_mutex_t data_mtx;
    pthread_cond_t read_cond;
    pthread_t read_thread;

    Interface* source;
    Packet pkt;
    bool bUpdating;
    function<void*()>* tfPersistent;

    public:
    BufferThread(Interface* source) : source(source) {
        // Multithreading construct initialization and thread spawning
        pthread_mutex_init(&upfl_mtx, NULL);
        pthread_mutex_init(&data_mtx, NULL);
        pthread_cond_init(&read_cond, NULL);
        
        tfPersistent = NULL;

        bUpdating = false;
    }

    ~BufferThread() {
        // Stop the thread and take care of the extra pthreads destruction
        // requirements
        pthread_cancel(read_thread);
        pthread_join(read_thread, NULL);

        pthread_cond_destroy(&read_cond);
        pthread_mutex_destroy(&data_mtx);
        pthread_mutex_destroy(&upfl_mtx);

        delete tfPersistent;
    }

    /**
     * This should be called to start the background sensor-communication
     * threads; ideally, this would be called before the first invocation
     * of readData().
     *
     * This function should only be called once per object. The result of
     * multiple invocations on a single object is undefined and may cause
     * memory leaks.
     *
     * This function should also not be called together with runContinuous()
     * on a single object; the result is also strictly speaking undefined,
     * but will most likely result in multiple threads all clamoring to update
     * and calls to readData() essentially being ignored. Also, the result of
     * isUpdating() would likely not make sense anymore.
     */
    void spawnThreads() {
        function<void*()> thrFun = bind(&BufferThread::threadMeth, this);
        tfPersistent = new function<void*()>(thrFun);
        pthread_create(&read_thread, NULL, &pthreadWrapper, tfPersistent);
    }

    /**
     * This should be called to start the background threads in continuous
     * operation mode. This means the threads do not wait for readData() to
     * update the packet, they just run another update once the previous one
     * is done. This could be useful for reading from data streams that are
     * constantly populated, where updating only on an external schedule could
     * potentially cause I/O buffers to overfill. Note that isUpdating() will
     * always return true once the thread has started, since the thread is
     * constantly updating.
     *
     * This function should only be called once per object. The result of
     * multiple invocations on a single object is undefined and may cause
     * memory leaks.
     *
     * This function should also not be called together with spawnThreads()
     * on a single object; the result is also strictly speaking undefined,
     * but will most likely result in multiple threads all clamoring to update
     * and calls to readData() essentially being ignored. Also, the result of
     * isUpdating() would likely not make sense anymore.
     */
    void runContinuous() {
        function<void*()> thrFun = bind(&BufferThread::tmContinuous, this, 0);
        tfPersistent = new function<void*()>(thrFun);
        pthread_create(&read_thread, NULL, &pthreadWrapper, tfPersistent);
    }


    Packet getPacket() {
        /* All data-access sections must have these lock/unlock guards.
         * They protect against access to the data while it is being modified.
         */
        Packet pkl;
        pthread_mutex_lock(&data_mtx);
        pkl = pkt; // Make a local copy to ensure correctness and safety
        pthread_mutex_unlock(&data_mtx);
        return pkl;
    }

    bool isUpdating() {
        /* This silly dance helps ensure thread safety.
         *
         * Yes, this is just a read of a boolean variable, but without the
         * locks there is no guarantee that this read instruction won't be
         * reordered by an aggressively optimizing compiler to some place that
         * makes the program's semantics invalid.
         *
         * Trust me, multi-threading is tricky business.
         */
        bool retval;
        pthread_mutex_lock(&upfl_mtx);
        retval = bUpdating;
        pthread_mutex_unlock(&upfl_mtx);
        return retval;
    }

    void readData() {
        pthread_mutex_lock(&upfl_mtx);
        if (bUpdating) {
            // Will not initiate an update while another is in progress.
            pthread_mutex_unlock(&upfl_mtx);
            return;
        } else {
            bUpdating = true;
            pthread_mutex_unlock(&upfl_mtx);
            pthread_cond_signal(&read_cond);
        }
    }

    /**
     * The updater thread function. This function is not meant to return; it
     * simply runs until the thread is canceled.
     *
     * It is called from an external wrapper function.
     */
    void* threadMeth() {
        bool bUpl; // Thread-local updating flag
        Packet pkl; // Thread-local packet
        while (true) {
            pthread_mutex_lock(&upfl_mtx);
            pthread_cond_wait(&read_cond, &upfl_mtx);
            /* Keep the critical section as short as possible (we don't want
             * to block callers of isUpdating() ).
             */
            bUpl = bUpdating;
            pthread_mutex_unlock(&upfl_mtx);

            if (bUpl) { // Is it OK to proceed?
                /* This is the actual bulk of the update functionality.
                 * It can (and should) be delegated to separate functions.
                 */

                // Communicate with the sensor
                pkl = source->getPacket();

                /* Keep the section in between the lock guards (i.e. the
                 * "critical section") as short and fast as possible. It should
                 * consist only of copying the data received from the sensor
                 * into the internal buffer variables.
                 *
                 * The reason is that other threads (like the main thread) may
                 * want to access data using the get-functions while this
                 * update is happening. If the locked section takes too long,
                 * that thread will be made to wait, which is not a good thing.
                 */
                pthread_mutex_lock(&data_mtx);
                // Update cached data
                pkt = pkl;
                pthread_mutex_unlock(&data_mtx);

                // Pthreads should ensure that these two code blocks are not
                // reordered with respect to each other.

                // Report that we are done updating.
                pthread_mutex_lock(&upfl_mtx);
                bUpdating = false;
                pthread_mutex_unlock(&upfl_mtx);
            }
        }
        // We'll never get here, but whatever keeps the compiler happy.
        return NULL;
    }

    /**
     * The updater thread function for continuous operation. Again, this
     * function is not meant to return; it simply runs until the thread
     * is canceled.
     *
     * \param intervalMs The minimum time between updates. If zero, the updates
     *        will immediately follow one another.
     *
     * Note that the interval-sleeping capability has not yet been implemented.
     *
     * It is called from an external wrapper function.
     */
    void* tmContinuous(int intervalMs) {
        // Basically the same as above, only we don't wait for readData.
        bool bUpl; // Thread-local updating flag
        Packet pkl; // Thread-local packet
        // We're constantly updating, so this flag just stays true.
        pthread_mutex_lock(&upfl_mtx);
        bUpl = true;
        pthread_mutex_unlock(&upfl_mtx);

        while (true) {

            // Communicate with the sensor
            pkl = source->getPacket();

            /* Keep the section in between the lock guards (i.e. the
             * "critical section") as short and fast as possible. It should
             * consist only of copying the data received from the sensor
             * into the internal buffer variables.
             *
             * The reason is that other threads (like the main thread) may
             * want to access data using the get-functions while this
             * update is happening. If the locked section takes too long,
             * that thread will be made to wait, which is not a good thing.
             */
            pthread_mutex_lock(&data_mtx);
            // Update cached data
            pkt = pkl;
            pthread_mutex_unlock(&data_mtx);

            // Cancellation point, just to be sure
            // TODO add timed loop capability
            sleep(0);
        }
        // We'll never get here, but whatever keeps the compiler happy.
        return NULL;
    }
};

/**
 * Definition of the callback function.
 *
 * Note that this should only occur once in the final repository. Place the
 * definition of this function somewhere separate from the buffer class.
 */
void* pthreadWrapper(void* arg) {
    // Aaugh! My eyes!
    function<void* ()>* tFun = static_cast<function<void* ()>* >(arg);
    // The responsibility for destruction of tFun lies with the class that
    // created it (destructor).
    return (*tFun)();
}

