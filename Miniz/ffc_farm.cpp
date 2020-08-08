#include <miniz.h>

#include <string>
#include <iostream>

#include <ff/ff.hpp>
#include <ff/pipeline.hpp>
#include <utility.hpp>
using namespace ff;
using namespace std;

struct Task {
    Task(size_t size, const std::string &name):
       size(size),filename(name) {}

    size_t             size;
    const std::string  filename;
};

struct Emitter:ff_node_t<Task>
{
    Emitter(const char **argv, int argc): argv(argv), argc(argc) {}

    bool doWork(const std::string& fname, size_t size) 
	{
        Task* tosend = new Task(size, fname);
		ff_send_out(tosend); // sending to the next stage
		return true;
    }

    bool walkDir(const std::string &dname, size_t size) 
	{
		DIR *dir;	
		if ((dir=opendir(dname.c_str())) == NULL) 
        {
			perror("opendir");
			fprintf(stderr, "Error: opendir %s\n", dname.c_str());
			return false;
		}
		struct dirent *file;
		bool error=false;
		while((errno=0, file =readdir(dir)) != NULL) 
        {
			struct stat statbuf;
			std::string filename = dname + "/" + file->d_name;
			if (stat(filename.c_str(), &statbuf)==-1) 
            {
                perror("stat");
                fprintf(stderr, "Error: stat %s\n", filename.c_str());
                return false;
			}
			if(S_ISDIR(statbuf.st_mode)) 
            {
                if ( !isdot(filename.c_str()) ) 
                {
                    if (!walkDir(filename,statbuf.st_size)) error = true;
                }
			} else 
            {
			    if (!doWork(filename, statbuf.st_size)) error = true;
			}
		}
		if (errno != 0) { perror("readdir"); error=true; }
		closedir(dir);
		return !error;
    }    
    Task* svc(Task*)
    {
        for(long i=0; i<argc; ++i) 
		{
			struct stat statbuf;
			if (stat(argv[i], &statbuf)==-1) {
			perror("stat");
			fprintf(stderr, "Error: stat %s\n", argv[i]);
			continue;
			}
			bool dir=false;
			if (S_ISDIR(statbuf.st_mode)) {
			success &= walkDir(argv[i], statbuf.st_size);
			} else {
			success &= doWork(argv[i], statbuf.st_size);
			}
		}
		return EOS;
    };
    const char **argv;
    const int    argc;
    bool success = true;
};


struct Worker:ff_node_t<Task>
{
    Task* svc(Task* fname)
    { 
        //compress file
        Task &f = *fname;
        cout << "Receive file named: " << f.filename << endl;
        unsigned char *inPtr = nullptr;
        size_t inSize = f.size;

        if (!mapFile(f.filename.c_str(), inSize, inPtr)) 
        {
            cout << "Cannot map file: " << f.filename << endl;
            return GO_ON;
        }
        unsigned long cmp_len = compressBound(inSize);
        // allocate memory to store compressed data in memory
        unsigned char *ptrOut = new unsigned char[cmp_len];
        if (compress(ptrOut, &cmp_len, (const unsigned char *)inPtr, inSize) != Z_OK) 
        {
            printf("Failed to compress file in memory\n");	   
            success=false;
            delete [] ptrOut;
            return GO_ON;
        }

        unmapFile(inPtr, inSize);

        //write compressed file
        string outfile = f.filename + ".zip";
        success &= writeFile(outfile, ptrOut, cmp_len);
        if (success && REMOVE_ORIGIN)
        {
            unlink(f.filename.c_str());
        }

        delete[] ptrOut;
        delete fname;
        return GO_ON;
    }
    void svc_end()
    {
        if (!success)
        {
            cout << "Compressor stage: Exiting with (some) Error(s)" << endl;
        }
    }
    bool success = true;
};

void usage()
{
    cout << "./ffc_farm directory" << endl;
}


int main(int argc, char** argv)
{
    // cout << argv[1] << endl;

    Emitter em(const_cast<const char**>(&argv[1]), argc);

    std::vector<std::unique_ptr<ff_node> > workers;
    for(size_t i=0;i<4;++i) workers.push_back(make_unique<Worker>());
    ff_Farm<float> farm(std::move(workers), em);
    farm.remove_collector(); 

    if (farm.run_and_wait_end()<0) {
        error("running farm");
        return -1;
    }

}