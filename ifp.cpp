#include <iostream>
#include <stdio.h>
#include <vector>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <memory>
#include <stack>
#include <stdexcept>
#include <sstream>
#include <time.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

using namespace std;


class threadpool 
{
    private:
    vector<thread> workers;
    queue<function<void()> > tasks;

    mutex queue_mutex;
    condition_variable cv;
    bool stop;
	int64 start;

    public:
    threadpool(size_t);
    template<class F, class... Args> 
    auto enqueue(F&& f, Args&&... args) -> future<typename result_of<F(Args...)>::type>; 
    ~threadpool();
};

inline threadpool::threadpool (size_t nworkers) :stop(false)
{
	start =cv::getTickCount();
    for (size_t i = 0; i < nworkers; i++)
    {
        workers.emplace_back
        (
            [this]
            {
                 while(1)
                {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(this->queue_mutex);
                        this->cv.wait(lock, [this]
                        {
                            return this->stop || !this->tasks.empty();
                        });
                        if (this->stop && this->tasks.empty())
                        {
                            return;
                        }
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            }
        );
    }
}

template <class F, class... Args>
auto threadpool::enqueue(F&& f, Args&&... args)
->std::future<typename std::result_of<F(Args...)>::type>
{
	using return_type = typename std::result_of<F(Args...)>::type;
	auto task = std::make_shared<std::packaged_task<return_type()> >
	(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
	);
	std::future<return_type> res = task->get_future(); 
	{
		unique_lock<mutex> lock(queue_mutex);
		if (stop)
		{
			throw runtime_error("Enqueue on stopped thread");
		}
		tasks.emplace([task](){ (*task)();});
	}
	cv.notify_all();
	return res;
}

inline threadpool::~threadpool()
{
	{
		unique_lock<mutex> lock(queue_mutex);
		stop = true;
	}
	cv.notify_all();
	for (std::thread &w: workers)
	{
		w.join();
	}
	int64 t = cv::getTickCount() - start;
	cout << "Running time = " << (double)t/cv::getTickFrequency() << " s" << endl;
}

void usage()
{
    cout << "ifp pardegree destdir /home/marcod/brina/*.jpg" << endl; 
}

int main (int argc, char** argv)
{
	if (argc < 4)
	{
		usage();
		return -1;
	}
	int nw = atoi(argv[1]);
	char* destpath = argv[2];
	cout << "Pardegree = " << nw << endl;
	cout << "Destination = " << argv[2] << endl;

	threadpool pool(nw);
	int count = 0;
	for (int k = 0; k < 10; k++)
	{
		for(int i = 3; i < argc; i++)
		{
			// cout << argv[i] << endl;
			auto res = pool.enqueue([&](char* path) 
				{
					// cout << "path is " << path << endl;
					cv::Mat img = cv::imread(path);
					// cout << "Img size = " << img.rows << " " << img.cols << endl;
					cv::Mat dest;
					cv::GaussianBlur(img, dest, cv::Size(3, 3), 0, 0, cv::BORDER_DEFAULT);
					cv::Sobel(dest, dest, -1, 1, 1);
					stringstream ss;
					ss << destpath << "/" << count << ".jpg";
					// cout << ss.str() << endl;
					count ++;
					cv::imwrite(ss.str(), dest);
				}, argv[i]);

		}
	}
	return 0;
}