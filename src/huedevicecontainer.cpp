//
//  huedevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huedevicecontainer.hpp"

#include "huedevice.hpp"

using namespace p44;


HueDeviceContainer::HueDeviceContainer(int aInstanceNumber) :
  inherited(aInstanceNumber)
{
}

const char *HueDeviceContainer::deviceClassIdentifier() const
{
  return "hue_Lights_Container";
}


void HueDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  collectedHandler = aCompletedCB;
  // if we have uuid and token of a bridge, try to re-find it
  if (ssdpUuid.length()>0 && apiToken.length()>0) {
    // search bridge by uuid
    bridgeSearcher = SsdpSearchPtr(new SsdpSearch(SyncIOMainLoop::currentMainLoop()));
    bridgeSearcher->startSearch(boost::bind(&HueDeviceContainer::bridgeRefindHandler, this, _1, _2), ssdpUuid.c_str());
  }
}


void HueDeviceContainer::bridgeRefindHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // apparently found bridge, extract base URL
    baseURL = aSsdpSearchP->locationURL;
    LOG(LOG_NOTICE, "Found my hue bridge by uuid: %s\n", baseURL.c_str());
    #warning "// TODO: verify that this is a hue bridge"
    collectedHandler(ErrorPtr()); // ok
  }
  else {
    // not found (usually timeout)
    LOG(LOG_NOTICE, "Error refinding hue bridge with uuid %s, error = %s\n", baseURL.c_str(), aError->description().c_str());
    collectedHandler(ErrorPtr()); // no hue bridge to collect lights from (but this is not a collect error)
  }
  aSsdpSearchP->stopSearch();
  bridgeSearcher.reset();
}



void HueDeviceContainer::setLearnMode(bool aEnableLearning)
{
  if (aEnableLearning) {
    // search for any device
    bridgeSearcher = SsdpSearchPtr(new SsdpSearch(SyncIOMainLoop::currentMainLoop()));
    bridgeSearcher->startSearch(boost::bind(&HueDeviceContainer::bridgeDiscoveryHandler, this, _1, _2), NULL);
  }
  else {
    // stop learning
    bridgeSearcher->stopSearch();
    bridgeSearcher.reset();
  }
}



void HueDeviceContainer::bridgeDiscoveryHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // check device for possibility of being a hue bridge
    LOG(LOG_NOTICE, "candidate device found at %s, server=%s, uuid=%s\n", aSsdpSearchP->locationURL.c_str(), aSsdpSearchP->server.c_str(), aSsdpSearchP->uuid.c_str());
  }
  else {
    LOG(LOG_NOTICE, "discovery failed, error = %s\n", aError->description().c_str());
    aSsdpSearchP->stopSearch();
    bridgeSearcher.reset();
  }
}