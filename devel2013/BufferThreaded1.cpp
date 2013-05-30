#include <pthread.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <unistd.h>

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
 * Demonstrative example for asynchronous sensor updating with a separate
 * thread for the sensor communication and data retrieval and processing
 * operations. Uses condition variables and ensures thread-safety.
 *
 * The extra thread is started by calling spawnThreads(). It is stopped by
 * destroying the BufferThreaded object.
 *
 * If you use this, please copy the declarations to a separate header file so
 * your class can be included by others.
 */
class BufferThreaded {

    private:
    pthread_mutex_t upfl_mtx;
    pthread_mutex_t data_mtx;
    pthread_cond_t read_cond;
    pthread_t read_thread;

    bool bUpdating;
    int *data;
    int numItems;

    int readFromSensor(int** dataIn) {
        // Dummy data-filling method.
        ++numItems;
        *dataIn = new int[numItems];
        for (int i = 0; i < numItems; i++) {
            (*dataIn)[i] = i;
        }
        return numItems;
    }

    public:
    BufferThreaded() {
        // Multithreading construct initialization and thread spawning
        pthread_mutex_init(&upfl_mtx, NULL);
        pthread_mutex_init(&data_mtx, NULL);
        pthread_cond_init(&read_cond, NULL);

        bUpdating = false;
        numItems = 0;
        data = NULL;
    }

    ~BufferThreaded() {
        // Stop the thread and take care of the extra pthreads destruction
        // requirements
        pthread_cancel(read_thread);
        pthread_join(read_thread, NULL);

        pthread_cond_destroy(&read_cond);
        pthread_mutex_destroy(&data_mtx);
        pthread_mutex_destroy(&upfl_mtx);

        // Just to be sure...
        delete[] data;
    }

    /**
     * This should be called to start the background sensor-communication
     * threads; ideally, this would be called before the first invocation
     * of readData().
     */
    void spawnThreads() {
        function<void*()> thrFun = bind(&BufferThreaded::threadMeth, this);
        function<void*()>* tfPersistent = new function<void*()>(thrFun);
        pthread_create(&read_thread, NULL, &pthreadWrapper, tfPersistent);
    }

    int getData(int** dataIn) {
        // All data-access sections must have these lock/unlock guards.
        // They protect against access to the data while it is being modified.
        pthread_mutex_lock(&data_mtx);
        *dataIn = new int[numItems];
        for (int i = 0; i < numItems; i++) {
            (*dataIn)[i] = data[i];
        }
        pthread_mutex_unlock(&data_mtx);
        return numItems;
    }

    bool isUpdating() {
        // This silly dance helps ensure thread safety.
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
        while (true) {
            pthread_mutex_lock(&upfl_mtx);
            pthread_cond_wait(&read_cond, &upfl_mtx);
            /* Keep the critical section as short as possible (we don't want
             * to block callers of isUpdating() ).
             */
            bool bUpl = bUpdating;
            pthread_mutex_unlock(&upfl_mtx);

            if (bUpl) { // Is it OK to proceed?
                /* This is the actual bulk of the update functionality.
                 * It can (and should) be delegated to separate functions.
                 */

                // Communicate with the sensor
                /* This simulates sensor latency -- remove in actual code!
                 */
                sleep(5);

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
                delete[] data;
                numItems = readFromSensor(&data);
                pthread_mutex_unlock(&data_mtx);

                // Report that we are done updating.
                pthread_mutex_lock(&upfl_mtx);
                bUpdating = false;
                pthread_mutex_unlock(&upfl_mtx);
            }
        }
    }
};

/* Definition of the callback function.
 */
void* pthreadWrapper(void* arg) {
    // Aaugh! My eyes!
    function<void* ()>* tFun = static_cast<function<void* ()>* >(arg);
    return (*tFun)();
}

/**
 * Interactive testing of the functionality of the threaded buffer.
 */
int main(int argc, char** argv) {
    bool bContinue = true;
    bool bReset = false;
    char cmd;
    int *data;
    int numItems;
    BufferThreaded* buf;
    buf = new BufferThreaded();
    buf->spawnThreads();
    while (bContinue) {
        if (bReset) {
            bReset = false;
            delete(buf); // This allows testing of clean and safe deletion
            buf = new BufferThreaded();
            buf->spawnThreads();
        }
        cout << "Please enter a command out of " <<
            "{\'u\', \'g\', \'i\', \'r\', \'q\'}: ";
        cin >> cmd;
        cout << endl;
        switch (cmd) {
        case 'u':
            // Update.
            buf->readData();
            break;
        case 'g':
            // Get data.
            numItems = buf->getData(&data);
            cout << "Got data: [";
            for (int i = 0; i < numItems; i++) {
                cout << data[i] << ", ";
            }
            cout << "]" << endl;
            delete(data);
            break;
        case 'i':
            // Inquiry, isUpdating.
            cout << "Updating: " << buf->isUpdating() << endl;
            break;
        case 'r':
            // Reset state.
            cout << "Resetting buffer." << endl;
            bReset = true;
            break;
        case 'q':
            // Quit.
            bContinue = false;
            break;
        default:
            cout << "Unknown command. Exiting." << endl;
            return -1;
        }
    }
    return 0;
}

