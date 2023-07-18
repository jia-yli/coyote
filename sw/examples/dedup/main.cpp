#include <iostream>
#include <string>
#include <malloc.h>
#include <time.h> 
#include <sys/time.h>  
#include <chrono>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>

#ifdef EN_AVX
#include <x86intrin.h>
#endif
#include <boost/program_options.hpp>
#include <numeric>
#include <stdlib.h>
#include <cassert>

#include "cBench.hpp"
#include "cProcess.hpp"

using namespace std;
using namespace fpga;

/* Def params */
constexpr auto const targetRegion = 0;
constexpr auto const nBenchRuns = 1;  

/**
 * @brief Benchmark API
 * 
 */
enum class CTLR : uint32_t {
    INITEN = 0,
    INITDONE = 1,
    START = 2,
    RDHOSTADDR = 3,
    WRHOSTADDR = 4,
    LEN = 5,
    CNT = 6,
    PID = 7,
    RDDONE = 8,
    WRDONE = 9,
    FACTORTHROU = 10
};

/**
 * @brief Average it out
 * 
 */
double vctr_avg(std::vector<double> const& v) {
    return 1.0 * std::accumulate(v.begin(), v.end(), 0LL) / v.size();
}

void * set_write_instr(void * startPtr, int startLBA, int LBALen, bool printEn);
void * set_nop(void * startPtr);

/**
 * @brief Throughput and latency tests, read and write
 * 
 */
int main(int argc, char *argv[])
{   
    time_t now = time(0);
    tm *ltm = localtime(&now);
    stringstream timeStamp;
    timeStamp << std::setfill ('0') << std::setw(4) << 1900 + ltm->tm_year << '_' ;
    timeStamp << std::setfill ('0') << std::setw(2) << 1 + ltm->tm_mon << std::setfill ('0') << std::setw(2) << ltm->tm_mday << '_' ;
    timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_hour;
    timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_min;
    timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_sec;

    // ---------------------------------------------------------------
    // Args 
    // ---------------------------------------------------------------
    boost::program_options::options_description programDescription("Options:");
    programDescription.add_options()
    ("nPage,n", boost::program_options::value<uint32_t>()->default_value(32), "Number of Pages")
    ("dupRatio,d", boost::program_options::value<double>()->default_value(0.5), "Duplication ratio amont all inputs")
    ("throuFactor,t", boost::program_options::value<uint32_t>()->default_value(8), "Store throughput factor")
    ("writeOpNum,w", boost::program_options::value<uint32_t>()->default_value(8), "Write op Num")
    ("clearMem,c", boost::program_options::value<bool>()->default_value(false), "soft reset to clear memory")
    ("hashTableIni,h", boost::program_options::value<bool>()->default_value(false), "initialize hash table before start")
    ("debug,g", boost::program_options::value<bool>()->default_value(true), "debug mode, exec once and dump results to file")
    ("nBenchRun,r", boost::program_options::value<uint32_t>()->default_value(1), "Number of bench run");
    boost::program_options::variables_map commandLineArgs;
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
    boost::program_options::notify(commandLineArgs);
    
    uint32_t n_page = commandLineArgs["nPage"].as<uint32_t>();
    double dupRatio = commandLineArgs["dupRatio"].as<double>();
    uint32_t throu_factor = commandLineArgs["throuFactor"].as<uint32_t>();
    uint32_t n_bench_run = commandLineArgs["nBenchRun"].as<uint32_t>();
    bool clearMem = commandLineArgs["clearMem"].as<bool>();
    bool hashTableIni = commandLineArgs["hashTableIni"].as<bool>();
    bool debugMode = commandLineArgs["debug"].as<bool>();
    uint32_t uniquePageNum = (uint32_t) (((double) n_page) * dupRatio);
    // uint32_t dup_factor = n_page/uniquePageNum
    uint32_t writeOpNum = commandLineArgs["writeOpNum"].as<uint32_t>();
    uint32_t pagePerOp = n_page/writeOpNum;

    assert(n_page%16 == 0 && "n_page should be an multiple of 16.");
    assert(uniquePageNum%16 == 0 && "n_page should be an multiple of 16.");
    assert(n_page%writeOpNum==0 && "n_page should be an multiple of writeOpNum.");
    // assert(n_page%dup_factor==0 && "n_page should be an multiple of dup_factor.");

    uint32_t pg_size = 4096; // change me for 16K
    uint32_t instr_size = 64; // 64B = 512b per instr
    uint32_t total_instr_pg_num = (writeOpNum * instr_size + pg_size - 1) / pg_size; // data transfer is done in pages
    uint32_t huge_pg_size = 2 * 1024 * 1024;
    uint32_t pg_size_ratio = huge_pg_size / pg_size;
    uint32_t n_hugepage_ini = ((uniquePageNum + 1) * pg_size + huge_pg_size -1) / huge_pg_size;// random initializer
    uint32_t n_hugepage_req = ((n_page + total_instr_pg_num) * pg_size + huge_pg_size -1) / huge_pg_size; // roundup, number of hugepage for n page
    uint32_t n_hugepage_rsp = (n_page * 64 + huge_pg_size-1) / huge_pg_size; // roundup, number of huge page for 64B response from each page
    

    std::cout << "Page allocation: n_hugepage_req=" << n_hugepage_req << "\tn_hugepage_rsp:" << n_hugepage_rsp << std::endl;
    cout << endl;
    cout << "Configuration:" << endl;
    cout << "page size: " << pg_size << "Byte/page" << endl;
    cout << "Total data page count: " << n_page << " , with unique page count: " << uniquePageNum << endl;
    cout << "Total instr page count: " << total_instr_pg_num << endl;

    // Handles and alloc
    cProcess cproc(targetRegion, getpid());
    void* iniMem = NULL;
    if (hashTableIni) {
      iniMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_ini});
    }
    void* reqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_req});
    void* rspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_rsp});

    // random initialize pages with dup_factor
    char* uniquePageBuffer = (char*) malloc(uniquePageNum*pg_size); // only unique pages
    assert(uniquePageBuffer != NULL);
    char* inputPageBuffer = (char*) malloc(n_page*pg_size); // with dup, all input pages
    assert(inputPageBuffer != NULL);
    cout << "Preparing unique page data" << endl;
    int urand=open("/dev/urandom", O_RDONLY);
    // int res = read(urand, reqMem, pg_size*uniquePageNum);
    // get unique page data
    int res = read(urand, uniquePageBuffer, pg_size*uniquePageNum);
    // for(int i = 0; i < uniquePageNum * pg_size; i++){
    //   uniquePageBuffer[i] = (char) (i/pg_size);
    // }
    // construct all input pages with duplication
    for (int j = 0; j < n_page; j++) {
        // policy: 1,...,N,1,...,N
        int copy_page_pointer = j % uniquePageNum;
        memcpy(inputPageBuffer + j * pg_size, uniquePageBuffer + copy_page_pointer * pg_size, pg_size);
    }
    close(urand);

    // golden res
    int* goldenPgIsExec = (int*) malloc(n_page * sizeof(int));
    int* goldenPgRefCount = (int*) malloc(n_page * sizeof(int));
    int* goldenPgIdx = (int*) malloc(n_page * sizeof(int));

    for (int pgIdx = 0; pgIdx < n_page; pgIdx ++){
      goldenPgIsExec[pgIdx] = (pgIdx < uniquePageNum) ? 1 : 0;
      goldenPgRefCount[pgIdx] = pgIdx / uniquePageNum + 1;
      // goldenPgIdx[pgIdx] = pgIdx;
      goldenPgIdx[pgIdx] = (pgIdx / pagePerOp) * pagePerOp + pgIdx;
    }

    // hash table init
    cout << "Preparing hashtable content initializer" << endl;
    void* initPtr = iniMem;
    if (hashTableIni) {
      for (int instrIdx = 0; instrIdx < pg_size; instrIdx ++){
        if (instrIdx < 1){
          // set instr: write instr
          // initPtr = set_write_instr(initPtr, instrIdx*pagePerOp, pagePerOp, false);
          initPtr = set_write_instr(initPtr, 0, uniquePageNum, false);
          
          // set pages
          char* initPtrChar = (char*) initPtr;
          for (int pgIdx = 0; pgIdx < uniquePageNum; pgIdx ++){
            int randPgIdx = (rand() % uniquePageNum);
            memcpy(initPtrChar + pgIdx * pg_size, uniquePageBuffer + randPgIdx * pg_size, pg_size);
            // memcpy(initPtrChar + pgIdx * pg_size, uniquePageBuffer + pgIdx * pg_size, pg_size);
          }
          initPtrChar = initPtrChar + uniquePageNum * pg_size;
          initPtr = (void*) initPtrChar;
        } else {
          // set Insrt: nop
          initPtr = set_nop(initPtr);
        }
      }
    }

    // request for throughput
    initPtr = reqMem;
    cout << "Preparing request for throughput" << endl;
    for (int instrIdx = 0; instrIdx < total_instr_pg_num * (pg_size / instr_size); instrIdx ++){
      if (instrIdx < writeOpNum){
        // cout << instrIdx << " at " << (instrIdx - total_instr_pg_num * pg_size + writeOpNum);
        // set instr: write instr
        // initPtr = set_write_instr(initPtr, instrIdx*pagePerOp, pagePerOp, false);
        initPtr = set_write_instr(initPtr, 2*instrIdx*pagePerOp, pagePerOp, false);
        
        // set pages
        char* initPtrChar = (char*) initPtr;
        // memcpy(initPtrChar, inputPageBuffer + instrIdx * pagePerOp * pg_size, pagePerOp * pg_size);
        memcpy(initPtrChar, inputPageBuffer + (instrIdx) * pagePerOp * pg_size, pagePerOp * pg_size);
        // for(int i = 0; i < pagePerOp * pg_size; i++){
        //   // assert ((int)*initPtrChar == 1);
        //   if((int)*initPtrChar != '\1'){
        //     // std::cout << (int) *initPtrChar << std::endl;
        //     std::cout << instrIdx << std::endl;
        //     break;
        //   }
        // }
        initPtrChar = initPtrChar + pagePerOp * pg_size;
        initPtr = (void*) initPtrChar;
      } else {
        // set Insrt: nop
        initPtr = set_nop(initPtr);
      }
    }

    // request for latency
    // cout << "Preparing request for latency" << endl;
    // initPtr = reqMem;
    // pagePerOp = 16;
    // n_page = 16;
    // total_instr_pg_num = 1;
    // for (int instrIdx = 0; instrIdx < total_instr_pg_num * (pg_size / instr_size); instrIdx ++){
    //   if (instrIdx < 1){
    //     // set instr: write instr
    //     // initPtr = set_write_instr(initPtr, instrIdx*pagePerOp, pagePerOp, false);
    //     initPtr = set_write_instr(initPtr, 0, pagePerOp, false);
        
    //     // set pages
    //     char* initPtrChar = (char*) initPtr;
    //     int sampleLen = 4; // place at hash table to sample
    //     memcpy(initPtrChar, uniquePageBuffer + (sampleLen-1) * 1024 * pg_size, pagePerOp * pg_size);

    //     initPtrChar = initPtrChar + pagePerOp * pg_size;
    //     initPtr = (void*) initPtrChar;
    //   } else {
    //     // set Insrt: nop
    //     initPtr = set_nop(initPtr);
    //   }
    // }

    // // double check
    // for (int j = 1; j < dup_factor; j++) {
    //     assert(memcmp(reqMem, reqMem+j*pg_size*uniquePageNum, pg_size*uniquePageNum)==0);
    // }
    
    // init dedup module: if not init done, init
    cout << endl;
    if (clearMem){
      // Soft reset is not working......
      std::cout << "Soft Reset: Force clean all internal states" << std::endl;
      cproc.setCSR(3, static_cast<uint32_t>(CTLR::INITEN));
    } else if (!cproc.getCSR(static_cast<uint32_t>(CTLR::INITDONE))){
      std::cout << "System was not initialized, initializing" << std::endl;
      cproc.setCSR(1, static_cast<uint32_t>(CTLR::INITEN));
    } else {
      std::cout << "System was initialized, no op" << std::endl;
    }
    // cproc.setCSR(1, static_cast<uint32_t>(CTLR::INITEN));
    cproc.setCSR(pg_size, static_cast<uint32_t>(CTLR::LEN));
    cproc.setCSR(cproc.getCpid(), static_cast<uint32_t>(CTLR::PID));
    cproc.setCSR(throu_factor, static_cast<uint32_t>(CTLR::FACTORTHROU));

    // confirm the init is done
    while(!cproc.getCSR(static_cast<uint32_t>(CTLR::INITDONE))) {
        // std::cout << "Waiting for initialization of BloomFilter and HashTable for SHA3 values" << std::endl;
        std::cout << "Waiting for HashTable header initialization" << std::endl;
        usleep(1);
    }
    
    if (hashTableIni){
      std::cout << std::endl << "Hash Table content initialization." << std::endl;
      assert(iniMem != NULL);
      cproc.setCSR(reinterpret_cast<uint64_t>(iniMem), static_cast<uint32_t>(CTLR::RDHOSTADDR));
      cproc.setCSR(reinterpret_cast<uint64_t>(rspMem), static_cast<uint32_t>(CTLR::WRHOSTADDR));
      cproc.setCSR(uniquePageNum + 1, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch

      cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
      sleep(1);
      std::cout << "RDDONE:" << cproc.getCSR(static_cast<uint32_t>(CTLR::RDDONE)) << std::endl;
      std::cout << "WRDONE:" << cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) << std::endl; 
    }

    std::cout << std::endl << "Initialization done. Workload started." << std::endl;

    cproc.setCSR(reinterpret_cast<uint64_t>(reqMem), static_cast<uint32_t>(CTLR::RDHOSTADDR));
    cproc.setCSR(reinterpret_cast<uint64_t>(rspMem), static_cast<uint32_t>(CTLR::WRHOSTADDR));
    cproc.setCSR(n_page + total_instr_pg_num, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch

    if (!debugMode){
      cout << endl << "In Benchmarking mode, start benchmarking ......" << endl;
      cBench bench(n_bench_run, true, true);
      auto benchmark_lat = [&]() {    
          cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
          while(cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) != n_page/16);
      };

      bench.runtime(benchmark_lat);
      bench.printOut();
    } else {
      cout << endl << "In Debuging mode, start execution ......" << endl;
      cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
      // while(cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) != n_page/16);
      sleep(1);
    }

    std::cout << "RDDONE:" << cproc.getCSR(static_cast<uint32_t>(CTLR::RDDONE)) << std::endl;
    std::cout << "WRDONE:" << cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) << std::endl; 

    ofstream outfile;
    outfile.open("./page_resp/resp_" + timeStamp.str() + ".txt", ios::out);

    bool allPassed = true;

    /** parse and print the page response */
    uint32_t* rspMemUInt32 = (uint32_t*) rspMem;
    for (int i=0; i < n_page; i++) {
        uint32_t sha3Hash_0   = rspMemUInt32[i*16 + 0];
        uint32_t sha3Hash_1   = rspMemUInt32[i*16 + 1];
        uint32_t sha3Hash_2   = rspMemUInt32[i*16 + 2];
        uint32_t sha3Hash_3   = rspMemUInt32[i*16 + 3];
        uint32_t sha3Hash_4   = rspMemUInt32[i*16 + 4];
        uint32_t sha3Hash_5   = rspMemUInt32[i*16 + 5];
        uint32_t sha3Hash_6   = rspMemUInt32[i*16 + 6];
        uint32_t sha3Hash_7   = rspMemUInt32[i*16 + 7];
        uint32_t refCount     = rspMemUInt32[i*16 + 8];
        uint32_t SSDLBA       = rspMemUInt32[i*16 + 9];
        uint32_t hostLBAStart = rspMemUInt32[i*16 + 10];
        uint32_t hostLBALen   = rspMemUInt32[i*16 + 11];
        uint32_t pad_0        = rspMemUInt32[i*16 + 12];
        uint32_t pad_1        = rspMemUInt32[i*16 + 13];
        uint32_t pad_2        = rspMemUInt32[i*16 + 14];
        uint32_t execStatus   = rspMemUInt32[i*16 + 15];
        bool isExec = (execStatus & (1 << 29)) ? true : false; // 1 -> op exec -> new page -> not exist
        uint32_t opCode = (execStatus>>30);

        bool pagePassed = true;
        pagePassed = pagePassed && (refCount == goldenPgRefCount[i]);
        pagePassed = pagePassed && (hostLBAStart == goldenPgIdx[i]);
        pagePassed = pagePassed && (hostLBALen == 1);
        pagePassed = pagePassed && (isExec == goldenPgIsExec[i]);
        pagePassed = pagePassed && (opCode == 1);

        allPassed = allPassed && pagePassed;
        // std::cout << opCode << std::endl;
        // std::cout << pad_0 << " " << pad_1 << " " << pad_2 << std::endl;
        // std::cout << "hostLBAStart:" << hostLBAStart << "\thostLBALen:" << hostLBALen << "\tisExec:" << isExec << std::endl;
        // std::cout << "refCount:" << refCount << "\tSSDLBA:" << SSDLBA << std::endl;

        std::stringstream SHA3sstream;
        for (int sha3PieceIdx = 7; sha3PieceIdx >= 0; sha3PieceIdx --){
          SHA3sstream << std::setfill ('0') << std::setw(sizeof(uint32_t)*2) << std::hex << rspMemUInt32[i*16 + sha3PieceIdx];
        }
        // std::cout << "SHA3:" << SHA3sstream.str() << std::endl;
        
        // assert(refCount == goldenPgRefCount[i]);
        // assert(hostLBAStart == goldenPgIdx[i]);
        // assert(hostLBALen == 1);
        // assert(isExec == goldenPgIsExec[i]);
        // assert(opCode == 1);

        // std::cout << refCount     << ' ' << goldenPgRefCount[i] << std::endl;
        // std::cout << hostLBAStart << ' ' << goldenPgIdx[i]      << std::endl;
        // std::cout << hostLBALen   << ' ' << 1                   << std::endl;
        // std::cout << isExec       << ' ' << goldenPgIsExec[i]   << std::endl;
        // std::cout << opCode       << ' ' << 1                   << std::endl;

        // write to file
        outfile << "page: " << i << ", at SSD LBA: " << SSDLBA << endl;
        outfile << "overall checking:" << pagePassed << endl;
        outfile << "refCount     " << refCount     << "\texpected " << goldenPgRefCount[i] << std::endl;
        outfile << "hostLBAStart " << hostLBAStart << "\texpected " << goldenPgIdx[i]      << std::endl;
        outfile << "hostLBALen   " << hostLBALen   << "\texpected " << 1                   << std::endl;
        outfile << "isExec       " << isExec       << "\texpected " << goldenPgIsExec[i]   << std::endl;
        outfile << "opCode       " << opCode       << "\texpected " << 1                   << std::endl;
        outfile << "SHA3         " << SHA3sstream.str() << endl;
        outfile << endl;
    }
    cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
    

    cproc.clearCompleted();
    free(uniquePageBuffer);
    free(inputPageBuffer);
    free(goldenPgIsExec);
    free(goldenPgRefCount);
    free(goldenPgIdx);

    return EXIT_SUCCESS;
}

// instr = 512 bit = 32bit x 16
void * set_write_instr(void * startPtr, int startLBA, int LBALen, bool printEn = false){
    uint32_t * startPtrUInt32 = (uint32_t *) startPtr;
    startPtrUInt32[0]  = LBALen;
    startPtrUInt32[1]  = startLBA;
    startPtrUInt32[2]  = 0;
    startPtrUInt32[3]  = 0;
    startPtrUInt32[4]  = 0;
    startPtrUInt32[5]  = 0;
    startPtrUInt32[6]  = 0;
    startPtrUInt32[7]  = 0;
    startPtrUInt32[8]  = 0;
    startPtrUInt32[9]  = 0;
    startPtrUInt32[10] = 0;
    startPtrUInt32[11] = 0;
    startPtrUInt32[12] = 0;
    startPtrUInt32[13] = 0;
    startPtrUInt32[14] = 0;
    startPtrUInt32[15] = 1 << 30;
    if(printEn){
      std::cout << "Instr: write, startLBA:" << startLBA << "\tLBALen:" << LBALen << std::endl;
    }
    return (void *) (startPtrUInt32 + 16);
}

void * set_nop(void * startPtr){
    uint32_t * startPtrUInt32 = (uint32_t *) startPtr;
    startPtrUInt32[0]  = 0;
    startPtrUInt32[1]  = 0;
    startPtrUInt32[2]  = 0;
    startPtrUInt32[3]  = 0;
    startPtrUInt32[4]  = 0;
    startPtrUInt32[5]  = 0;
    startPtrUInt32[6]  = 0;
    startPtrUInt32[7]  = 0;
    startPtrUInt32[8]  = 0;
    startPtrUInt32[9]  = 0;
    startPtrUInt32[10] = 0;
    startPtrUInt32[11] = 0;
    startPtrUInt32[12] = 0;
    startPtrUInt32[13] = 0;
    startPtrUInt32[14] = 0;
    startPtrUInt32[15] = 0;
    return (void *) (startPtrUInt32 + 16);
}
