//
//  Copyright (c) 2013-2017 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44vdc.
//
//  p44vdc is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44vdc is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44vdc. If not, see <http://www.gnu.org/licenses/>.
//

#include "vdchost.hpp"

#include "vdc.hpp"

#include <string.h>

#include "device.hpp"

#include "macaddress.hpp"

#if ENABLE_LOCAL_BEHAVIOUR
// for local behaviour
#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"
#endif


// TODO: move scene processing to output?
// TODO: enocean outputs need to have a channel, too - which one? For now: always channel 0
// TODO: review output value updating mechanisms, especially in light of MOC transactions


using namespace p44;

// how often to write mainloop statistics into log output
#define DEFAULT_MAINLOOP_STATS_INTERVAL (60) // every 5 min (with periodic activity every 5 seconds: 60*5 = 300 = 5min)

// how long vDC waits after receiving ok from one announce until it fires the next
#define ANNOUNCE_PAUSE (10*MilliSecond)

// how long until a not acknowledged registrations is considered timed out (and next device can be attempted)
#define ANNOUNCE_TIMEOUT (30*Second)

// how long until a not acknowledged announcement for a device is retried again for the same device
#define ANNOUNCE_RETRY_TIMEOUT (300*Second)

// default product name
#define DEFAULT_PRODUCT_NAME "plan44.ch vdcd"

// default description template
#define DEFAULT_DESCRIPTION_TEMPLATE "%V %M%N #%S"


static VdcHost *sharedVdcHostP = NULL;

VdcHost::VdcHost() :
  inheritedParams(dsParamStore),
  mac(0),
  networkConnected(true), // start with the assumption of a connected network
  externalDsuid(false),
  storedDsuid(false),
  DsAddressable(this),
  collecting(false),
  lastActivity(0),
  lastPeriodicRun(0),
  learningMode(false),
  announcementTicket(0),
  periodicTaskTicket(0),
  localDimDirection(0), // undefined
  mainloopStatsInterval(DEFAULT_MAINLOOP_STATS_INTERVAL),
  mainLoopStatsCounter(0),
  productName(DEFAULT_PRODUCT_NAME)
{
  // remember singleton's address
  sharedVdcHostP = this;
  // obtain MAC address
  mac = macAddress();
}


VdcHostPtr VdcHost::sharedVdcHost()
{
  return VdcHostPtr(sharedVdcHostP);
}


void VdcHost::setEventMonitor(VdchostEventCB aEventCB)
{
  eventMonitorHandler = aEventCB;
}


void VdcHost::postEvent(VdchostEvent aEvent)
{
  // let all vdcs know
  for (VdcMap::iterator pos = vdcs.begin(); pos != vdcs.end(); ++pos) {
    pos->second->handleGlobalEvent(aEvent);
  }
  // also let app-level event monitor know
  if (eventMonitorHandler) {
    eventMonitorHandler(aEvent);
  }
}


void VdcHost::setName(const string &aName)
{
  if (aName!=getAssignedName()) {
    // has changed
    inherited::setName(aName);
    // make sure it will be saved
    markDirty();
    // is a global event - might need re-advertising services
    postEvent(vdchost_descriptionchanged);
  }
}



void VdcHost::setIdMode(DsUidPtr aExternalDsUid)
{
  if (aExternalDsUid) {
    externalDsuid = true;
    dSUID = *aExternalDsUid;
  }
}



void VdcHost::addVdc(VdcPtr aVdcPtr)
{
  vdcs[aVdcPtr->getDsUid()] = aVdcPtr;
}



void VdcHost::setIconDir(const char *aIconDir)
{
	iconDir = nonNullCStr(aIconDir);
	if (!iconDir.empty() && iconDir[iconDir.length()-1]!='/') {
		iconDir.append("/");
	}
}


const char *VdcHost::getIconDir()
{
	return iconDir.c_str();
}





void VdcHost::setPersistentDataDir(const char *aPersistentDataDir)
{
	persistentDataDir = nonNullCStr(aPersistentDataDir);
	if (!persistentDataDir.empty() && persistentDataDir[persistentDataDir.length()-1]!='/') {
		persistentDataDir.append("/");
	}
}


const char *VdcHost::getPersistentDataDir()
{
	return persistentDataDir.c_str();
}



string VdcHost::publishedDescription()
{
  // derive the descriptive name
  // "%V %M%N %S"
  string n = descriptionTemplate;
  if (n.empty()) n = DEFAULT_DESCRIPTION_TEMPLATE;
  string s;
  size_t i;
  // Vendor
  while ((i = n.find("%V"))!=string::npos) { n.replace(i, 2, vendorName()); }
  // Model
  while ((i = n.find("%M"))!=string::npos) { n.replace(i, 2, modelName()); }
  // (optional) Name
  s = getName();
  if (!s.empty()) {
    s = " \""+s+"\"";
  }
  while ((i = n.find("%N"))!=string::npos) { n.replace(i, 2, s); }
  // Serial/hardware ID
  s = getDeviceHardwareId();
  if (s.empty()) {
    // use dSUID if no other ID is specified
    s = getDsUid().getString();
  }
  while ((i = n.find("%S"))!=string::npos) { n.replace(i, 2, s); }
  return n;
}


// MARK: ===== global status

bool VdcHost::isApiConnected()
{
  return getSessionConnection()!=NULL;
}


bool VdcHost::isNetworkConnected()
{
  uint32_t ipv4 = ipv4Address();
  // Only consider connected if we have a IP address, and none from the 169.254.0.0/16
  // link-local autoconfigured ones (RFC 3927/APIPA).
  bool nowConnected = (ipv4!=0) && ((ipv4 & 0xFFFF0000)!=0xA9FE0000);
  if (nowConnected!=networkConnected) {
    // change in connection status - post it
    networkConnected = nowConnected;
    LOG(LOG_NOTICE, "*** Network connection %s", networkConnected ? "re-established" : "lost");
    postEvent(networkConnected ? vdchost_network_reconnected : vdchost_network_lost);
  }
  return networkConnected;
}




// MARK: ===== initializisation of DB and containers


// Version history
//  1 : alpha/beta phase DB
//  2 : no schema change, but forced re-creation due to changed scale of brightness (0..100 now, was 0..255 before)
//  3 : no schema change, but forced re-creation due to bug in storing output behaviour settings
#define DSPARAMS_SCHEMA_MIN_VERSION 3 // minimally supported version, anything older will be deleted
#define DSPARAMS_SCHEMA_VERSION 3 // current version

string DsParamStore::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
{
  string sql;
  if (aFromVersion==0) {
    // create DB from scratch
		// - use standard globs table for schema version
    sql = inherited::dbSchemaUpgradeSQL(aFromVersion, aToVersion);
		// - no vdchost level table to create at this time
    //   (PersistentParams create and update their tables as needed)
    // reached final version in one step
    aToVersion = DSPARAMS_SCHEMA_VERSION;
  }
  return sql;
}



void VdcHost::prepareForVdcs(bool aFactoryReset)
{
  // initialize dsParamsDB database
  string databaseName = getPersistentDataDir();
  string_format_append(databaseName, "DsParams.sqlite3");
  ErrorPtr error = dsParamStore.connectAndInitialize(databaseName.c_str(), DSPARAMS_SCHEMA_VERSION, DSPARAMS_SCHEMA_MIN_VERSION, aFactoryReset);
  // load the vdc host settings and determine the dSUID (external > stored > mac-derived)
  loadAndFixDsUID();
}


void VdcHost::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
  // Log start message
  LOG(LOG_NOTICE,
    "\n\n\n*** starting initialisation of vcd host '%s'\n*** dSUID (%s) = %s, MAC: %s, IP = %s\n",
    publishedDescription().c_str(),
    externalDsuid ? "external" : "MAC-derived",
    shortDesc().c_str(),
    macAddressToString(mac, ':').c_str(),
    ipv4ToString(ipv4Address()).c_str()
  );
  // start the API server
  if (vdcApiServer) {
    vdcApiServer->setConnectionStatusHandler(boost::bind(&VdcHost::vdcApiConnectionStatusHandler, this, _1, _2));
    vdcApiServer->start();
  }
  // start initialisation of class containers
  initializeNextVdc(aCompletedCB, aFactoryReset, vdcs.begin());
}



void VdcHost::initializeNextVdc(StatusCB aCompletedCB, bool aFactoryReset, VdcMap::iterator aNextVdc)
{
  // initialize all vDCs, even when some
  if (aNextVdc!=vdcs.end()) {
    aNextVdc->second->initialize(boost::bind(&VdcHost::vdcInitialized, this, aCompletedCB, aFactoryReset, aNextVdc, _1), aFactoryReset);
    return;
  }
  // successfully done
  postEvent(vdchost_vdcs_initialized);
  aCompletedCB(ErrorPtr());
}


void VdcHost::vdcInitialized(StatusCB aCompletedCB, bool aFactoryReset, VdcMap::iterator aNextVdc, ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    LOG(LOG_ERR, "vDC %s: failed to initialize: %s", aNextVdc->second->shortDesc().c_str(), aError->description().c_str());
  }
  // anyway, initialize next
  aNextVdc++;
  initializeNextVdc(aCompletedCB, aFactoryReset, aNextVdc);
}



void VdcHost::startRunning()
{
  // force initial network connection check
  // Note: will NOT post re-connected message if we're initializing normally with network up,
  //   but will post network lost event if we do NOT have a connection now.
  isNetworkConnected();
  // start periodic tasks needed during normal running like announcement checking and saving parameters
  MainLoop::currentMainLoop().executeOnce(boost::bind(&VdcHost::periodicTask, vdcHostP, _1), 1*Second);
}



// MARK: ===== collect devices


void VdcHost::collectDevices(StatusCB aCompletedCB, RescanMode aRescanFlags)
{
  if (!collecting) {
    collecting = true;
    if ((aRescanFlags & rescanmode_incremental)==0) {
      // only for non-incremental collect, close vdsm connection
      if (activeSessionConnection) {
        LOG(LOG_NOTICE, "requested to re-collect devices -> closing vDC API connection");
        activeSessionConnection->closeConnection(); // close the API connection
        resetAnnouncing();
        activeSessionConnection.reset(); // forget connection
        postEvent(vdchost_vdcapi_disconnected);
      }
      dSDevices.clear(); // forget existing ones
    }
    collectFromNextVdc(aCompletedCB, aRescanFlags, vdcs.begin());
  }
}


void VdcHost::collectFromNextVdc(StatusCB aCompletedCB, RescanMode aRescanFlags, VdcMap::iterator aNextVdc)
{
  if (aNextVdc!=vdcs.end()) {
    VdcPtr vdc = aNextVdc->second;
    LOG(LOG_NOTICE,
      "=== collecting devices from vdc %s (%s #%d)",
      vdc->shortDesc().c_str(),
      vdc->vdcClassIdentifier(),
      vdc->getInstanceNumber()
    );
    vdc->collectDevices(boost::bind(&VdcHost::vdcCollected, this, aCompletedCB, aRescanFlags, aNextVdc, _1), aRescanFlags);
    return;
  }
  // all devices collected, but not yet initialized
  postEvent(vdchost_devices_collected);
  // now initialize devices (which are already identified by now!)
  initializeNextDevice(aCompletedCB, dSDevices.begin());
}


void VdcHost::vdcCollected(StatusCB aCompletedCB, RescanMode aRescanFlags, VdcMap::iterator aNextVdc, ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    LOG(LOG_ERR, "vDC %s: error collecting devices: %s", aNextVdc->second->shortDesc().c_str(), aError->description().c_str());
  }
  // load persistent params for vdc
  aNextVdc->second->load();
  LOG(LOG_NOTICE, "=== done collecting from %s\n", aNextVdc->second->shortDesc().c_str());
  // next
  aNextVdc++;
  collectFromNextVdc(aCompletedCB, aRescanFlags, aNextVdc);
}


void VdcHost::initializeNextDevice(StatusCB aCompletedCB, DsDeviceMap::iterator aNextDevice)
{
  if (aNextDevice!=dSDevices.end()) {
    // TODO: now never doing factory reset init, maybe parametrize later
    aNextDevice->second->initializeDevice(boost::bind(&VdcHost::deviceInitialized, this, aCompletedCB, aNextDevice, _1), false);
    return;
  }
  // all devices initialized
  postEvent(vdchost_devices_initialized);
  aCompletedCB(ErrorPtr());
  collecting = false;
}


void VdcHost::deviceInitialized(StatusCB aCompletedCB, DsDeviceMap::iterator aNextDevice, ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    LOG(LOG_ERR, "*** error initializing device %s: %s", aNextDevice->second->shortDesc().c_str(), aError->description().c_str());
  }
  else {
    LOG(LOG_NOTICE, "--- initialized device: %s",aNextDevice->second->description().c_str());
  }
  // check next
  ++aNextDevice;
  initializeNextDevice(aCompletedCB, aNextDevice);
}



// MARK: ===== adding/removing devices


// add a new device, replaces possibly existing one based on dSUID
bool VdcHost::addDevice(DevicePtr aDevice)
{
  if (!aDevice)
    return false; // no device, nothing added
  // check if device with same dSUID already exists
  DsDeviceMap::iterator pos = dSDevices.find(aDevice->getDsUid());
  if (pos!=dSDevices.end()) {
    LOG(LOG_INFO, "- device %s already registered, not added again",aDevice->shortDesc().c_str());
    return false; // duplicate dSUID, not added
  }
  // set for given dSUID in the container-wide map of devices
  dSDevices[aDevice->getDsUid()] = aDevice;
  LOG(LOG_NOTICE, "--- added device: %s (not yet initialized)",aDevice->shortDesc().c_str());
  // load the device's persistent params
  aDevice->load();
  // if not collecting, initialize device right away.
  // Otherwise, initialisation will be done when collecting is complete
  if (!collecting) {
    aDevice->initializeDevice(boost::bind(&VdcHost::deviceInitialized, this, aDevice), false);
  }
  return true;
}

void VdcHost::deviceInitialized(DevicePtr aDevice)
{
  LOG(LOG_NOTICE, "--- initialized device: %s",aDevice->description().c_str());
  // trigger announcing when initialized (no problem when called while already announcing)
  startAnnouncing();
}




// remove a device from container list (but does not disconnect it!)
void VdcHost::removeDevice(DevicePtr aDevice, bool aForget)
{
  if (aForget) {
    // permanently remove from DB
    aDevice->forget();
  }
  else {
    // save, as we don't want to forget the settings associated with the device
    aDevice->save();
  }
  // remove from container-wide map of devices
  dSDevices.erase(aDevice->getDsUid());
  LOG(LOG_NOTICE, "--- removed device: %s", aDevice->shortDesc().c_str());
}



void VdcHost::startLearning(LearnCB aLearnHandler, bool aDisableProximityCheck)
{
  // enable learning in all class containers
  learnHandler = aLearnHandler;
  learningMode = true;
  LOG(LOG_NOTICE, "=== start learning%s", aDisableProximityCheck ? " with proximity check disabled" : "");
  for (VdcMap::iterator pos = vdcs.begin(); pos != vdcs.end(); ++pos) {
    pos->second->setLearnMode(true, aDisableProximityCheck, undefined);
  }
}


void VdcHost::stopLearning()
{
  // disable learning in all class containers
  for (VdcMap::iterator pos = vdcs.begin(); pos != vdcs.end(); ++pos) {
    pos->second->setLearnMode(false, false, undefined);
  }
  LOG(LOG_NOTICE, "=== stopped learning");
  learningMode = false;
  learnHandler.clear();
}


void VdcHost::reportLearnEvent(bool aLearnIn, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aLearnIn) {
      LOG(LOG_NOTICE, "--- learned in (paired) new device(s)");
    }
    else {
      LOG(LOG_NOTICE, "--- learned out (unpaired) device(s)");
    }
  }
  // report status
  if (learnHandler) {
    learnHandler(aLearnIn, aError);
  }
}



// MARK: ===== activity monitoring


void VdcHost::signalActivity()
{
  lastActivity = MainLoop::now();
  postEvent(vdchost_activitysignal);
}



void VdcHost::setUserActionMonitor(DeviceUserActionCB aUserActionCB)
{
  deviceUserActionHandler = aUserActionCB;
}


bool VdcHost::signalDeviceUserAction(Device &aDevice, bool aRegular)
{
  LOG(LOG_INFO, "vdSD %s: reports %s user action", aDevice.shortDesc().c_str(), aRegular ? "regular" : "identification");
  if (deviceUserActionHandler) {
    deviceUserActionHandler(DevicePtr(&aDevice), aRegular);
    return true; // suppress normal action
  }
  if (!aRegular) {
    // this is a non-regular user action, i.e. one for identification purposes. Generate special identification notification
    VdcApiConnectionPtr api = getSessionConnection();
    if (api) {
      // send an identify notification
      aDevice.sendRequest("identify", ApiValuePtr(), NULL);
    }
    return true; // no normal action, prevent further processing
  }
  return false; // normal processing
}




// MARK: ===== periodic activity


#define PERIODIC_TASK_INTERVAL (5*Second)
#define PERIODIC_TASK_FORCE_INTERVAL (1*Minute)

#define ACTIVITY_PAUSE_INTERVAL (1*Second)

void VdcHost::periodicTask(MLMicroSeconds aCycleStartTime)
{
  // cancel any pending executions
  MainLoop::currentMainLoop().cancelExecutionTicket(periodicTaskTicket);
  // prevent during activity as saving DB might affect performance
  if (
    (aCycleStartTime>lastActivity+ACTIVITY_PAUSE_INTERVAL) || // some time passed after last activity or...
    (aCycleStartTime>lastPeriodicRun+PERIODIC_TASK_FORCE_INTERVAL) // ...too much time passed since last run
  ) {
    lastPeriodicRun = aCycleStartTime;
    if (!collecting) {
      // re-check network connection, might cause re-collection in some vdcs
      isNetworkConnected();
      // check again for devices that need to be announced
      startAnnouncing();
      // do a save run as well
      // - myself
      save();
      // - device containers
      for (VdcMap::iterator pos = vdcs.begin(); pos!=vdcs.end(); ++pos) {
        pos->second->save();
      }
      // - devices
      for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
        pos->second->save();
      }
    }
  }
  if (mainloopStatsInterval>0) {
    // show mainloop statistics
    if (mainLoopStatsCounter<=0) {
      LOG(LOG_INFO, "%s", MainLoop::currentMainLoop().description().c_str());
      MainLoop::currentMainLoop().statistics_reset();
      mainLoopStatsCounter = mainloopStatsInterval;
    }
    else {
      --mainLoopStatsCounter;
    }
  }
  // schedule next run
  periodicTaskTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VdcHost::periodicTask, this, _1), PERIODIC_TASK_INTERVAL);
}


// MARK: ===== local operation mode


void VdcHost::checkForLocalClickHandling(ButtonBehaviour &aButtonBehaviour, DsClickType aClickType)
{
  if (!activeSessionConnection) {
    // not connected to a vdSM, handle clicks locally
    handleClickLocally(aButtonBehaviour, aClickType);
  }
}


void VdcHost::handleClickLocally(ButtonBehaviour &aButtonBehaviour, DsClickType aClickType)
{
  #if ENABLE_LOCAL_BEHAVIOUR
  // TODO: Not really conforming to ds-light yet...
  int scene = -1; // none
  // if button has up/down, direction is derived from button
  int newDirection = aButtonBehaviour.localFunctionElement()==buttonElement_up ? 1 : (aButtonBehaviour.localFunctionElement()==buttonElement_down ? -1 : 0); // -1=down/off, 1=up/on, 0=toggle
  if (newDirection!=0)
    localDimDirection = newDirection;
  switch (aClickType) {
    case ct_tip_1x:
    case ct_click_1x:
      scene = ROOM_ON;
      // toggle direction if click has none
      if (newDirection==0)
        localDimDirection *= -1; // reverse if already determined
      break;
    case ct_tip_2x:
    case ct_click_2x:
      scene = PRESET_2;
      break;
    case ct_tip_3x:
    case ct_click_3x:
      scene = PRESET_3;
      break;
    case ct_tip_4x:
      scene = PRESET_4;
      break;
    case ct_hold_start:
      scene = INC_S; // just as a marker to start dimming (we'll use dimChannelForArea(), not legacy dimming!)
      // toggle direction if click has none
      if (newDirection==0)
        localDimDirection *= -1; // reverse if already determined
      break;
    case ct_hold_end:
      scene = STOP_S; // just as a marker to stop dimming (we'll use dimChannelForArea(), not legacy dimming!)
      break;
    default:
      break;
  }
  if (scene>=0) {
    DsChannelType channeltype = channeltype_brightness; // default to brightness
    if (aButtonBehaviour.buttonChannel!=channeltype_default) {
      channeltype = aButtonBehaviour.buttonChannel;
    }
    signalActivity(); // local activity
    // some action to perform on every light device
    for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
      DevicePtr dev = pos->second;
      if (scene==STOP_S) {
        // stop dimming
        dev->dimChannelForArea(channeltype, dimmode_stop, 0, 0);
      }
      else {
        // call scene or start dimming
        LightBehaviourPtr l = boost::dynamic_pointer_cast<LightBehaviour>(dev->output);
        if (l) {
          // - figure out direction if not already known
          if (localDimDirection==0 && l->brightness->getLastSync()!=Never) {
            // get initial direction from current value of first encountered light with synchronized brightness value
            localDimDirection = l->brightness->getChannelValue() >= l->brightness->getMinDim() ? -1 : 1;
          }
          if (scene==INC_S) {
            // Start dimming
            // - minimum scene if not already there
            if (localDimDirection>0 && l->brightness->getChannelValue()==0) {
              // starting dimming up from minimum
              l->brightness->setChannelValue(l->brightness->getMinDim(), 0, true);
            }
            // now dim (safety timeout after 10 seconds)
            dev->dimChannelForArea(channeltype, localDimDirection>0 ? dimmode_up : dimmode_down, 0, 10*Second);
          }
          else {
            // call a scene
            if (localDimDirection<0)
              scene = ROOM_OFF; // switching off a scene = call off scene
            dev->callScene(scene, true);
          }
        }
      }
    }
  }
  #endif // ENABLE_LOCAL_BEHAVIOUR
}



// MARK: ===== vDC API


bool VdcHost::sendApiRequest(const string &aMethod, ApiValuePtr aParams, VdcApiResponseCB aResponseHandler)
{
  if (activeSessionConnection) {
    signalActivity();
    return Error::isOK(activeSessionConnection->sendRequest(aMethod, aParams, aResponseHandler));
  }
  // cannot send
  return false;
}


void VdcHost::vdcApiConnectionStatusHandler(VdcApiConnectionPtr aApiConnection, ErrorPtr &aError)
{
  if (Error::isOK(aError)) {
    // new connection, set up reequest handler
    aApiConnection->setRequestHandler(boost::bind(&VdcHost::vdcApiRequestHandler, this, _1, _2, _3, _4));
  }
  else {
    // error or connection closed
    LOG(LOG_ERR, "vDC API connection closing, reason: %s", aError->description().c_str());
    // - close if not already closed
    aApiConnection->closeConnection();
    if (aApiConnection==activeSessionConnection) {
      // this is the active session connection
      resetAnnouncing(); // stop possibly ongoing announcing
      activeSessionConnection.reset();
      postEvent(vdchost_vdcapi_disconnected);
      LOG(LOG_NOTICE, "vDC API session ends because connection closed ");
    }
    else {
      LOG(LOG_NOTICE, "vDC API connection (not yet in session) closed ");
    }
  }
}


void VdcHost::vdcApiRequestHandler(VdcApiConnectionPtr aApiConnection, VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  signalActivity();
  // now process
  if (aRequest) {
    // Methods
    // - Check session init/end methods
    if (aMethod=="hello") {
      respErr = helloHandler(aRequest, aParams);
    }
    else if (aMethod=="bye") {
      respErr = byeHandler(aRequest, aParams);
    }
    else {
      if (!activeSessionConnection) {
        // all following methods must have an active session
        respErr = Error::err<VdcApiError>(401, "no vDC session - cannot call method");
      }
      else {
        // session active - all commands need dSUID parameter
        DsUid dsuid;
        if (Error::isOK(respErr = checkDsuidParam(aParams, "dSUID", dsuid))) {
          // operation method
          respErr = handleMethodForDsUid(aMethod, aRequest, dsuid, aParams);
        }
      }
    }
  }
  else {
    // Notifications
    // Note: out of session, notifications are simply ignored
    if (activeSessionConnection) {
      // Notifications can be adressed to one or multiple dSUIDs
      // Notes
      // - for protobuf API, dSUID is always an array (as it is a repeated field in protobuf)
      // - for JSON API, caller may provide an array or a single dSUID.
      ApiValuePtr o;
      respErr = checkParam(aParams, "dSUID", o);
      if (Error::isOK(respErr)) {
        DsUid dsuid;
        // can be single dSUID or array of dSUIDs
        if (o->isType(apivalue_array)) {
          // array of dSUIDs
          for (int i=0; i<o->arrayLength(); i++) {
            ApiValuePtr e = o->arrayGet(i);
            dsuid.setAsBinary(e->binaryValue());
            handleNotificationForDsUid(aMethod, dsuid, aParams);
          }
        }
        else {
          // single dSUID
          dsuid.setAsBinary(o->binaryValue());
          handleNotificationForDsUid(aMethod, dsuid, aParams);
        }
      }
    }
    else {
      LOG(LOG_DEBUG, "Received notification '%s' out of session -> ignored", aMethod.c_str());
    }
  }
  // check status
  // Note: in case method call triggers an action that does not immediately complete,
  //   we'll get NULL for respErr here, and method handler must take care of acknowledging the method call!
  if (respErr) {
    // method call immediately returned a status (might be explicit OK error object)
    if (aRequest) {
      // report back in case of method call
      aRequest->sendStatus(respErr);
    }
    else {
      // just log in case of error of a notification
      if (!Error::isOK(respErr)) {
        LOG(LOG_WARNING, "Notification '%s' processing error: %s", aMethod.c_str(), respErr->description().c_str());
      }
    }
  }
}


/// vDC API version
/// 1 (aka 1.0 in JSON) : first version, used in P44-DSB-DEH versions up to 0.5.0.x
/// 2 : cleanup, no official JSON support any more, added MOC extensions
#define VDC_API_VERSION 2

ErrorPtr VdcHost::helloHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  ApiValuePtr v;
  string s;
  // check API version
  if (Error::isOK(respErr = checkParam(aParams, "api_version", v))) {
    if (v->int32Value()!=VDC_API_VERSION)
      respErr = Error::err<VdcApiError>(505, "Incompatible vDC API version - found %d, expected %d", v->int32Value(), VDC_API_VERSION);
    else {
      // API version ok, check dSUID
      DsUid vdsmDsUid;
      if (Error::isOK(respErr = checkDsuidParam(aParams, "dSUID", vdsmDsUid))) {
        // same vdSM can restart session any time. Others will be rejected
        if (!activeSessionConnection || vdsmDsUid==connectedVdsm) {
          // ok to start new session
          if (activeSessionConnection) {
            // session connection was already there, re-announce
            resetAnnouncing();
          }
          // - start session with this vdSM
          connectedVdsm = vdsmDsUid;
          // - remember the session's connection
          activeSessionConnection = aRequest->connection();
          postEvent(vdchost_vdcapi_connected);
          // - create answer
          ApiValuePtr result = activeSessionConnection->newApiValue();
          result->setType(apivalue_object);
          result->add("dSUID", aParams->newBinary(getDsUid().getBinary()));
          aRequest->sendResult(result);
          // - trigger announcing devices
          startAnnouncing();
        }
        else {
          // not ok to start new session, reject
          respErr = Error::err<VdcApiError>(503, "this vDC already has an active session with vdSM %s",connectedVdsm.getString().c_str());
          aRequest->sendError(respErr);
          // close after send
          aRequest->connection()->closeAfterSend();
          // prevent sending error again
          respErr.reset();
        }
      }
    }
  }
  return respErr;
}


ErrorPtr VdcHost::byeHandler(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  // always confirm Bye, even out-of-session, so using aJsonRpcComm directly to answer (jsonSessionComm might not be ready)
  aRequest->sendResult(ApiValuePtr());
  // close after send
  aRequest->connection()->closeAfterSend();
  // success
  return ErrorPtr();
}



DsAddressablePtr VdcHost::addressableForParams(const DsUid &aDsUid, ApiValuePtr aParams)
{
  if (aDsUid.empty()) {
    // not addressing by dSUID, check for alternative addressing methods
    ApiValuePtr o = aParams->get("x-p44-itemSpec");
    if (o) {
      string query = o->stringValue();
      if(query.find("vdc:")==0) {
        // starts with "vdc:" -> look for vdc by class identifier and instance no
        query.erase(0, 4); // remove "vdc:" prefix
        // ccccccc[:ii] cccc=vdcClassIdentifier(), ii=instance
        size_t i=query.find(':');
        int instanceNo = 1; // default to first instance
        if (i!=string::npos) {
          // with instance number
          instanceNo = atoi(query.c_str()+i+1);
          query.erase(i); // cut off :iii part
        }
        for (VdcMap::iterator pos = vdcs.begin(); pos!=vdcs.end(); ++pos) {
          VdcPtr c = pos->second;
          if (
            strcmp(c->vdcClassIdentifier(), query.c_str())==0 &&
            c->getInstanceNumber()==instanceNo
          ) {
            // found - return this vDC container
            return c;
          }
        }
      }
      // x-p44-query specified, but nothing found
      return DsAddressablePtr();
    }
    // empty dSUID but no special query: default to vdchost itself (root object)
    return DsAddressablePtr(this);
  }
  // not special query, not empty dSUID
  if (aDsUid==getDsUid()) {
    // my own dSUID: vdc-host is addressed
    return DsAddressablePtr(this);
  }
  else {
    // Must be device or vdc level
    // - find device to handle it (more probable case)
    DsDeviceMap::iterator pos = dSDevices.find(aDsUid);
    if (pos!=dSDevices.end()) {
      return pos->second;
    }
    else {
      // is not a device, try vdcs
      VdcMap::iterator pos = vdcs.find(aDsUid);
      if (pos!=vdcs.end()) {
        return pos->second;
      }
    }
  }
  // not found
  return DsAddressablePtr();
}



ErrorPtr VdcHost::handleMethodForDsUid(const string &aMethod, VdcApiRequestPtr aRequest, const DsUid &aDsUid, ApiValuePtr aParams)
{
  DsAddressablePtr addressable = addressableForParams(aDsUid, aParams);
  if (addressable) {
    // check special case of device remove command - we must execute this because device should not try to remove itself
    DevicePtr dev = boost::dynamic_pointer_cast<Device>(addressable);
    if (dev && aMethod=="remove") {
      return removeHandler(aRequest, dev);
    }
    // non-device addressable or not remove -> just let addressable handle the method itself
    return addressable->handleMethod(aRequest, aMethod, aParams);
  }
  else {
    LOG(LOG_WARNING, "Target entity %s not found for method '%s'", aDsUid.getString().c_str(), aMethod.c_str());
    return Error::err<VdcApiError>(404, "unknown dSUID");
  }
}



void VdcHost::handleNotificationForDsUid(const string &aMethod, const DsUid &aDsUid, ApiValuePtr aParams)
{
  DsAddressablePtr addressable = addressableForParams(aDsUid, aParams);
  if (addressable) {
    addressable->handleNotification(aMethod, aParams);
  }
  else {
    LOG(LOG_WARNING, "Target entity %s not found for notification '%s'", aDsUid.getString().c_str(), aMethod.c_str());
  }
}



// MARK: ===== vDC level methods and notifications


ErrorPtr VdcHost::removeHandler(VdcApiRequestPtr aRequest, DevicePtr aDevice)
{
  // dS system wants to disconnect this device from this vDC. Try it and report back success or failure
  // Note: as disconnect() removes device from all containers, only aDevice may keep it alive until disconnection is complete.
  //   That's why we are passing aDevice to the handler, so we can be certain the device lives long enough
  aDevice->disconnect(true, boost::bind(&VdcHost::removeResultHandler, this, aDevice, aRequest, _1));
  return ErrorPtr();
}


void VdcHost::removeResultHandler(DevicePtr aDevice, VdcApiRequestPtr aRequest, bool aDisconnected)
{
  if (aDisconnected)
    aRequest->sendResult(ApiValuePtr()); // disconnected successfully
  else
    aRequest->sendError(Error::err<VdcApiError>(403, "Device cannot be removed, is still connected"));
}



// MARK: ===== session management


/// reset announcing devices (next startAnnouncing will restart from beginning)
void VdcHost::resetAnnouncing()
{
  // end pending announcement
  MainLoop::currentMainLoop().cancelExecutionTicket(announcementTicket);
  // end all device sessions
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    dev->announced = Never;
    dev->announcing = Never;
  }
  // end all vdc sessions
  for (VdcMap::iterator pos = vdcs.begin(); pos!=vdcs.end(); ++pos) {
    VdcPtr vdc = pos->second;
    vdc->announced = Never;
    vdc->announcing = Never;
  }
}



/// start announcing all not-yet announced entities to the vdSM
void VdcHost::startAnnouncing()
{
  if (!collecting && announcementTicket==0 && activeSessionConnection) {
    announceNext();
  }
}


void VdcHost::announceNext()
{
  if (collecting) return; // prevent announcements during collect.
  // cancel re-announcing
  MainLoop::currentMainLoop().cancelExecutionTicket(announcementTicket);
  // announce vdcs first
  for (VdcMap::iterator pos = vdcs.begin(); pos!=vdcs.end(); ++pos) {
    VdcPtr vdc = pos->second;
    if (
      vdc->isPublicDS() && // only public ones
      vdc->announced==Never &&
      (vdc->announcing==Never || MainLoop::now()>vdc->announcing+ANNOUNCE_RETRY_TIMEOUT) &&
      (!vdc->invisibleWhenEmpty() || vdc->getNumberOfDevices()>0)
    ) {
      // mark device as being in process of getting announced
      vdc->announcing = MainLoop::now();
      // call announcevdc method (need to construct here, because dSUID must be sent as vdcdSUID)
      ApiValuePtr params = getSessionConnection()->newApiValue();
      params->setType(apivalue_object);
      params->add("dSUID", params->newBinary(vdc->getDsUid().getBinary()));
      if (!sendApiRequest("announcevdc", params, boost::bind(&VdcHost::announceResultHandler, this, vdc, _2, _3, _4))) {
        LOG(LOG_ERR, "Could not send vdc announcement message for %s %s", vdc->entityType(), vdc->shortDesc().c_str());
        vdc->announcing = Never; // not registering
      }
      else {
        LOG(LOG_NOTICE, "Sent vdc announcement for %s %s", vdc->entityType(), vdc->shortDesc().c_str());
      }
      // schedule a retry
      announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VdcHost::announceNext, this), ANNOUNCE_TIMEOUT);
      // done for now, continues after ANNOUNCE_TIMEOUT or when registration acknowledged
      return;
    }
  }
  // check all devices for unnannounced ones and announce those
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    if (
      dev->isPublicDS() && // only public ones
      (dev->vdcP->announced!=Never) && // class container must have already completed an announcement
      dev->announced==Never &&
      (dev->announcing==Never || MainLoop::now()>dev->announcing+ANNOUNCE_RETRY_TIMEOUT)
    ) {
      // mark device as being in process of getting announced
      dev->announcing = MainLoop::now();
      // call announce method
      ApiValuePtr params = getSessionConnection()->newApiValue();
      params->setType(apivalue_object);
      // include link to vdc for device announcements
      params->add("vdc_dSUID", params->newBinary(dev->vdcP->getDsUid().getBinary()));
      if (!dev->sendRequest("announcedevice", params, boost::bind(&VdcHost::announceResultHandler, this, dev, _2, _3, _4))) {
        LOG(LOG_ERR, "Could not send device announcement message for %s %s", dev->entityType(), dev->shortDesc().c_str());
        dev->announcing = Never; // not registering
      }
      else {
        LOG(LOG_NOTICE, "Sent device announcement for %s %s", dev->entityType(), dev->shortDesc().c_str());
      }
      // schedule a retry
      announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VdcHost::announceNext, this), ANNOUNCE_TIMEOUT);
      // done for now, continues after ANNOUNCE_TIMEOUT or when registration acknowledged
      return;
    }
  }
}


void VdcHost::announceResultHandler(DsAddressablePtr aAddressable, VdcApiRequestPtr aRequest, ErrorPtr &aError, ApiValuePtr aResultOrErrorData)
{
  if (Error::isOK(aError)) {
    // set device announced successfully
    LOG(LOG_NOTICE, "Announcement for %s %s acknowledged by vdSM", aAddressable->entityType(), aAddressable->shortDesc().c_str());
    aAddressable->announced = MainLoop::now();
    aAddressable->announcing = Never; // not announcing any more
  }
  // cancel retry timer
  MainLoop::currentMainLoop().cancelExecutionTicket(announcementTicket);
  // try next announcement, after a pause
  announcementTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&VdcHost::announceNext, this), ANNOUNCE_PAUSE);
}


// MARK: ===== DsAddressable API implementation

ErrorPtr VdcHost::handleMethod(VdcApiRequestPtr aRequest,  const string &aMethod, ApiValuePtr aParams)
{
  return inherited::handleMethod(aRequest, aMethod, aParams);
}


void VdcHost::handleNotification(const string &aMethod, ApiValuePtr aParams)
{
  inherited::handleNotification(aMethod, aParams);
}



// MARK: ===== property access

static char devicecontainer_key;
static char vdc_container_key;
static char vdc_key;

enum {
  vdcs_key,
  valueSources_key,
  webui_url_key,
  numDeviceContainerProperties
};



int VdcHost::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor->hasObjectKey(vdc_container_key)) {
    return (int)vdcs.size();
  }
  return inherited::numProps(aDomain, aParentDescriptor)+numDeviceContainerProperties;
}


// note: is only called when getDescriptorByName does not resolve the name
PropertyDescriptorPtr VdcHost::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numDeviceContainerProperties] = {
    { "x-p44-vdcs", apivalue_object+propflag_container, vdcs_key, OKEY(vdc_container_key) },
    { "x-p44-valueSources", apivalue_null, valueSources_key, OKEY(devicecontainer_key) },
    { "configURL", apivalue_string, webui_url_key, OKEY(devicecontainer_key) }
  };
  int n = inherited::numProps(aDomain, aParentDescriptor);
  if (aPropIndex<n)
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


PropertyDescriptorPtr VdcHost::getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyAccessMode aMode, PropertyDescriptorPtr aParentDescriptor)
{
  if (aParentDescriptor->hasObjectKey(vdc_container_key)) {
    // accessing one of the vdcs by numeric index
    return getDescriptorByNumericName(
      aPropMatch, aStartIndex, aDomain, aParentDescriptor,
      OKEY(vdc_key)
    );
  }
  // None of the containers within Device - let base class handle Device-Level properties
  return inherited::getDescriptorByName(aPropMatch, aStartIndex, aDomain, aMode, aParentDescriptor);
}


PropertyContainerPtr VdcHost::getContainer(const PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain)
{
  if (aPropertyDescriptor->isArrayContainer()) {
    // local container
    return PropertyContainerPtr(this); // handle myself
  }
  else if (aPropertyDescriptor->hasObjectKey(vdc_key)) {
    // - just iterate into map, we'll never have more than a few logical vdcs!
    int i = 0;
    for (VdcMap::iterator pos = vdcs.begin(); pos!=vdcs.end(); ++pos) {
      if (i==aPropertyDescriptor->fieldKey()) {
        // found
        return pos->second;
      }
      i++;
    }
  }
  // unknown here
  return NULL;
}


bool VdcHost::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(devicecontainer_key)) {
    if (aMode==access_read) {
      switch (aPropertyDescriptor->fieldKey()) {
        case valueSources_key:
          aPropValue->setType(apivalue_object); // make object (incoming object is NULL)
          createValueSourcesList(aPropValue);
          return true;
        case webui_url_key:
          aPropValue->setStringValue(webuiURLString());
          return true;
      }
    }
  }
  // not my field, let base class handle it
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


// MARK: ===== value sources

void VdcHost::createValueSourcesList(ApiValuePtr aApiObjectValue)
{
  // iterate through all devices and all of their sensors and inputs
  for (DsDeviceMap::iterator pos = dSDevices.begin(); pos!=dSDevices.end(); ++pos) {
    DevicePtr dev = pos->second;
    // Sensors
    for (BehaviourVector::iterator pos2 = dev->sensors.begin(); pos2!=dev->sensors.end(); ++pos2) {
      DsBehaviourPtr b = *pos2;
      ValueSource *vs = dynamic_cast<ValueSource *>(b.get());
      if (vs) {
        aApiObjectValue->add(string_format("%s_S%zu",dev->getDsUid().getString().c_str(), b->getIndex()), aApiObjectValue->newString(vs->getSourceName().c_str()));
      }
    }
    // Inputs
    for (BehaviourVector::iterator pos2 = dev->binaryInputs.begin(); pos2!=dev->binaryInputs.end(); ++pos2) {
      DsBehaviourPtr b = *pos2;
      ValueSource *vs = dynamic_cast<ValueSource *>(b.get());
      if (vs) {
        aApiObjectValue->add(string_format("%s_I%zu",dev->getDsUid().getString().c_str(), b->getIndex()), aApiObjectValue->newString(vs->getSourceName().c_str()));
      }
    }
  }
}


ValueSource *VdcHost::getValueSourceById(string aValueSourceID)
{
  ValueSource *valueSource = NULL;
  // value source ID is
  //  dSUID:Sx for sensors (x=sensor index)
  //  dSUID:Ix for inputs (x=input index)
  // - extract dSUID
  size_t i = aValueSourceID.find("_");
  if (i!=string::npos) {
    DsUid dsuid(aValueSourceID.substr(0,i));
    DsDeviceMap::iterator pos = dSDevices.find(dsuid);
    if (pos!=dSDevices.end()) {
      // is a device
      DevicePtr dev = pos->second;
      const char *p = aValueSourceID.c_str()+i+1;
      if (*p) {
        char ty = *p++;
        // scan index
        int idx = 0;
        if (sscanf(p, "%d", &idx)==1) {
          if (ty=='S' && idx<dev->sensors.size()) {
            // sensor
            valueSource = dynamic_cast<ValueSource *>(dev->sensors[idx].get());
          }
          else if (ty=='I' && idx<dev->binaryInputs.size()) {
            // input
            valueSource = dynamic_cast<ValueSource *>(dev->binaryInputs[idx].get());
          }
        }
      }
    }
  }
  return valueSource;
}



// MARK: ===== persistent vdc host level parameters

ErrorPtr VdcHost::loadAndFixDsUID()
{
  ErrorPtr err;
  // generate a default dSUID if no external one is given
  if (!externalDsuid) {
    // we don't have a fixed external dSUID to base everything on, so create a dSUID of our own:
    // single vDC per MAC-Adress scenario: generate UUIDv5 with name = macaddress
    // - calculate UUIDv5 based dSUID
    DsUid vdcNamespace(DSUID_VDC_NAMESPACE_UUID);
    dSUID.setNameInSpace(mac ? macAddressToString(mac,0) : "UnknownMACAddress", vdcNamespace);
  }
  DsUid originalDsUid = dSUID;
  // load the vdc host settings, which might override the default dSUID
  err = loadFromStore(entityType()); // is a singleton, identify by type
  if (!Error::isOK(err)) LOG(LOG_ERR,"Error loading settings for vdc host: %s", err->description().c_str());
  // check for settings from files
  loadSettingsFromFiles();
  // now check
  if (!externalDsuid) {
    if (storedDsuid) {
      // a dSUID was loaded from DB -> check if different from default
      if (!(originalDsUid==dSUID)) {
        // stored dSUID is not same as MAC derived -> we are running a migrated config
        LOG(LOG_WARNING,"Running a migrated configuration: dSUID collisions with original unit possible");
        LOG(LOG_WARNING,"- native vDC host dSUID of this instance would be %s", originalDsUid.getString().c_str());
        LOG(LOG_WARNING,"- if this is not a replacement unit -> factory reset recommended!");
      }
    }
    else {
      // no stored dSUID was found so far -> we need to save the current one
      markDirty();
      save();
    }
  }
  return ErrorPtr();
}



ErrorPtr VdcHost::save()
{
  ErrorPtr err;
  // save the vdc settings
  err = saveToStore(entityType(), false); // is a singleton, identify by type, single instance
  return ErrorPtr();
}


ErrorPtr VdcHost::forget()
{
  // delete the vdc settings
  deleteFromStore();
  return ErrorPtr();
}



void VdcHost::loadSettingsFromFiles()
{
  // try to open config file
  string fn = getPersistentDataDir();
  fn += "vdchostsettings.csv";
  // if vdc has already stored properties, only explicitly marked properties will be applied
  if (loadSettingsFromFile(fn.c_str(), rowid!=0)) markClean();
}


// MARK: ===== persistence implementation

// SQLIte3 table name to store these parameters to
const char *VdcHost::tableName()
{
  return "VdcHostSettings";
}


// data field definitions

static const size_t numFields = 2;

size_t VdcHost::numFieldDefs()
{
  return inheritedParams::numFieldDefs()+numFields;
}


const FieldDefinition *VdcHost::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numFields] = {
    { "vdcHostName", SQLITE_TEXT },
    { "vdcHostDSUID", SQLITE_TEXT },
  };
  if (aIndex<inheritedParams::numFieldDefs())
    return inheritedParams::getFieldDef(aIndex);
  aIndex -= inheritedParams::numFieldDefs();
  if (aIndex<numFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void VdcHost::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP)
{
  inheritedParams::loadFromRow(aRow, aIndex, aCommonFlagsP);
  // get the name
  setName(nonNullCStr(aRow->get<const char *>(aIndex++)));
  // get the vdc host dSUID
  if (!externalDsuid) {
    // only if dSUID is not set externally, we try to load it
    DsUid loadedDsUid;
    if (loadedDsUid.setAsString(nonNullCStr(aRow->get<const char *>(aIndex)))) {
      // dSUID string from DB is valid
      dSUID = loadedDsUid; // activate it as the vdc host dSUID
      storedDsuid = true; // we're using a stored dSUID now
    }
  }
  aIndex++;
}


// bind values to passed statement
void VdcHost::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags)
{
  inheritedParams::bindToStatement(aStatement, aIndex, aParentIdentifier, aCommonFlags);
  // bind the fields
  aStatement.bind(aIndex++, getAssignedName().c_str(), false); // c_str() ist not static in general -> do not rely on it (even if static here)
  if (externalDsuid)
    aStatement.bind(aIndex++); // do not save externally defined dSUIDs
  else
    aStatement.bind(aIndex++, dSUID.getString().c_str(), false); // not static, string is local obj
}



// MARK: ===== description

string VdcHost::description()
{
  string d = string_format("VdcHost with %lu vDCs:", vdcs.size());
  for (VdcMap::iterator pos = vdcs.begin(); pos!=vdcs.end(); ++pos) {
    d.append("\n");
    d.append(pos->second->description());
  }
  return d;
}



