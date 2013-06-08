#include <pthread.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <unistd.h>
#include <sys/time.h>
#include <utility>

using boost::function;
using boost::bind;

#include "BufferThreadedP.h" // for the pthreadWrapper definition

// Header guards -- this file may be included more than once.
#ifndef IOBUFFER_H_
#define IOBUFFER_H_

/**
 * Wrapper for interfacing with C-style pthreads library (so C linkage may or
 * may not be required for callbacks)
 *
 * The parameter arg should always be of type boost::function<void*()>* .
 */
extern "C" void* pthreadWrapper(void* arg);

/**
 * Template for supporting asynchronous updating with arbitrary interface
 * and packet classes. Provides separate threads intended primarily for
 * processing, such as running expensive map or search operations. Ensures
 * thread-safety.
 *
 * This template uses move semantics wherever possible to avoid unnecessary
 * copying of potentially large data structures. Note that the use of move
 * semantics implies that the buffer's packet caches may be invalidated by
 * moves; callers should be prepared to keep a copy of the output packet in
 * case the buffer's packet is invalid when the caller attempts to access it.
 *
 * There are three template parameters. The first one is the packet that
 * contains the input data, the second is the packet type for the process's
 * output, and the third is an interface that must provide a function with
 * signature `OutputPacket runProcess(InputPacket)` that starts the process,
 * providing relevant data in an ImputPacket, and returns the resulting data
 * in an OutputPacket. The Packet classes must have sensible copy-constructors
 * and operator= defined. In addition, it is recommended that large Packets
 * have sensible move semantics ( operator=(const Packet&& other) )
 *
 * TODO This could potentially be done better with unique_ptr functionality,
 * rather than packet move semantics.
 */
template <class InputPacket, class OutputPacket, class Interface>
class IOBuffer {

    private:
    pthread_mutex_t upfl_mtx;
    pthread_mutex_t idata_mtx;
    pthread_mutex_t odata_mtx;
    pthread_cond_t newipt;
    pthread_t read_thread;

    Interface* source;
    InputPacket ipkt;
    OutputPacket opkt;
    bool idata_new; // Also tells whether the object is valid.
    bool odata_new; // If it's new, it's valid; if it's not, it may be consumed.

    bool bUpdating;
    function<void*()>* tfPersistent;

    public:
    IOBuffer(Interface* source) : source(source) {
        // Multithreading construct initialization and thread spawning
        pthread_mutex_init(&upfl_mtx, NULL);
        pthread_mutex_init(&idata_mtx, NULL);
        pthread_mutex_init(&odata_mtx, NULL);
        pthread_cond_init(&newpkt_cond, NULL);

        tfPersistent = NULL;

        bUpdating = false;
    }

    ~IOBuffer() {
        // Stop the thread and take care of the extra pthreads destruction
        // requirements
        pthread_cancel(read_thread);
        pthread_join(read_thread, NULL);

        pthread_mutex_destroy(&idata_mtx);
        pthread_mutex_destroy(&odata_mtx);
        pthread_mutex_destroy(&upfl_mtx);

        delete tfPersistent;
    }

    /**
     * This should be called to start the background threads in continuous
     * operation mode. This means the threads do not wait for readData() to
     * update the packet, they just run another update once the previous one
     * is done. Note that isUpdating() will always return true once the thread
     * has started, since the thread is constantly updating.
     *
     * This function should only be called once per object. The result of
     * multiple invocations on a single object is undefined and may cause
     * memory leaks.
     */
    void runContinuous() {
        function<void*()> thrFun = bind(&BufferThread::tmContinuous, this, 0);
        tfPersistent = new function<void*()>(thrFun);
        pthread_create(&read_thread, NULL, &pthreadWrapper, tfPersistent);
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

    /**
     * Tells whether the input data packet has not already been consumed,
     * so callers can potentially save themselves a copy operation.
     */
    bool isInputUnsed() {
        bool retval;
        pthread_mutex_lock(&idata_mtx);
        retval = idata_new;
        pthread_mutex_unlock(&idata_mtx);
        return retval;
    }

    /**
     * Tells whether a new output packet is available, so callers do not
     * inadvertently get a destroyed packet.
     */
    bool isOutputNew() {
        bool retval;
        pthread_mutex_lock(&odata_mtx);
        retval = odata_new;
        pthread_mutex_unlock(&odata_mtx);
        return retval;
    }


    void providePacket(InputPacket input) {
        pthread_mutex_lock(&idata_mtx);
        // Using move semantics so that the input packet isn't unncecessarily
        // copied
        ipkt = std::move(input);
        idata_new = true;
        pthread_mutex_unlock(&idata_mtx);
        pthread_cond_signal(&newipt);
    }

    /**
     * Copies an output packet to the provided address and returns true, if a
     * new packet is available. If not, false is returned and the input address
     * is left unchanged.
     */
    bool getPacket(OutputPacket* output) {
        /* All data-access sections must have these lock/unlock guards.
         * They protect against access to the data while it is being modified.
         */
        bool retval = true;
        pthread_mutex_lock(&odata_mtx);
        if (odata_new) {
            *output = std::move(opkt);
            // Important: The buffer's output packet is no longer valid.
            odata_new = false;
            retval = true;
        } else {
            retval = false;
        }
        pthread_mutex_unlock(&odata_mtx);

        return retval;
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

        InputPacket ipkl; // Thread-local packets
        OutputPacket opkl;
        // We're constantly updating, so this flag just stays true.
        pthread_mutex_lock(&upfl_mtx);
        bUpdating = true;
        pthread_mutex_unlock(&upfl_mtx);

        while (true) {

            // Consume the input packet. This operation invalidates the
            // existing InputPacket.
            pthread_mutex_lock(&ipkt);
            while (!idata_new) {
                pthread_cond_wait(&newipt)
            }
            ipkl = std::move(ipkt);
            idata_new = false;
            pthread_mutex_unlock(&ipkt);

            opkl = source->runProcess(ipkl);

            pthread_mutex_lock(&odata_mtx);
            // Update cached packet, mark it new and valid.
            opkt = std::move(opkl);
            odata_new = true;
            pthread_mutex_unlock(&odata_mtx);

            // Cancellation point, just to be sure
            // TODO add timed loop capability
            sleep(0);
        }
        // We'll never get here, but whatever keeps the compiler happy.
        return NULL;
    }
};

