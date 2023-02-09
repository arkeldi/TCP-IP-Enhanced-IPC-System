#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

void patient_thread_function(int n, int p, BoundedBuffer *req_buf)
{
    for (int i = 0; i < n; i++)
    {
        datamsg d(p, i * 0.004, EGCNO);
        req_buf->push((char *)&d, sizeof(datamsg));
    }
}

void safe_read(TCPRequestChannel *rc, char *response, int nbytes)
{

	int read = 0;
	while (read < nbytes)
	{
		int read_bytes = rc->cread(response + read, nbytes - read);

		assert(read_bytes >= 0);
		read += read_bytes;
	}
	assert(read == nbytes);
}

void worker_thread_function(const char *ip, const char *port, BoundedBuffer *req_buf, BoundedBuffer *res_buf, int buf_cap)
{
    // Create local channel
    TCPRequestChannel chan(ip, port);

    // allocate memory to handle messages
    vector<char> request_v(buf_cap);
    vector<char> fileDataBuf_v(buf_cap);
    char *request = request_v.data();
    char *fileDataBuf = fileDataBuf_v.data();

    while (true)
    {
        int nbytes = req_buf->pop(request, buf_cap);
        MESSAGE_TYPE type = *(MESSAGE_TYPE *)request;
        chan.cwrite(request, nbytes);

        if (type == QUIT_MSG)
        {
            break;
        }
        else if (type == DATA_MSG)
        {
            double res = 0;
            chan.cread(&res, sizeof(double));
            pair<int, double> hist_data(((datamsg *)request)->person, res);
            res_buf->push((char *)&hist_data, sizeof(pair<int, double>));
        }
        else if (type == FILE_MSG)
        {
            string filename = request + sizeof(filemsg);
            int write_num = ((filemsg *)request)->length;
            int write_off = ((filemsg *)request)->offset;
            // chan.cread(fileDataBuf, write_num);
            safe_read(&chan, fileDataBuf, write_num);

            // Write to file the given section
            int fd = open(("./received/" + filename).c_str(), O_CREAT | O_WRONLY, 0666);
            assert(fd != -1);
            lseek(fd, write_off, SEEK_SET);
            write(fd, fileDataBuf, write_num);
            close(fd);
        }
    }
}

void histogram_thread_function(BoundedBuffer *res_buf, HistogramCollection *hc)
{
    /*
        Functionality of the histogram threads
    */
    pair<int, double> response_pair;
    while (true)
    {
        res_buf->pop((char *)&response_pair, sizeof(pair<int, double>));
        if (response_pair.first == -1)
        {
            break;
        }

        hc->update(response_pair.first, response_pair.second);
    }
}

void file_thread_function(string filename, BoundedBuffer *req_buf, TCPRequestChannel *chan, int buf_cap)
{
    /*
        Use lseek by f.offset to write to the output file
    */
    if (filename == "")
    {
        return;
    }
    filemsg fm(0, 0);
    int len = sizeof(filemsg) + filename.size() + 1;
    vector<char> msg(len);

    memcpy(msg.data(), &fm, sizeof(filemsg));
    std::strcpy(msg.data() + sizeof(filemsg), filename.c_str());

    // Send file request
    chan->cwrite(msg.data(), len);

    // Get response
    __int64_t filelen;
    chan->cread(&filelen, sizeof(__int64_t));

    int count = ceil((double)filelen / buf_cap);
    filemsg *buffer_reqs = (filemsg *)msg.data();
    for (int i = 0; i < count; i++)
    {
        double currentPos = i * buf_cap;
        if (filelen - currentPos < buf_cap)
        {
            buf_cap = filelen - currentPos;
        }
        buffer_reqs->length = buf_cap;
        buffer_reqs->offset = currentPos;
        req_buf->push(msg.data(), len);
    }
}

int main(int argc, char *argv[])
{

    int opt;
    int n = 15000;
    int p = 1;
    int h = 100;
    int w = 100;
    char *ip = NULL;
    char *port = NULL;
    __int64_t buffercapacity = MAX_MESSAGE;

    string filename = "";
    bool transfer = false;
    int b = 10; // size of bounded buffer, note: this is different from another variable buffercapacity/m
    // take all the arguments first because some of these may go to the server
    while ((opt = getopt(argc, argv, "f:n:p:h:w:m:b:a:r:")) != -1)
    {
        switch (opt)
        {
        case 'f':
            filename = optarg;
            transfer = true;
            break;
        case 'n':
            n = stoi(optarg);
            break;
        case 'p':
            p = stoi(optarg);
            break;
        case 'h':
            h = stoi(optarg);
            break;
        case 'w':
            w = stoi(optarg);
            break;
        case 'm':
            buffercapacity = stoi(optarg);
            break;
        case 'b':
            b = stoi(optarg);
            break;
        case 'a':
            ip = strdup(optarg);
            break;
        case 'r':
            port = strdup(optarg);
            break;
        }
    }

    assert(ip != NULL);
    assert(port != NULL);

    TCPRequestChannel chan(ip, port);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
    HistogramCollection hc;

    // initialize histograms
    for (int i = 0; i < p; i++)
    {
        Histogram *h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }

    struct timeval start, end;
    gettimeofday(&start, 0);

    /* Start all threads here */
    vector<thread> patients;
    if (!transfer)
    {
        for (int i = 0; i < p; i++)
        {
            patients.push_back(thread(patient_thread_function, n, i + 1, &request_buffer));
        }
    }

    vector<thread> workers;
    vector<TCPRequestChannel *> workchans;
    for (int i = 0; i < w; i++)
    {

        workers.push_back(thread(worker_thread_function, ip, port, &request_buffer, &response_buffer, buffercapacity));
    }

    // Start file thread after the workers given that the same channel is used as when creating them
    thread filethread(file_thread_function, filename, &request_buffer, &chan, buffercapacity);

    vector<thread> histograms;
    if (!transfer)
    {
        for (int i = 0; i < h; i++)
        {
            histograms.push_back(thread(histogram_thread_function, &response_buffer, &hc));
        }
    }

    /* Join all threads here */
    if (!transfer)
    {
        for (int i = 0; i < p; i++)
        {
            patients[i].join();
        }
    }
    filethread.join();

    for (int i = 0; i < w; i++)
    {
        MESSAGE_TYPE q(QUIT_MSG);
        request_buffer.push((char *)&q, sizeof(MESSAGE_TYPE));
    }

    for (int i = 0; i < w; i++)
    {
        workers[i].join();
    }

    // push sentinal pair to response buffer
    if (!transfer)
    {
        for (int i = 0; i < h; i++)
        {
            pair<int, double> quit_req(-1, -1.0);
            response_buffer.push((char *)&quit_req, sizeof(pair<int, double>));
        }
    }

    if (!transfer)
    {
        for (int i = 0; i < h; i++)
        {
            histograms[i].join();
        }
    }

    gettimeofday(&end, 0);

    // print the results and time difference
    if (filename == "")
    {
        hc.print();
    }

    int millis = (int)(end.tv_sec * 1e3 + end.tv_usec / 1e3 - start.tv_sec * 1e3 - start.tv_usec / 1e3);
    cout << millis << "ms" << endl;

    // closing the channel
    MESSAGE_TYPE q(QUIT_MSG);
    chan.cwrite(&q, sizeof(MESSAGE_TYPE));
    // client waiting for the server process, which is the child, to terminate
    wait(0);
    cout << "Client process exited" << endl;

    free(port);
    free(ip);
}
