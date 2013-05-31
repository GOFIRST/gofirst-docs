#include "BufferThreadedP.h"
#include <vector>
#include <iostream>

using namespace std;

class TestPacket {
    int* data;
    size_t numItems;
    timeval tStamp;

    public:
    TestPacket() : data(NULL), numItems(0) {
        gettimeofday(&tStamp, NULL);
    }

    /**
     * Construct a packet with pre-existing data. Note that the data array
     * must be allocated on the heap to ensure it has the correct lifetime.
     *
     * This constructor implicitly transfers ownership of the data array to
     * this class. The array will be destroyed upon packet destruction.
     */
    TestPacket(int* data, size_t numItems, timeval tStamp) :
        data(data), numItems(numItems), tStamp(tStamp) {}

    TestPacket(const TestPacket& other) {
        tStamp = other.tStamp;
        // Deep copy of the data array
        numItems = other.numItems;
        data = new int[numItems];
        for (int i = 0; i < numItems; i++) {
            data[i] = other.data[i];
        }
    }

    ~TestPacket() {
        delete[] data;
    }

    TestPacket operator=(const TestPacket& other) {
        tStamp = other.tStamp;
        // Deep copy of the data array
        numItems = other.numItems;
        data = new int[numItems];
        for (int i = 0; i < numItems; i++) {
            data[i] = other.data[i];
        }
        return *this;
    }

    friend ostream& operator<<(ostream& st, const TestPacket& pkt);

    std::vector<int> getData() {
        std::vector<int> ovec(data, data + numItems);
        return ovec;
    }

    timeval getTimeStamp() {
        return tStamp;
    }
};

ostream& operator<<(ostream& st, const TestPacket& pkt) {
    st << "Data: [";
    for (int i = 0; i < pkt.numItems; i++) {
        st << pkt.data[i] << ", ";
    }
    st << "]" << endl;
    st << "Timestamp: " << ctime(&(pkt.tStamp.tv_sec)) <<
        pkt.tStamp.tv_usec << " ms" << endl;
    return st;
}

class TestInterface {
    size_t numItems;

    public:
    TestInterface() : numItems(0) {}

    TestPacket getPacket() {
        // Simulate sensor latency
        sleep(3);
        ++numItems;
        int* data = new int[numItems];
        for (int i = 0; i < numItems; i++) {
            data[i] = i;
        }
        timeval tStamp;
        gettimeofday(&tStamp, NULL);
        TestPacket pkt(data, numItems, tStamp);
        return pkt;
    }
};


/**
 * Interactive testing of the functionality of the threaded buffer.
 */
int main(int argc, char** argv) {
    bool bContinue = true;
    char cmd;
    TestPacket tp;
    TestInterface iface;

    BufferThread<TestPacket, TestInterface>* buf;
    buf = new BufferThread<TestPacket, TestInterface>(&iface);
    buf->spawnThreads();
    while (bContinue) {
        cout << "Please enter a command out of " <<
            "{\'u\', \'g\', \'i\', \'q\'}: ";
        cin >> cmd;
        cout << endl;
        switch (cmd) {
        case 'u':
            // Update.
            buf->readData();
            break;
        case 'g':
            // Get data.
            tp = buf->getPacket();
            cout << "Got packet: " << endl << tp;
            break;
        case 'i':
            // Inquiry, isUpdating.
            cout << "Updating: " << buf->isUpdating() << endl;
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

