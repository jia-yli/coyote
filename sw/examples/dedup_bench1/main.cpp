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
void * set_erase_instr(void * startPtr, uint32_t * sha3Val, bool printEn);
void * set_nop(void * startPtr);
bool parse_response(uint32_t pageCount, void* rspMem, int* goldenPgIsExec, int* goldenPgRefCount, int* goldenPgIdx, int goldenOpCode, ofstream& outfile);


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
  ("nPage,n", boost::program_options::value<uint32_t>()->default_value(1024), "Number of Pages in 1 Batch")
  ("dupRatio,d", boost::program_options::value<double>()->default_value(0.5), "Overlap(batch, existing page)/nPage")
  ("fullness,f", boost::program_options::value<double>()->default_value(0.5), "fullness of hash table")
  ("throuFactor,t", boost::program_options::value<uint32_t>()->default_value(8), "Store throughput factor")
  ("writeOpNum,w", boost::program_options::value<uint32_t>()->default_value(8), "Write op Num")
  ("nBenchRun,r", boost::program_options::value<uint32_t>()->default_value(2), "Number of bench run");
  boost::program_options::variables_map commandLineArgs;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
  boost::program_options::notify(commandLineArgs);
  
  uint32_t n_page = commandLineArgs["nPage"].as<uint32_t>();
  double dup_ratio = commandLineArgs["dupRatio"].as<double>();
  double hash_table_fullness = commandLineArgs["fullness"].as<double>();
  uint32_t throu_factor = commandLineArgs["throuFactor"].as<uint32_t>();
  uint32_t n_bench_run = commandLineArgs["nBenchRun"].as<uint32_t>();
  uint32_t write_op_num = commandLineArgs["writeOpNum"].as<uint32_t>();
  string output_dir = "/home/jiayli/projects/coyote/sw/examples/dedup_bench1/page_resp";

  // checkings
  assert(n_page%16 == 0 && "n_page should be an multiple of 16.");
  assert(n_page%write_op_num==0 && "n_page should be an multiple of writeOpNum.");

  // hard parameters
  uint32_t pg_size = 4096; // change me for 16K
  uint32_t huge_pg_size = 2 * 1024 * 1024;
  uint32_t pg_per_huge_pg = huge_pg_size / pg_size;
  uint32_t instr_size = 64; // 64B = 512b per instr
  uint32_t instr_per_page = pg_size/instr_size;
  // uint32_t ht_nbucket = (1 << 17);
  // uint32_t ht_size = (1 << 20);
  uint32_t ht_nbucket = (1 << 13);
  uint32_t ht_size = (1 << 16);

  // drived parameters
  uint32_t total_old_page_unique_count = (((uint32_t) (hash_table_fullness * ht_size) + 15)/16) * 16;
  uint32_t batch_new_page_unique_count = (((uint32_t) (n_page * (1 - dup_ratio)) + 15)/16) * 16;
  uint32_t batch_old_page_unique_count =  n_page - batch_new_page_unique_count;
  uint32_t total_page_unique_count = total_old_page_unique_count + batch_new_page_unique_count;
  assert (total_old_page_unique_count > 0);
  assert (batch_old_page_unique_count <= total_old_page_unique_count);
  assert (total_page_unique_count <= ht_size);
  cout << "Config: "<< endl;
  cout << "1. number of initial page to fill up: " << total_old_page_unique_count << endl;
  cout << "2. number of page to run benchmark: " << n_page << endl;
  cout << "3. number of existing page in benchmark: " << batch_old_page_unique_count << endl;
  cout << "4. number of new page in benchmark: " << batch_new_page_unique_count << endl;

  // Handles and alloc
  cProcess cproc(targetRegion, getpid());

  // Step 0: 
  cout << endl << "Step0: system init" << endl;
  cproc.setCSR(pg_size, static_cast<uint32_t>(CTLR::LEN));
  cproc.setCSR(cproc.getCpid(), static_cast<uint32_t>(CTLR::PID));
  cproc.setCSR(throu_factor, static_cast<uint32_t>(CTLR::FACTORTHROU));
  if (!cproc.getCSR(static_cast<uint32_t>(CTLR::INITDONE))){
    std::cout << "System was not initialized, initializing" << std::endl;
    cproc.setCSR(1, static_cast<uint32_t>(CTLR::INITEN));
    usleep(1);
  } else {
    std::cout << "System was initialized, no op" << std::endl;
  }
  // confirm the init is done
  while(!cproc.getCSR(static_cast<uint32_t>(CTLR::INITDONE))) {
    // std::cout << "Waiting for initialization of BloomFilter and HashTable for SHA3 values" << std::endl;
    std::cout << "Waiting for HashTable header initialization" << std::endl;
    usleep(1);
  }

  // Step 1: Insert all old and new pages, get SHA3
  cout << endl << "Step1: get all page SHA3, total unique page count: "<< total_page_unique_count << endl;
  cout << "Preparing unique page data" << endl;
  char* all_unique_page_buffer = (char*) malloc(total_page_unique_count * pg_size);
  uint32_t* all_unique_page_sha3 = (uint32_t*) malloc(total_page_unique_count * 32); // SHA3 256bit = 32B
  assert(all_unique_page_buffer != NULL);
  assert(all_unique_page_sha3   != NULL);
  // get unique page data
  int urand = open("/dev/urandom", O_RDONLY);
  int res = read(urand, all_unique_page_buffer, total_page_unique_count * pg_size);
  close(urand);

  // support 64x2M page address mapping, do 32 in one insertion round
  ofstream outfile;
  std::stringstream outfile_name;
  outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step1.txt";
  outfile.open(outfile_name.str(), ios::out);
  bool allPassed = true;

  uint32_t n_insertion_round = (total_page_unique_count + 32 * pg_per_huge_pg - 1) / (32 * pg_per_huge_pg);
  cout << "insert all unique pages, in "<< n_insertion_round << " rounds"<< endl;
  for (int insertion_rount_idx; insertion_rount_idx < n_insertion_round; insertion_rount_idx++){
    uint32_t pg_idx_start = insertion_rount_idx * 32 * pg_per_huge_pg;
    uint32_t pg_idx_end = ((pg_idx_start + 32 * pg_per_huge_pg) > total_page_unique_count) ? total_page_unique_count : (pg_idx_start + 32 * pg_per_huge_pg);
    uint32_t pg_idx_count = pg_idx_end - pg_idx_start;
    uint32_t prep_instr_pg_num = (1 * instr_size + pg_size - 1) / pg_size; // 1 op
    uint32_t n_hugepage_prep_req = ((pg_idx_count + prep_instr_pg_num) * pg_size + huge_pg_size -1) / huge_pg_size; // roundup, number of hugepage for n page
    uint32_t n_hugepage_prep_rsp = (pg_idx_count * 64 + huge_pg_size-1) / huge_pg_size; // roundup, number of huge page for 64B response from each page

    void* prepReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_prep_req});
    void* prepRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_prep_rsp});

    void* initPtr = prepReqMem;
    for (int instrIdx = 0; instrIdx < instr_per_page * prep_instr_pg_num; instrIdx ++){
      if (instrIdx < 1){
        // set instr: write instr
        initPtr = set_write_instr(initPtr, 100 + pg_idx_start, pg_idx_count, false);

        // set pages
        char* initPtrChar = (char*) initPtr;
        memcpy(initPtrChar, all_unique_page_buffer + pg_idx_start * pg_size, pg_idx_count * pg_size);
        initPtrChar = initPtrChar + pg_idx_count * pg_size;
        initPtr = (void*) initPtrChar;
      } else {
        // set Insrt: nop
        initPtr = set_nop(initPtr);
      }
    }

    // golden res
    int* prepGoldenPgIsExec = (int*) malloc(pg_idx_count * sizeof(int));
    int* prepGoldenPgRefCount = (int*) malloc(pg_idx_count * sizeof(int));
    int* prepGoldenPgIdx = (int*) malloc(pg_idx_count * sizeof(int));

    for (int pgIdx = 0; pgIdx < pg_idx_count; pgIdx ++){
      prepGoldenPgIsExec[pgIdx] = 1;
      prepGoldenPgRefCount[pgIdx] = 1;
      prepGoldenPgIdx[pgIdx] = 100 + pg_idx_start + pgIdx;
    }

    cout << "round " << insertion_rount_idx << " start execution: page " << pg_idx_start << " to " << pg_idx_end << endl;
    cproc.setCSR(reinterpret_cast<uint64_t>(prepReqMem), static_cast<uint32_t>(CTLR::RDHOSTADDR));
    cproc.setCSR(reinterpret_cast<uint64_t>(prepRspMem), static_cast<uint32_t>(CTLR::WRHOSTADDR));
    cproc.setCSR(pg_idx_count + prep_instr_pg_num, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch
    cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
    sleep(1);

    std::cout << "read page count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::RDDONE)) << "/" << pg_idx_count + prep_instr_pg_num << std::endl;
    std::cout << "write resp count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) * 16 << "/" << pg_idx_count << std::endl; 

    /** parse and print the page response */
    cout << "parsing the results" << endl;
    bool check_res = parse_response(pg_idx_count, prepRspMem, prepGoldenPgIsExec, prepGoldenPgRefCount, prepGoldenPgIdx, 1, outfile);
    allPassed = allPassed && check_res;
    uint32_t* rspMemUInt32 = (uint32_t*) prepRspMem;
    cout << "get all SHA3" << endl;
    for (int i=0; i < pg_idx_count; i++) {
      memcpy(all_unique_page_sha3 + (pg_idx_start + i) * 8, (void*) (rspMemUInt32 + i*16), 32);
    }
    free(prepGoldenPgIsExec);
    free(prepGoldenPgRefCount);
    free(prepGoldenPgIdx);
    cproc.freeMem(prepReqMem);
    cproc.freeMem(prepRspMem);
  }
  cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
  outfile.close();

  cout << endl << "Step2: clean new pages" << endl;
  if (batch_new_page_unique_count > 0){
    uint32_t num_page_to_clean = batch_new_page_unique_count;
    uint32_t clean_instr_pg_num = (num_page_to_clean * instr_size + pg_size - 1) / pg_size; // data transfer is done in pages
    uint32_t n_hugepage_clean_req = (clean_instr_pg_num * pg_size + huge_pg_size -1) / huge_pg_size;
    uint32_t n_hugepage_clean_rsp = (num_page_to_clean * 64 + huge_pg_size-1) / huge_pg_size;
    void* cleanReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_clean_req});
    void* cleanRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_clean_rsp});

    void* initPtr = cleanReqMem;
    for (int instrIdx = 0; instrIdx < clean_instr_pg_num * instr_per_page; instrIdx ++){
      if (instrIdx < num_page_to_clean){
        // set instr: erase instr
        int pgIdx = total_old_page_unique_count + instrIdx;
        initPtr = set_erase_instr(initPtr, all_unique_page_sha3 + pgIdx * 8, false);
      } else {
        // set Insrt: nop
        initPtr = set_nop(initPtr);
      }
    }

    // golden res
    int* cleanGoldenPgIsExec = (int*) malloc(num_page_to_clean * sizeof(int));
    int* cleanGoldenPgRefCount = (int*) malloc(num_page_to_clean * sizeof(int));
    int* cleanGoldenPgIdx = (int*) malloc(num_page_to_clean * sizeof(int));

    for (int pgIdx = 0; pgIdx < num_page_to_clean; pgIdx ++){
      cleanGoldenPgIsExec[pgIdx] = 1;
      cleanGoldenPgRefCount[pgIdx] = 0;
      cleanGoldenPgIdx[pgIdx] = 0;
    }

    cproc.setCSR(reinterpret_cast<uint64_t>(cleanReqMem), static_cast<uint32_t>(CTLR::RDHOSTADDR));
    cproc.setCSR(reinterpret_cast<uint64_t>(cleanRspMem), static_cast<uint32_t>(CTLR::WRHOSTADDR));
    cproc.setCSR(clean_instr_pg_num, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch
    cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
    sleep(1);

    std::cout << "read page count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::RDDONE)) << "/" << clean_instr_pg_num << std::endl;
    std::cout << "write resp count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) * 16 << "/" << num_page_to_clean << std::endl; 

    ofstream outfile2;
    std::stringstream outfile_name2;
    outfile_name2 << output_dir << "/resp_"<< timeStamp.str() << "_step2.txt";
    outfile2.open(outfile_name2.str(), ios::out);

    cout << "parsing the results" << endl;
    allPassed = parse_response(num_page_to_clean, cleanRspMem, cleanGoldenPgIsExec, cleanGoldenPgRefCount, cleanGoldenPgIdx, 2, outfile2);
    cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
    outfile2.close();
    free(cleanGoldenPgIsExec);
    free(cleanGoldenPgRefCount);
    free(cleanGoldenPgIdx);
    cproc.freeMem(cleanReqMem);
    cproc.freeMem(cleanRspMem);
  }

  cout << endl << "Step3: start benchmarking, insertion only" << endl;
  std::vector<double> times_lst;
  cout << n_bench_run << " runs in total" << endl;
  for (int bench_idx = 0; bench_idx < n_bench_run; bench_idx ++){
    cout << endl << "starting run " << bench_idx + 1 << "/" << n_bench_run << endl;
    uint32_t benchmark_insert_pg_num = n_page;
    uint32_t benchmark_insert_page_per_instr = benchmark_insert_pg_num / write_op_num;
    uint32_t benchmark_insert_instr_pg_num = (write_op_num * instr_size + pg_size - 1) / pg_size; // data transfer is done in pages
    uint32_t n_hugepage_bench_insert_req = ((benchmark_insert_pg_num + benchmark_insert_instr_pg_num) * pg_size + huge_pg_size - 1) / huge_pg_size;
    uint32_t n_hugepage_bench_insert_rsp = (benchmark_insert_pg_num * 64 + huge_pg_size - 1) / huge_pg_size;
    void* benchInsertReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_insert_req});
    void* benchInsertRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_insert_rsp});

    // get page index
    vector<int> benchmark_insert_page_idx_lst;
    {
      vector<int> random_old_page_idx_lst;
      for (int i = 0; i < total_old_page_unique_count; i++){
        random_old_page_idx_lst.push_back(i);
      }
      
      random_shuffle(random_old_page_idx_lst.begin(), random_old_page_idx_lst.end());
      
      for (int i = 0; i < benchmark_insert_pg_num; i++){
        if (i < batch_new_page_unique_count){
          benchmark_insert_page_idx_lst.push_back(i + total_old_page_unique_count);
        } else {
          // random one from old
          benchmark_insert_page_idx_lst.push_back(random_old_page_idx_lst[i - batch_new_page_unique_count]);
        }
      }
    }
    random_shuffle(benchmark_insert_page_idx_lst.begin(), benchmark_insert_page_idx_lst.end());

    void* initPtr = benchInsertReqMem;
    for (int instrIdx = 0; instrIdx < instr_per_page * benchmark_insert_instr_pg_num; instrIdx ++){
      if (instrIdx < write_op_num){
        // set instr: write instr
        initPtr = set_write_instr(initPtr, 100000 + instrIdx * benchmark_insert_page_per_instr, benchmark_insert_page_per_instr, false);

        // set pages
        char* initPtrChar = (char*) initPtr;
        for (int pgIdx = 0; pgIdx < benchmark_insert_page_per_instr; pgIdx ++){
          int randPgIdx = benchmark_insert_page_idx_lst[instrIdx * benchmark_insert_page_per_instr + pgIdx];
          memcpy(initPtrChar + pgIdx * pg_size, all_unique_page_buffer + randPgIdx * pg_size, pg_size);
          // memcpy(initPtrChar + pgIdx * pg_size, uniquePageBuffer + pgIdx * pg_size, pg_size);
        }
        initPtrChar = initPtrChar + benchmark_insert_page_per_instr * pg_size;
        initPtr = (void*) initPtrChar;
      } else {
        // set Insrt: nop
        initPtr = set_nop(initPtr);
      }
    }

    // golden res
    int* benchInsertGoldenPgIsExec = (int*) malloc(benchmark_insert_pg_num * sizeof(int));
    int* benchInsertGoldenPgRefCount = (int*) malloc(benchmark_insert_pg_num * sizeof(int));
    int* benchInsertGoldenPgIdx = (int*) malloc(benchmark_insert_pg_num * sizeof(int));

    for (int pgIdx = 0; pgIdx < benchmark_insert_pg_num; pgIdx ++){
      if (benchmark_insert_page_idx_lst[pgIdx] < total_old_page_unique_count){
        // old page
        benchInsertGoldenPgIsExec[pgIdx] = 0;
        benchInsertGoldenPgRefCount[pgIdx] = 2;
        benchInsertGoldenPgIdx[pgIdx] = 100000 + pgIdx;
      } else {
        // new page
        benchInsertGoldenPgIsExec[pgIdx] = 1;
        benchInsertGoldenPgRefCount[pgIdx] = 1;
        benchInsertGoldenPgIdx[pgIdx] = 100000 + pgIdx;
      }
    }

    cproc.setCSR(reinterpret_cast<uint64_t>(benchInsertReqMem), static_cast<uint32_t>(CTLR::RDHOSTADDR));
    cproc.setCSR(reinterpret_cast<uint64_t>(benchInsertRspMem), static_cast<uint32_t>(CTLR::WRHOSTADDR));
    cproc.setCSR(benchmark_insert_pg_num + benchmark_insert_instr_pg_num, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch

    auto begin_time = std::chrono::high_resolution_clock::now();
    cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
    while(cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) != benchmark_insert_pg_num/16);
    auto end_time = std::chrono::high_resolution_clock::now();

    double time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();
    times_lst.push_back(time);

    std::cout << "read page count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::RDDONE)) << "/" << benchmark_insert_pg_num + benchmark_insert_instr_pg_num << std::endl;
    std::cout << "write resp count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) * 16 << "/" << benchmark_insert_pg_num << std::endl; 
    std::cout << "time used: " << time << "ns" << std::endl;

    ofstream outfile3_1;
    std::stringstream outfile_name3_1;
    outfile_name3_1 << output_dir << "/resp_" << timeStamp.str() << "_step3_1.txt";
    outfile3_1.open(outfile_name3_1.str(), ios::out);

    cout << "parsing the results" << endl;
    allPassed = parse_response(benchmark_insert_pg_num, benchInsertRspMem, benchInsertGoldenPgIsExec, benchInsertGoldenPgRefCount, benchInsertGoldenPgIdx, 1, outfile3_1);
    cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
    outfile3_1.close();

    free(benchInsertGoldenPgIsExec);
    free(benchInsertGoldenPgRefCount);
    free(benchInsertGoldenPgIdx);
    cproc.freeMem(benchInsertReqMem);
    cproc.freeMem(benchInsertRspMem);

    uint32_t benchmark_num_page_to_clean = benchmark_insert_pg_num;
    uint32_t benchmark_clean_instr_pg_num = (benchmark_num_page_to_clean * instr_size + pg_size - 1) / pg_size; // data transfer is done in pages
    uint32_t n_hugepage_bench_clean_req = (benchmark_clean_instr_pg_num * pg_size + huge_pg_size -1) / huge_pg_size;
    uint32_t n_hugepage_bench_clean_rsp = (benchmark_num_page_to_clean * 64 + huge_pg_size-1) / huge_pg_size;
    void* benchCleanReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_clean_req});
    void* benchCleanRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_clean_rsp});

    initPtr = benchCleanReqMem;
    for (int instrIdx = 0; instrIdx < benchmark_clean_instr_pg_num * instr_per_page; instrIdx ++){
      if (instrIdx < benchmark_num_page_to_clean){
        // set instr: erase instr
        int pgIdx = benchmark_insert_page_idx_lst[instrIdx];
        initPtr = set_erase_instr(initPtr, all_unique_page_sha3 + pgIdx * 8, false);
      } else {
        // set Insrt: nop
        initPtr = set_nop(initPtr);
      }
    }

    // golden res
    int* benchCleanGoldenPgIsExec = (int*) malloc(benchmark_num_page_to_clean * sizeof(int));
    int* benchCleanGoldenPgRefCount = (int*) malloc(benchmark_num_page_to_clean * sizeof(int));
    int* benchCleanGoldenPgIdx = (int*) malloc(benchmark_num_page_to_clean * sizeof(int));

    for (int pgIdx = 0; pgIdx < benchmark_num_page_to_clean; pgIdx ++){
      if (benchmark_insert_page_idx_lst[pgIdx] < total_old_page_unique_count){
        // old page
        benchCleanGoldenPgIsExec[pgIdx] = 0;
        benchCleanGoldenPgRefCount[pgIdx] = 1;
        benchCleanGoldenPgIdx[pgIdx] = 0;
      } else {
        // new page, GC
        benchCleanGoldenPgIsExec[pgIdx] = 1;
        benchCleanGoldenPgRefCount[pgIdx] = 0;
        benchCleanGoldenPgIdx[pgIdx] = 0;
      }
    }

    cproc.setCSR(reinterpret_cast<uint64_t>(benchCleanReqMem), static_cast<uint32_t>(CTLR::RDHOSTADDR));
    cproc.setCSR(reinterpret_cast<uint64_t>(benchCleanRspMem), static_cast<uint32_t>(CTLR::WRHOSTADDR));
    cproc.setCSR(benchmark_clean_instr_pg_num, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch
    cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
    sleep(1);

    std::cout << "read page count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::RDDONE)) << "/" << benchmark_clean_instr_pg_num << std::endl;
    std::cout << "write resp count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) * 16 << "/" << benchmark_num_page_to_clean << std::endl; 

    ofstream outfile3_2;
    std::stringstream outfile_name3_2;
    outfile_name3_2 << output_dir << "/resp_" << timeStamp.str() << "_step3_2.txt";
    outfile3_2.open(outfile_name3_2.str(), ios::out);

    cout << "parsing the results" << endl;
    allPassed = parse_response(benchmark_num_page_to_clean, benchCleanRspMem, benchCleanGoldenPgIsExec, benchCleanGoldenPgRefCount, benchCleanGoldenPgIdx, 2, outfile3_2);
    cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
    outfile3_2.close();

    free(benchCleanGoldenPgIsExec);
    free(benchCleanGoldenPgRefCount);
    free(benchCleanGoldenPgIdx);
    cproc.freeMem(benchCleanReqMem);
    cproc.freeMem(benchCleanRspMem);
  }

  cout << endl << "Step4: clean up all remaining pages" << endl;
  if (total_old_page_unique_count > 0){
    uint32_t final_num_page_to_clean = total_old_page_unique_count;
    uint32_t final_clean_instr_pg_num = (final_num_page_to_clean * instr_size + pg_size - 1) / pg_size; // data transfer is done in pages
    uint32_t n_hugepage_final_clean_req = (final_clean_instr_pg_num * pg_size + huge_pg_size -1) / huge_pg_size;
    uint32_t n_hugepage_final_clean_rsp = (final_num_page_to_clean * 64 + huge_pg_size-1) / huge_pg_size;
    void* finalCleanReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_final_clean_req});
    void* finalCleanRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_final_clean_rsp});

    void* initPtr = finalCleanReqMem;
    for (int instrIdx = 0; instrIdx < final_clean_instr_pg_num * instr_per_page; instrIdx ++){
      if (instrIdx < final_num_page_to_clean){
        // set instr: erase instr
        int pgIdx = instrIdx;
        initPtr = set_erase_instr(initPtr, all_unique_page_sha3 + pgIdx * 8, false);
      } else {
        // set Insrt: nop
        initPtr = set_nop(initPtr);
      }
    }

    // golden res
    int* finalCleanGoldenPgIsExec = (int*) malloc(final_num_page_to_clean * sizeof(int));
    int* finalCleanGoldenPgRefCount = (int*) malloc(final_num_page_to_clean * sizeof(int));
    int* finalCleanGoldenPgIdx = (int*) malloc(final_num_page_to_clean * sizeof(int));

    for (int pgIdx = 0; pgIdx < final_num_page_to_clean; pgIdx ++){
      finalCleanGoldenPgIsExec[pgIdx] = 1;
      finalCleanGoldenPgRefCount[pgIdx] = 0;
      finalCleanGoldenPgIdx[pgIdx] = 0;
    }

    cproc.setCSR(reinterpret_cast<uint64_t>(finalCleanReqMem), static_cast<uint32_t>(CTLR::RDHOSTADDR));
    cproc.setCSR(reinterpret_cast<uint64_t>(finalCleanRspMem), static_cast<uint32_t>(CTLR::WRHOSTADDR));
    cproc.setCSR(final_clean_instr_pg_num, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch
    cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
    sleep(1);

    std::cout << "read page count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::RDDONE)) << "/" << final_clean_instr_pg_num << std::endl;
    std::cout << "write resp count: " << cproc.getCSR(static_cast<uint32_t>(CTLR::WRDONE)) * 16 << "/" << final_num_page_to_clean << std::endl; 

    ofstream outfile4;
    std::stringstream outfile_name4;
    outfile_name4 << output_dir << "/resp_" << timeStamp.str() << "_step4.txt";
    outfile4.open(outfile_name4.str(), ios::out);

    cout << "parsing the results" << endl;
    allPassed = parse_response(final_num_page_to_clean, finalCleanRspMem, finalCleanGoldenPgIsExec, finalCleanGoldenPgRefCount, finalCleanGoldenPgIdx, 2, outfile4);
    cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
    outfile4.close();
    free(finalCleanGoldenPgIsExec);
    free(finalCleanGoldenPgRefCount);
    free(finalCleanGoldenPgIdx);
    cproc.freeMem(finalCleanReqMem);
    cproc.freeMem(finalCleanRspMem);
  }

  cproc.clearCompleted();
  free(all_unique_page_buffer);
  free(all_unique_page_sha3);

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

void * set_erase_instr(void * startPtr, uint32_t * sha3Val, bool printEn = false){
    uint32_t * startPtrUInt32 = (uint32_t *) startPtr;
    memcpy(startPtrUInt32, (void*) sha3Val, 32);
    startPtrUInt32[9]  = 0;
    startPtrUInt32[10] = 0;
    startPtrUInt32[11] = 0;
    startPtrUInt32[12] = 0;
    startPtrUInt32[13] = 0;
    startPtrUInt32[14] = 0;
    startPtrUInt32[15] = 2 << 30;
    if(printEn){
      std::stringstream SHA3sstream;
      for (int sha3PieceIdx = 7; sha3PieceIdx >= 0; sha3PieceIdx --){
        SHA3sstream << std::setfill ('0') << std::setw(sizeof(uint32_t)*2) << std::hex << sha3Val[sha3PieceIdx];
      }
      std::cout << "Instr: erase, SHA3:" << SHA3sstream.str() << std::endl;
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

bool parse_response(uint32_t pageCount, void* rspMem, int* goldenPgIsExec, int* goldenPgRefCount, int* goldenPgIdx, int goldenOpCode, ofstream& outfile){
  bool allPassed = true;
  uint32_t* rspMemUInt32 = (uint32_t*) rspMem;
  for (int i=0; i < pageCount; i++) {
    uint32_t refCount     = rspMemUInt32[i*16 + 8];
    uint32_t SSDLBA       = rspMemUInt32[i*16 + 9];
    uint32_t hostLBAStart = rspMemUInt32[i*16 + 10];
    uint32_t hostLBALen   = rspMemUInt32[i*16 + 11];
    uint32_t execStatus   = rspMemUInt32[i*16 + 15];
    bool isExec = (execStatus & (1 << 29)) ? true : false; // 1 -> op exec -> new page -> not exist(or GC)
    uint32_t opCode = (execStatus>>30);

    bool pagePassed = true;
    pagePassed = pagePassed && (refCount == goldenPgRefCount[i]);
    pagePassed = pagePassed && (hostLBAStart == goldenPgIdx[i]);
    pagePassed = pagePassed && (hostLBALen == 1);
    pagePassed = pagePassed && (isExec == goldenPgIsExec[i]);
    pagePassed = pagePassed && (opCode == goldenOpCode);

    allPassed = allPassed && pagePassed;

    std::stringstream SHA3sstream;
    for (int sha3PieceIdx = 7; sha3PieceIdx >= 0; sha3PieceIdx --){
      SHA3sstream << std::setfill ('0') << std::setw(sizeof(uint32_t)*2) << std::hex << rspMemUInt32[i*16 + sha3PieceIdx];
    }

    // write to file
    outfile << "page: " << i << ", at SSD LBA: " << SSDLBA << endl;
    outfile << "overall checking:" << pagePassed << endl;
    outfile << "refCount     " << refCount     << "\texpected " << goldenPgRefCount[i] << std::endl;
    outfile << "hostLBAStart " << hostLBAStart << "\texpected " << goldenPgIdx[i]      << std::endl;
    outfile << "hostLBALen   " << hostLBALen   << "\texpected " << 1                   << std::endl;
    outfile << "isExec       " << isExec       << "\texpected " << goldenPgIsExec[i]   << std::endl;
    outfile << "opCode       " << opCode       << "\texpected " << goldenOpCode        << std::endl;
    outfile << "SHA3         " << SHA3sstream.str() << endl;
    outfile << endl;
  }
  return allPassed;
}
