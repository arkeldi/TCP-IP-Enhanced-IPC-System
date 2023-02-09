#include "BoundedBuffer.h"
#include <iostream>

using namespace std;

BoundedBuffer::BoundedBuffer(int _cap) : cap(_cap)
{
    // modify as needed
}

BoundedBuffer::~BoundedBuffer()
{
    // modify as needed
}

void BoundedBuffer::push(char *msg, int size)
{

    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    // use one of the vector constructors
    std::vector<char> vec(msg, msg + size);
    
    {
        unique_lock<mutex> lock(mux);

        // 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
        // waiting on the slot avaliable
        slot.wait(lock, [this]
                  { return static_cast<int>(this->size()) < cap; });

        // 3. Then push the vector at the end of the queue
        q.push(vec);
    }

    // 4. Wake up threads that were waiting for push
    data.notify_one();
}

int BoundedBuffer::pop(char *msg, int size)
{

    std::vector<char> front;

    // Remove object from queue
    {
        unique_lock<mutex> lock(mux);

        // 1. Wait until the queue has at least 1 item
        // waiting on data avaiable
        data.wait(lock, [this]
                  { return this->size() > 0; });

        // 2. Pop the front item of the queue. The popped item is a vector<char>
        front = q.front();
        q.pop();
    }
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    assert((int)front.size() <= size);
    memcpy(msg, front.data(), front.size());

    // 4. Wake up threads that were waiting for pop
    slot.notify_one();

    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return front.size();
}

size_t BoundedBuffer::size()
{
    return q.size();
}
