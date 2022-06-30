#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <random>
#include <chrono>
#include <thread>
#include <limits>
#include <assert.h>
#include <string>

#include "ibvQpConn.hpp"

using namespace std;

namespace fpga {


/* ---------------------------------------------------------------------------------------
/* Public
/* ---------------------------------------------------------------------------------------

/**
 * Ctor
 * @param: fdev - attached vFPGA
 * @param: node_id - current node ID
 * @param: n_pages - number of buffer pages
 */
ibvQpConn::ibvQpConn(cProc *fdev, uint32_t node_id, uint32_t n_pages) {
    this->fdev = fdev;
    this->n_pages = n_pages;

    // Conn
    is_connected = false;

    // Initialize local queues
    initLocalQueue(node_id);

    // ARP lookup
    // FIXME: fcnfg.qsfp_rdma is private
    fdev->doArpLookup(0);
}

/**
 * Dtor
 */
ibvQpConn::~ibvQpConn() {
    closeConnection();
    fdev->freeMem((void*) qpair->local.vaddr);
}


static unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

/**
 * Initialization of the local queues
 */
void ibvQpConn::initLocalQueue(uint32_t node_id) {
    std::default_random_engine rand_gen(seed);
    std::uniform_int_distribution<int> distr(0, std::numeric_limits<std::uint32_t>::max());

    uint32_t ib_addr = base_ib_addr + node_id;

    qpair = std::make_unique<ibvQp>();

    // IP 
    qpair->local.uintToGid(0, ib_addr);
    qpair->local.uintToGid(8, ib_addr);
    qpair->local.uintToGid(16, ib_addr);
    qpair->local.uintToGid(24, ib_addr);

    // qpn and psn
    qpair->local.node_id = node_id;
    qpair->local.vfid = fdev->getVfid();
    qpair->local.qpn = fdev->getCpid();
    if(qpair->local.qpn == -1) 
        throw std::runtime_error("Coyote PID incorrect, vfid: " + fdev->getVfid());
    qpair->local.psn = distr(rand_gen) & 0xFFFFFF;
    qpair->local.rkey = 0;

    // Allocate buffer
    void *vaddr = fdev->getMem({CoyoteAlloc::HOST_2M, n_pages});
    qpair->local.vaddr = (uint64_t) vaddr;
    qpair->local.size = n_pages * hugePageSize;
}

/**
 * @brief Set connection
 */
void ibvQpConn::setConnection(int connection) {
    this->connection = connection;
    is_connected = true;
}

void ibvQpConn::closeConnection() {
    if(isConnected()) {
        close(connection);
        is_connected = false;
    }
}

/**
 * @brief Write queue pair context
 */
void ibvQpConn::writeContext(uint16_t port) {
    fdev->writeQpContext(qpair.get());
    fdev->writeConnContext(qpair.get(), port);
}

/**
 * RDMA ops
 * @param: wr - RDMA operation
 */
void ibvQpConn::ibvPostSend(ibvSendWr *wr) {
    if(!is_connected)
        throw std::runtime_error("Queue pair not connected\n");

    fdev->ibvPostSend(qpair.get(), wr);
}

void ibvQpConn::ibvPostGo() {
    if(!is_connected)
        throw std::runtime_error("Queue pair not connected\n");

    fdev->ibvPostGo(qpair.get());
}

/**
 * RDMA polling function for incoming data
 */
uint32_t ibvQpConn::ibvDone() {
    return fdev->checkCompleted(CoyoteOper::WRITE);
}

/**
 * RDMA polling function for outgoing data
 */
uint32_t ibvQpConn::ibvSent() {
    return fdev->checkCompleted(CoyoteOper::READ);
}

/**
 * Clear completed flags
 */
void ibvQpConn::ibvClear() {
    fdev->clearCompleted();
}

/**
 * Sync with remote
 * @param: node_id - target node id
 */
uint32_t ibvQpConn::readAck() {
    uint32_t ack;
   
    if (::read(connection, &ack, sizeof(uint32_t)) != sizeof(uint32_t)) {
        ::close(connection);
        throw std::runtime_error("Could not read ack\n");
    }

    return ack;
}

/**
 * Wait on close remote
 * @param: node_id - target node id
 */
void ibvQpConn::closeAck() {
    uint32_t ack;
    
    if (::read(connection, &ack, sizeof(uint32_t)) == 0) {
        ::close(connection);
    }
}


/**
 * Sync with remote
 * @param: node_id - target node id
 * @param: ack - acknowledge message
 */
void ibvQpConn::sendAck(uint32_t ack) {
    if(::write(connection, &ack, sizeof(uint32_t)) != sizeof(uint32_t))  {
        ::close(connection);
        throw std::runtime_error("Could not send ack\n");
    }
}

/**
 * Sync with remote
 * @param: node_id - target node id
 */
void ibvQpConn::ibvSync(bool mstr) {
    if(mstr) {
        sendAck(0);
        readAck();
    } else {
        readAck();
        sendAck(0);
    }
}

}
