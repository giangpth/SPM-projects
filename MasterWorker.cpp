#include <iostream>
#include <ff/ff.hpp>
#include <ff/pipeline.hpp>
#include <ff/farm.hpp>
#include <vector>
#include <cmath>
#include <queue>

using namespace ff;
using namespace std;
using ull = unsigned long long;



static bool is_prime(ull n) 
{
    if (n <= 3)  return n > 1; // 1 is not prime !
    
    if (n % 2 == 0 || n % 3 == 0) return false;

    for (ull i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) 
            return false;
    }
    return true;
}

int nw = 1;
int toprint = 1;

struct master:ff_node_t< int, int > 
{
    master(int begin, int end)
    {
        wholerange.first = begin;
        wholerange.second = end;
        result.reserve((size_t)(end-begin)/log(begin));
        // cout << "My range: " << wholerange.first << " " << wholerange.second << endl;
    }
    int* svc(int* r)
    {
        ull part = (wholerange.second - wholerange.first)/(nw + 1);
        // cout << "Each part: " << part << endl;
        if(r == nullptr)
        {
            //send job
            for (int i = 0; i < nw; i++)
            {
                int*  t = new int [2];
                t[0]= wholerange.first + i*part; t[1] = wholerange.first + (i+1)*part - 1;
                // cout << "To send: " << t[0] << " " << t[1] << endl;
                ff_send_out(t);
                // cout << "Send out task " << endl;
            }
            //do my job
            // cout << "Master job from " << wholerange.first + nw*part << " to " << wholerange.second << endl;
            for (int i = wholerange.first + nw*part; i <=  wholerange.second; i++)
            {
                if (is_prime(i))
                {
                    // cout << "Calculate from master " << i << " is prime" << endl;
                    result.push_back(i);
                }
            }
            return GO_ON;
        }
        int &tem = *r;
        // cout << "Master receive result: " << tem << endl; 
        result.push_back(tem);
        delete r;
        if (onthefly == 0)
        {
            return EOS;
        }
        return GO_ON;
    }
    void eosnotify(ssize_t id)
    {
        // cout <<"Master receive on EOS" << endl;
        onthefly--;
    }
    void svc_end()
    {
        // cout << "Master end, final result: " << endl;
        if (toprint == 1)
        {
            sort(result.begin(), result.end());
            cout << "Final result " << endl;
            for (int i = 0; i < result.size(); i++)
            {
                cout << result[i] << " ";
            }
            cout << endl;
        }
    }
    pair<int, int> wholerange;
    int onthefly = nw;
    // std::priority_queue<int, std::vector<int>, std::greater<int> > result;;
    vector<ull> result;
};

struct worker:ff_node_t<int, int >
{
    int* svc(int* task)
    {
        // cout << "Worker receives range from " << task[0] << " to " << task[1] << endl;
        for(ull i = task[0]; i <= task[1]; i++)
        {
            if (is_prime(i))
            {
                // cout << i << " is prime" << endl;
                ff_send_out (new int(i));
            }
        }
        ff_send_out(this->EOS);
        delete [] task;
        return EOS;
    }
    void eosnotify(ssize_t id)
    {
        ff_send_out(this->EOS);
    }
    void svc_end()
    {
        // cout << "Worker end" << endl;
    }

};


void usage()
{
    cout << "MasterWorker begin end nw toprint";
}
int main(int argc, char** argv)
{
    if (argc < 5)
    {
        usage();
        return -1;
    }
    ull begin = atoi(argv[1]);
    ull end = atoi(argv[2]);
    nw = atoi(argv[3]);
    toprint = atoi(argv[4]);

    master m(begin, end);
    vector<std::unique_ptr<ff_node> > workers;
    for(int i = 0; i < nw; i++)
    {
        workers.push_back(make_unique<worker>() );
    }
    ff_Farm<pair<int, int> > farm(std::move(workers), m);
    farm.remove_collector();
    farm.wrap_around();
    ffTime(START_TIME);
    if(farm.run_and_wait_end()<0)
    {
        error("Running farm");
        return -1;
    }
    ffTime(STOP_TIME); 
    std::cout << "Time: " << ffTime(GET_TIME) << "\n";
    return 0;
}