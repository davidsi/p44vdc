//
//  serialqueue.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 12.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "serialqueue.hpp"


#define DEFAULT_RECEIVE_TIMEOUT 3000 // 3 seconds


#pragma mark - SerialOperation

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif


SQMilliSeconds SerialOperation::now()
{
  #ifdef __APPLE__
  static bool timeInfoKnown = false;
  static mach_timebase_info_data_t tb;
  if (!timeInfoKnown) {
    mach_timebase_info(&tb);
  }
  double t = mach_absolute_time();
  return t * (double)tb.numer / (double)tb.denom / 1e6; // ms
  #else
  struct timespec tsp;
  clock_gettime(CLOCK_MONOTONIC, &tsp);
  // return milliseconds
  return tsp.tv_sec*1000 + tsp.tv_nsec/1000000;
  #endif
}


SerialOperation::SerialOperation() :
  initiated(false),
  aborted(false),
  timeout(0), // no timeout
  timesOutAt(0), // no timeout time set
  initiationDelay(0), // no initiation delay
  initiatesNotBefore(0), // no initiation time
  inSequence(true) // by default, execute in sequence
{
}

// set transmitter
void SerialOperation::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}

// set timeout
void SerialOperation::setTimeout(SQMilliSeconds aTimeout)
{
  timeout = aTimeout;
}


// set delay for initiation (after first attempt to initiate)
void SerialOperation::setInitiationDelay(SQMilliSeconds aInitiationDelay)
{
  initiationDelay = aInitiationDelay;
  initiatesNotBefore = 0;
}

// set earliest time to execute
void SerialOperation::setInitiatesAt(SQMilliSeconds aInitiatesAt)
{
  initiatesNotBefore = aInitiatesAt;
}


// set callback to execute when operation completes
void SerialOperation::setSerialOperationCB(SerialOperationFinalizeCB aCallBack)
{
  finalizeCallback = aCallBack;
}


// check if can be initiated
bool SerialOperation::canInitiate()
{
  if (initiationDelay>0) {
    if (initiatesNotBefore==0) {
      // first time queried, start delay now
      initiatesNotBefore = SerialOperation::now()+initiationDelay;
      initiationDelay = 0; // consumed
    }
  }
  // can be initiated when delay is over
  return initiatesNotBefore==0 || initiatesNotBefore<SerialOperation::now();
}



// call to initiate operation
bool SerialOperation::initiate()
{
  if (!canInitiate()) return false;
  initiated = true;
  if (timeout!=0)
    timesOutAt = SerialOperation::now()+timeout;
  else
    timesOutAt = 0;
  return initiated;
}

/// check if already initiated
bool SerialOperation::isInitiated()
{
  return initiated;
}

// call to deliver received bytes
size_t SerialOperation::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  return 0;
}


// call to check if operation has completed
bool SerialOperation::hasCompleted()
{
  return true;
}


bool SerialOperation::hasTimedOutAt(SQMilliSeconds aRefTime)
{
  if (timesOutAt==0) return false;
  return aRefTime>=timesOutAt;
}


// call to execute after completion
SerialOperationPtr SerialOperation::finalize(SerialOperationQueue *aQueueP)
{
  if (finalizeCallback) {
    finalizeCallback(this,aQueueP,ErrorPtr());
    finalizeCallback = NULL; // call once only
  }
  return SerialOperationPtr();
}





// call to execute to abort operation
void SerialOperation::abortOperation(ErrorPtr aError)
{
  if (finalizeCallback && !aborted) {
    aborted = true;
    finalizeCallback(this,NULL,aError);
    finalizeCallback = NULL; // call once only
  }
}


#pragma mark - SerialOperationSend


SerialOperationSend::SerialOperationSend(size_t aNumBytes, uint8_t *aBytes)
{
  // copy data
  dataP = NULL;
  dataSize = aNumBytes;
  if (dataSize>0) {
    dataP = (uint8_t *)malloc(dataSize);
    memcpy(dataP, aBytes, dataSize);
  }
}


SerialOperationSend::~SerialOperationSend()
{
  if (dataP) {
    free(dataP);
  }
}


bool SerialOperationSend::initiate()
{
  if (!canInitiate()) return false;
  size_t res;
  if (dataP && transmitter) {
    // transmit
    res = transmitter(dataSize,dataP);
    if (res!=dataSize) {
      // error
      abortOperation(ErrorPtr(new SQError(SQErrorTransmit)));
    }
    // early release
    free(dataP);
    dataP = NULL;
  }
  // executed
  return inherited::initiate();
}



#pragma mark - SerialOperationReceive


SerialOperationReceive::SerialOperationReceive(size_t aExpectedBytes)
{
  // allocate buffer
  expectedBytes = aExpectedBytes;
  dataP = (uint8_t *)malloc(expectedBytes);
  dataIndex = 0;
  setTimeout(DEFAULT_RECEIVE_TIMEOUT);
};


size_t SerialOperationReceive::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  // append bytes into buffer
  if (!initiated)
    return 0; // cannot accept bytes when not yet initiated
  if (aNumBytes>expectedBytes)
    aNumBytes = expectedBytes;
  if (aNumBytes>0) {
    memcpy(dataP+dataIndex, aBytes, aNumBytes);
    dataIndex += aNumBytes;
    expectedBytes -= aNumBytes;
  }
  // return number of bytes actually accepted
  return aNumBytes;
}


bool SerialOperationReceive::hasCompleted()
{
  // completed if all expected bytes received
  return expectedBytes<=0;
}


void SerialOperationReceive::abortOperation(ErrorPtr aError)
{
  expectedBytes = 0; // don't expect any more
  inherited::abortOperation(aError);
}


#pragma mark - SerialOperationSendAndReceive


SerialOperationSendAndReceive::SerialOperationSendAndReceive(size_t aNumBytes, uint8_t *aBytes, size_t aExpectedBytes) :
  inherited(aNumBytes, aBytes),
  expectedBytes(aExpectedBytes)
{
};


SerialOperationPtr SerialOperationSendAndReceive::finalize(SerialOperationQueue *aQueueP)
{
  if (aQueueP) {
    // insert receive operation
    SerialOperationPtr op(new SerialOperationReceive(expectedBytes));
    op->setSerialOperationCB(finalizeCallback); // inherit completion callback
    finalizeCallback = NULL; // prevent it to be called from this object!
    return op;
  }
  return SerialOperationPtr(); // none
}


#pragma mark - SerialOperationQueue



// set transmitter
void SerialOperationQueue::setTransmitter(SerialOperationTransmitter aTransmitter)
{
  transmitter = aTransmitter;
}


// queue a new operation
void SerialOperationQueue::queueOperation(SerialOperationPtr aOperation)
{
  aOperation->setTransmitter(transmitter);
  operationQueue.push_back(aOperation);
}


// deliver bytes to the most recent waiting operation
size_t SerialOperationQueue::acceptBytes(size_t aNumBytes, uint8_t *aBytes)
{
  // first check if some operations still need processing
  processOperations();
  // let operations receive bytes
  size_t acceptedBytes = 0;
  for (operationQueue_t::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
    size_t consumed = (*pos)->acceptBytes(aNumBytes, aBytes);
    aBytes += consumed; // advance pointer
    aNumBytes -= consumed; // count
    acceptedBytes += consumed;
    if (aNumBytes<=0)
      break; // all bytes consumed
  }
  if (aNumBytes>0) {
    // Still bytes left to accept
    // TODO: possibly create "unexpected receive" operation
  }
  // check if some operations might be complete now
  processOperations();
  // return number of accepted bytes
  return acceptedBytes;
};


// process operations now
void SerialOperationQueue::processOperations()
{
  bool processed = false;
  SQMilliSeconds now = SerialOperation::now();
  while (!processed) {
    operationQueue_t::iterator pos;
    // (re)start with first element in queue
    for (pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
      SerialOperationPtr op = *pos;
      if (op->hasTimedOutAt(now)) {
        // remove from list
        operationQueue.erase(pos);
        // abort with timeout
        op->abortOperation(ErrorPtr(new SQError(SQErrorTimedOut)));
        // restart with start of (modified) queue
        break;
      }
      if (!op->isInitiated()) {
        // initiate now
        if (!op->initiate()) {
          // cannot initiate this one now, check if we can continue with others
          if (op->inSequence) {
            // this op needs to be initiated before others can be checked
            processed = true; // something must happen outside this routine to change the state of the op, so done for now
            break;
          }
        }
      }
      if (op->isInitiated()) {
        // initiated, check if already completed
        if (op->hasCompleted()) {
          // operation has completed
          // - remove from list
          operationQueue_t::iterator nextPos = operationQueue.erase(pos);
          // - finalize. This might push new operations in front or back of the queue
          SerialOperationPtr nextOp = op->finalize(this);
          if (nextOp) {
            operationQueue.insert(nextPos, nextOp);
          }
          // restart with start of (modified) queue
          break;
        }
        else {
          // operation has not yet completed
          if (op->inSequence) {
            // this op needs to be complete before others can be checked
            processed = true; // something must happen outside this routine to change the state of the op, so done for now
            break;
          }
        }
      }
    } // for all ops in queue
    if (pos==operationQueue.end()) processed = true; // if seen all, we're done for now as well
  } // while not processed
};



// abort all pending operations
void SerialOperationQueue::abortOperations()
{
  for (operationQueue_t::iterator pos = operationQueue.begin(); pos!=operationQueue.end(); ++pos) {
    (*pos)->abortOperation(ErrorPtr(new SQError(SQErrorAborted)));
  }
  // empty queue
  operationQueue.clear();
}





