#include <pthread.h>

/**
 * Demonstrative example for asynchronous sensor updating with a separate
 * thread for the sensor communication and data retrieval and processing
 * operations. Uses condition variables and ensures thread-safety.
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
        int numItems = 4;
        *dataIn = new int[numItems];
        for (int i = 0; i < numItems; i++) {
            (*dataIn)[i] = 5; // For example...
        }
        return numItems;
    }

    public:
    BufferThreaded() {
        // Multithreading construct initialization and thread spawning
        pthread_mutex_init(&upfl_mtx, NULL);
        pthread_mutex_init(&data_mtx, NULL);
        pthread_cond_init(&read_cond, NULL);

        pthread_create(&read_thread, NULL, &threadCB, this);

        bUpdating = false;
        numItems = 0;
    }

    ~BufferThreaded() {
        // Stop the thread and take care of the extra pthreads destruction
        // requirements
        pthread_cancel(read_thread);

        pthread_cond_destroy(&read_cond);
        pthread_mutex_destroy(&data_mtx);
        pthread_mutex_destroy(&upfl_mtx);
    }

    int getData(int** dataIn) {
        // These lock/unlock statements MUST be here.
        // This is a critical section.
        pthread_mutex_lock(&data_mtx);
        *dataIn = new int[numItems];
        for (int i = 0; i < numItems; i++) {
            (*dataIn)[i] = data[i];
        }
        pthread_mutex_unlock(&data_mtx);
        return numItems;
    }

    bool isUpdating() {
        // This silly dance is required for thread safety.
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
     * simply runs until canceled.
     *
     * It is called from a static callback function.
     */
    void* threadMeth() {
        while (true) {
            pthread_mutex_lock(&upfl_mtx);
            pthread_cond_wait(&read_cond, &upfl_mtx);
            if (bUpdating) { // Is it OK to proceed?
                /* This is the actual bulk of the update functionality.
                 * It can (and should) be delegated to separate functions.
                 */

                // Communicate with the sensor
                pthread_mutex_lock(&data_mtx);
                // Update cached data
                numItems = readFromSensor(&data);
                pthread_mutex_unlock(&data_mtx);
            }
            pthread_mutex_unlock(&upfl_mtx);
        }
    }

    /**
     * Boilerplate callback function for thread creation
     * (pthread_create needs a static function)
     *
     * Takes as an argument the object on which the main thread method
     * threadMeth() should be called.
     */
    static void* threadCB(void* arg) {
        return static_cast<BufferThreaded*>(arg)->threadMeth();
    }

}






