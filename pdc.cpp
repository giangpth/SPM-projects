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


using namespace std;
using namespace std::chrono;

#define START(timename) auto timename = std::chrono::system_clock::now();
#define STOP(timename,elapsed)  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - timename).count();


class utimer {
  std::chrono::system_clock::time_point start;
  std::chrono::system_clock::time_point stop;
  std::string message; 
  using usecs = std::chrono::microseconds;
  using msecs = std::chrono::milliseconds;

private:
  long * us_elapsed;
  
public:

  utimer(const std::string m) : message(m),us_elapsed((long *)NULL) {
    start = std::chrono::system_clock::now();
  }
    
  utimer(const std::string m, long * us) : message(m),us_elapsed(us) {
    start = std::chrono::system_clock::now();
  }

  ~utimer() {
    stop =
      std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed =
      stop - start;
    auto musec =
      std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    
    std::cout << message << " computed in \t \t" << musec << " usec " 
	      << std::endl;
    if(us_elapsed != NULL)
      (*us_elapsed) = musec;
  }
};

atomic<int> nw;
int maxnw = 8;

bool basecase(vector<int> input)
{
    if (input.size() <= 2)
    {
        return true;
    }
    return false;
}

int solve(vector<int> input)
{
    if (input.size() == 1)
    {
        return input[0];
    }
    if (input.size() == 2)
    {
        return input[0] + input[1];
    }
    return 0;
}

vector<vector<int> > divide(vector<int> input)
{
    int mid = (int)input.size();
    mid = mid/2;

    vector<int> part1(mid);
    vector<int> part2(input.size() - mid);
    copy(input.begin(), input.begin() + mid, part1.begin());
    copy(input.begin() + mid, input.end(), part2.begin());

    // for (int i = 0; i < part1.size(); i++)
    // {
    //     cout << part1[i] << " ";
    // }
    // cout << endl;

    // for (int i = 0; i < part2.size(); i++)
    // {
    //     cout << part2[i] << " ";
    // }
    // cout << endl;

    vector<vector<int> > out;
    out.push_back(part1);
    out.push_back(part2);
    return out;
}

int conquer (vector<int> res)
{
    // cout << "Conquer " << endl;
    int result = 0;
    for (int i = 0; i < res.size(); i++)
    {
        // cout << res[i] << " ";
        result += res[i];
    }
    // cout << endl;
    // cout << "Result of conquer = " << result << endl;
    return result;
}

template<typename Tin, typename Tout>
Tout dc(Tin input,
        bool (*basecase)(Tin),
        Tout (*solve)(Tin),
        std::vector<Tin> (*divide)(Tin),
        Tout (*conquer)(std::vector<Tout>)) 
{
    if(basecase(input)) 
    {
        int res = solve(input);
        return res;
    } 
    else 
    {
        vector<Tin> subproblems = divide(input);
        cout << "Subproblem size = " << subproblems.size();
        vector<Tout> subres(subproblems.size());

        std::transform(subproblems.begin(),
                       subproblems.end(),
                       subres.begin(),
                       [&](Tin x) 
                       {
                            auto res = dc(x, basecase, solve, divide, conquer);
                            // cout << "In transform function" << endl;
                            // cout << res << endl;
                            return(res);
                       });
        auto result = conquer(subres);
        return(result);
    }
};

template<typename Tin, typename Tout>
Tout pdc(Tin input,
        bool (*basecase)(Tin),
        Tout (*solve)(Tin),
        std::vector<Tin> (*divide)(Tin),
        Tout (*conquer)(std::vector<Tout>)) 
{
    
    if(basecase(input)) 
    {
        int res = solve(input);
        // cout << "Base case return " << res << endl;
        return res;
    } 
    else 
    {
        vector<Tin> subproblems = divide(input);
        vector<Tout> subres(subproblems.size());

        if (nw + subproblems.size() <= maxnw)
        {
            vector<thread> threads;
            for (int i = 0; i < subproblems.size(); i++)
            {
                nw += 1;
                auto func = [&](int i)
                {
                    Tout res = pdc(subproblems[i], basecase, solve, divide, conquer);
                    subres[i] = res;
                };
                threads.push_back(thread(func, i));
            }
            for(thread& t:threads)
            {
                t.join();
            }
            Tout result = conquer(subres);
            return result;
        }
        else
        {
            std::transform(subproblems.begin(),
                       subproblems.end(),
                       subres.begin(),
                       [&](Tin x) 
                       {
                            Tout res = pdc(x, basecase, solve, divide, conquer);
                            return(res);
                       });
            Tout result = conquer(subres);
            return result;
        }
    }
};

void usage()
{
    cout << "You can maximum number of worker" << endl;
}

int main (int argc, char** argv)
{
    if (argc == 2)
    {
        maxnw = atoi(argv[1]);
    }
    nw = 0;
    cout << "Maximum number of worker is " << maxnw << endl;
    vector<int> test;
    for (int i = 0; i < 10000; i++)
    {
        test.push_back(i);
    }
    cout << "Test begin with vector of " << test.size() << " with maximun number of nw = " << maxnw << endl;
    {
        utimer t("Runing time");
        int res = pdc<vector<int>, int>(test, basecase, solve, divide, conquer);
        cout << "Final result = " << res << endl;
    }
    return 0;
}
