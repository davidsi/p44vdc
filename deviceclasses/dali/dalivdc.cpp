//
//  Copyright (c) 2013-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "dalivdc.hpp"

#include "dalidevice.hpp"

#if ENABLE_DALI

using namespace p44;


DaliVdc::DaliVdc(int aInstanceNumber, VdcHost *aVdcHostP, int aTag) :
  Vdc(aInstanceNumber, aVdcHostP, aTag)
{
  daliComm = DaliCommPtr(new 	DaliComm(MainLoop::currentMainLoop()));
}



// vDC name
const char *DaliVdc::vdcClassIdentifier() const
{
  return "DALI_Bus_Container";
}


bool DaliVdc::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("vdc_dali", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


// MARK: ===== DB and initialisation

// Version history
//  1 : first version
//  2 : added groupNo (0..15) for DALI groups
#define DALI_SCHEMA_MIN_VERSION 1 // minimally supported version, anything older will be deleted
#define DALI_SCHEMA_VERSION 2 // current version

string DaliPersistence::dbSchemaUpgradeSQL(int aFromVersion, int &aToVersion)
{
  string sql;
  if (aFromVersion==0) {
    // create DB from scratch
		// - use standard globs table for schema version
    sql = inherited::dbSchemaUpgradeSQL(aFromVersion, aToVersion);
		// - create my tables
    sql.append(
      "CREATE TABLE compositeDevices ("
      " dimmerUID TEXT,"
      " dimmerType TEXT,"
      " collectionID INTEGER," // table-unique ID for this collection
      " groupNo INTEGER," // DALI group Number (0..15), valid for dimmerType "GRP" only
      " PRIMARY KEY (dimmerUID)"
      ");"
    );
    // reached final version in one step
    aToVersion = DALI_SCHEMA_VERSION;
  }
  else if (aFromVersion==1) {
    // V1->V2: groupNo added
    sql =
      "ALTER TABLE compositeDevices ADD groupNo INTEGER;";
    // reached version 2
    aToVersion = 2;
  }
  return sql;
}


void DaliVdc::initialize(StatusCB aCompletedCB, bool aFactoryReset)
{
	string databaseName = getPersistentDataDir();
	string_format_append(databaseName, "%s_%d.sqlite3", vdcClassIdentifier(), getInstanceNumber());
  ErrorPtr error = db.connectAndInitialize(databaseName.c_str(), DALI_SCHEMA_VERSION, DALI_SCHEMA_MIN_VERSION, aFactoryReset);
	aCompletedCB(error); // return status of DB init
}




// MARK: ===== collect devices


int DaliVdc::getRescanModes() const
{
  // normal and incremental make sense, no exhaustive mode
  return rescanmode_incremental+rescanmode_normal+rescanmode_exhaustive;
}


void DaliVdc::collectDevices(StatusCB aCompletedCB, bool aIncremental, bool aExhaustive, bool aClearSettings)
{
  if (!aIncremental) {
    removeDevices(aClearSettings);
    // clear the cache, we want fresh info from the devices!
    deviceInfoCache.clear();
  }
  // start collecting, allow quick scan when not exhaustively collecting (will still use full scan when bus collisions are detected)
  daliComm->daliFullBusScan(boost::bind(&DaliVdc::deviceListReceived, this, aCompletedCB, _1, _2, _3), !aExhaustive);
}


// recollect devices after grouping change without scanning bus again
void DaliVdc::recollectDevices(StatusCB aCompletedCB)
{
  removeDevices(false);
  // no scan used, just use the cache
  // - create a Dali bus device for every cached devInf
  DaliBusDeviceListPtr busDevices(new DaliBusDeviceList);
  for (DaliDeviceInfoMap::iterator pos = deviceInfoCache.begin(); pos!=deviceInfoCache.end(); ++pos) {
    // create bus device
    DaliBusDevicePtr busDevice(new DaliBusDevice(*this));
    busDevice->setDeviceInfo(pos->second); // use cached device info
    // - add bus device to list
    busDevices->push_back(busDevice);
  }
  // now start processing full device info for each device (no actual query will happen, it's already in the cache)
  queryNextDev(busDevices, busDevices->begin(), aCompletedCB, ErrorPtr());
}


void DaliVdc::deviceListReceived(StatusCB aCompletedCB, DaliComm::ShortAddressListPtr aDeviceListPtr, DaliComm::ShortAddressListPtr aUnreliableDeviceListPtr, ErrorPtr aError)
{
  // check if any devices
  if (aError || aDeviceListPtr->size()==0)
    return aCompletedCB(aError); // no devices to query, completed
  // create a Dali bus device for every detected device
  DaliBusDeviceListPtr busDevices(new DaliBusDeviceList);
  for (DaliComm::ShortAddressList::iterator pos = aDeviceListPtr->begin(); pos!=aDeviceListPtr->end(); ++pos) {
    // create simple device info containing only short address
    DaliDeviceInfoPtr info = DaliDeviceInfoPtr(new DaliDeviceInfo);
    info->shortAddress = *pos; // assign short address
    info->devInfStatus = DaliDeviceInfo::devinf_needsquery;
    deviceInfoCache[*pos] = info; // put it into the cache to represent the device
    // create bus device
    DaliBusDevicePtr busDevice(new DaliBusDevice(*this));
    busDevice->setDeviceInfo(info); // assign info to bus device
    // - add bus device to list
    busDevices->push_back(busDevice);
  }
  // now start collecting full device info for each device
  queryNextDev(busDevices, busDevices->begin(), aCompletedCB, ErrorPtr());
}


void DaliVdc::queryNextDev(DaliBusDeviceListPtr aBusDevices, DaliBusDeviceList::iterator aNextDev, StatusCB aCompletedCB, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    if (aNextDev != aBusDevices->end()) {
      DaliAddress addr = (*aNextDev)->deviceInfo->shortAddress;
      // check device info cache
      DaliDeviceInfoMap::iterator pos = deviceInfoCache.find(addr);
      if (pos!=deviceInfoCache.end() && pos->second->devInfStatus!=DaliDeviceInfo::devinf_needsquery) {
        // we already have real device info for this device, or know the device does not have any
        // -> have it processed (but via mainloop to avoid stacking up recursions here
        LOG(LOG_INFO, "Using cached device info for device at shortAddress %d", addr);
        MainLoop::currentMainLoop().executeOnce(boost::bind(&DaliVdc::deviceInfoValid, this, aBusDevices, aNextDev, aCompletedCB, pos->second));
        return;
      }
      else {
        // we need to fetch it from device
        daliComm->daliReadDeviceInfo(boost::bind(&DaliVdc::deviceInfoReceived, this, aBusDevices, aNextDev, aCompletedCB, _1, _2), addr);
        return;
      }
    }
    // all done successfully, complete bus info now available in aBusDevices
    // - look for dimmers that are to be addressed as a group
    DaliBusDeviceListPtr dimmerDevices = DaliBusDeviceListPtr(new DaliBusDeviceList());
    uint16_t groupsInUse = 0; // groups in use
    while (aBusDevices->size()>0) {
      // get first remaining
      DaliBusDevicePtr busDevice = aBusDevices->front();
      // duplicate dSUID check
      bool anyDuplicates = false;
      for (DaliBusDeviceList::iterator refpos = ++aBusDevices->begin(); refpos!=aBusDevices->end(); ++refpos) {
        if (busDevice->dSUID==(*refpos)->dSUID) {
          // duplicate dSUID, indicates DALI devices with invalid device info that slipped all heuristics
          LOG(LOG_ERR, "Bus devices #%d and #%d have same dSUID -> assuming invalid device info, reverting both to short address based dSUID", busDevice->deviceInfo->shortAddress, (*refpos)->deviceInfo->shortAddress);
          // - clear all device info except short address and revert to short address derived dSUID
          (*refpos)->clearDeviceInfo();
          anyDuplicates = true; // at least one found
        }
      }
      if (anyDuplicates) {
        // consider my own info invalid as well
        busDevice->clearDeviceInfo();
      }
      // check if this device is part of a DALI group
      sqlite3pp::query qry(db);
      string sql = string_format("SELECT groupNo FROM compositeDevices WHERE dimmerUID = '%s' AND dimmerType='GRP'", busDevice->dSUID.getString().c_str());
      if (qry.prepare(sql.c_str())==SQLITE_OK) {
        sqlite3pp::query::iterator i = qry.begin();
        if (i!=qry.end()) {
          // this is part of a DALI group
          int groupNo = i->get<int>(0);
          // - collect all with same group (= those that once were combined, in any order)
          sql = string_format("SELECT dimmerUID FROM compositeDevices WHERE groupNo = %d AND dimmerType='GRP'", groupNo);
          if (qry.prepare(sql.c_str())==SQLITE_OK) {
            // we know that we found at least one dimmer of this group on the bus, so we'll instantiate
            // the group (even if some dimmers might be missing)
            groupsInUse |= 1<<groupNo; // flag used
            DaliBusDeviceGroupPtr daliGroup = DaliBusDeviceGroupPtr(new DaliBusDeviceGroup(*this, groupNo));
            for (sqlite3pp::query::iterator j = qry.begin(); j != qry.end(); ++j) {
              DsUid dimmerUID(nonNullCStr(i->get<const char *>(0)));
              // see if we have this dimmer on the bus
              DaliBusDevicePtr dimmer;
              for (DaliBusDeviceList::iterator pos = aBusDevices->begin(); pos!=aBusDevices->end(); ++pos) {
                if ((*pos)->dSUID == dimmerUID) {
                  // found dimmer
                  dimmer = *pos;
                  // consumed, remove from the list
                  aBusDevices->erase(pos);
                  break;
                }
              }
              // process dimmer
              if (!dimmer) {
                // dimmer not found
                LOG(LOG_WARNING, "Missing DALI dimmer %s for DALI group %d", dimmerUID.getString().c_str(), groupNo);
                // insert dummy instead
                dimmer = DaliBusDevicePtr(new DaliBusDevice(*this));
                dimmer->isDummy = true; // disable bus access
                dimmer->dSUID = dimmerUID; // just set the dSUID we know from the DB
              }
              // add the dimmer (real or dummy)
              daliGroup->addDaliBusDevice(dimmer);
            } // for all needed dimmers
            // - derive dSUID for group
            daliGroup->deriveDsUid();
            // - add group to the list of single channel dimmer devices (groups and single devices)
            dimmerDevices->push_back(daliGroup);
          }
        } // part of group
        else {
          // definitely NOT part of group, single device dimmer
          dimmerDevices->push_back(busDevice);
          aBusDevices->remove(busDevice);
        }
      }
    }
    // initialize dimmer devices
    initializeNextDimmer(dimmerDevices, groupsInUse, dimmerDevices->begin(), aCompletedCB, ErrorPtr());
  }
  else {
    // collecting failed
    aCompletedCB(aError);
  }
}


void DaliVdc::initializeNextDimmer(DaliBusDeviceListPtr aDimmerDevices, uint16_t aGroupsInUse, DaliBusDeviceList::iterator aNextDimmer, StatusCB aCompletedCB, ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    LOG(LOG_ERR, "Error initializing dimmer: %s", aError->description().c_str());
  }
  if (aNextDimmer!=aDimmerDevices->end()) {
    // check next
    (*aNextDimmer)->initialize(boost::bind(&DaliVdc::initializeNextDimmer, this, aDimmerDevices, aGroupsInUse, ++aNextDimmer, aCompletedCB, _1), aGroupsInUse);
  }
  else {
    // done, now create dS devices from dimmers
    createDsDevices(aDimmerDevices, aCompletedCB);
  }
}




void DaliVdc::createDsDevices(DaliBusDeviceListPtr aDimmerDevices, StatusCB aCompletedCB)
{
  // - look up multi-channel composite devices
  //   If none of the devices are found on the bus, the entire composite device is considered missing
  //   If at least one device is found, non-found bus devices will be added as dummy bus devices
  DaliBusDeviceList singleDevices;
  while (aDimmerDevices->size()>0) {
    // get first remaining
    DaliBusDevicePtr busDevice = aDimmerDevices->front();
    // check if this device is part of a multi-channel composite device (but not a DALI group)
    sqlite3pp::query qry(db);
    string sql = string_format("SELECT collectionID FROM compositeDevices WHERE dimmerUID = '%s' AND dimmerType!='GRP'", busDevice->dSUID.getString().c_str());
    if (qry.prepare(sql.c_str())==SQLITE_OK) {
      sqlite3pp::query::iterator i = qry.begin();
      if (i!=qry.end()) {
        // this is part of a composite device
        int collectionID = i->get<int>(0);
        // - collect all with same collectionID (= those that once were combined, in any order)
        sql = string_format("SELECT dimmerType, dimmerUID FROM compositeDevices WHERE collectionID = %d", collectionID);
        if (qry.prepare(sql.c_str())==SQLITE_OK) {
          // we know that we found at least one dimmer of this composite on the bus, so we'll instantiate
          // a composite (even if some dimmers might be missing)
          DaliCompositeDevicePtr daliDevice = DaliCompositeDevicePtr(new DaliCompositeDevice(this));
          daliDevice->collectionID = collectionID; // remember from what collection this was created
          for (sqlite3pp::query::iterator j = qry.begin(); j != qry.end(); ++j) {
            string dimmerType = nonNullCStr(i->get<const char *>(0));
            DsUid dimmerUID(nonNullCStr(i->get<const char *>(1)));
            // see if we have this dimmer on the bus
            DaliBusDevicePtr dimmer;
            for (DaliBusDeviceList::iterator pos = aDimmerDevices->begin(); pos!=aDimmerDevices->end(); ++pos) {
              if ((*pos)->dSUID == dimmerUID) {
                // found dimmer on the bus, use it
                dimmer = *pos;
                // consumed, remove from the list
                aDimmerDevices->erase(pos);
                break;
              }
            }
            // process dimmer
            if (!dimmer) {
              // dimmer not found
              LOG(LOG_WARNING, "Missing DALI dimmer %s (type %s) for composite device", dimmerUID.getString().c_str(), dimmerType.c_str());
              // insert dummy instead
              dimmer = DaliBusDevicePtr(new DaliBusDevice(*this));
              dimmer->isDummy = true; // disable bus access
              dimmer->dSUID = dimmerUID; // just set the dSUID we know from the DB
            }
            // add the dimmer (real or dummy)
            daliDevice->addDimmer(dimmer, dimmerType);
          } // for all needed dimmers
          // - add it to our collection (if not already there)
          addDevice(daliDevice);
        }
      } // part of composite multichannel device
      else {
        // definitely NOT part of composite, put into single channel dimmer list
        singleDevices.push_back(busDevice);
        aDimmerDevices->remove(busDevice);
      }
    }
  }
  // remaining devices are single channel or DT8 dimmer devices
  for (DaliBusDeviceList::iterator pos = singleDevices.begin(); pos!=singleDevices.end(); ++pos) {
    DaliBusDevicePtr daliBusDevice = *pos;
    // simple single-dimmer device
    DaliSingleControllerDevicePtr daliSingleControllerDevice(new DaliSingleControllerDevice(this));
    // - set whiteDimmer (gives device info to calculate dSUID)
    daliSingleControllerDevice->daliController = daliBusDevice;
    // - add it to our collection (if not already there)
    addDevice(daliSingleControllerDevice);
  }
  // collecting complete
  aCompletedCB(ErrorPtr());
}


void DaliVdc::deviceInfoReceived(DaliBusDeviceListPtr aBusDevices, DaliBusDeviceList::iterator aNextDev, StatusCB aCompletedCB, DaliDeviceInfoPtr aDaliDeviceInfoPtr, ErrorPtr aError)
{
  bool missingData = aError && aError->isError(DaliCommError::domain(), DaliCommError::MissingData);
  bool badData =
    aError &&
    (aError->isError(DaliCommError::domain(), DaliCommError::BadChecksum) || aError->isError(DaliCommError::domain(), DaliCommError::BadDeviceInfo));
  if (!Error::isOK(aError) && !missingData && !badData) {
    // real fatal error, can't continue
    LOG(LOG_ERR, "Error reading device info: %s",aError->description().c_str());
    return aCompletedCB(aError);
  }
  // no error, or error but due to missing or bad data -> device exists
  if (missingData) { LOG(LOG_INFO, "Device at shortAddress %d does not have device info",aDaliDeviceInfoPtr->shortAddress); }
  if (badData) { LOG(LOG_INFO, "Device at shortAddress %d does not have valid device info",aDaliDeviceInfoPtr->shortAddress); }
  // update entry in the cache
  // Note: callback always gets a deviceInfo back, possibly with devinf_none if device does not have devInf at all (or garbage)
  //   So, assigning this here will make sure no entries with devinf_needsquery will remain.
  deviceInfoCache[aDaliDeviceInfoPtr->shortAddress] = aDaliDeviceInfoPtr;
  // use device info and continue
  deviceInfoValid(aBusDevices, aNextDev, aCompletedCB, aDaliDeviceInfoPtr);
}


void DaliVdc::deviceInfoValid(DaliBusDeviceListPtr aBusDevices, DaliBusDeviceList::iterator aNextDev, StatusCB aCompletedCB, DaliDeviceInfoPtr aDaliDeviceInfoPtr)
{
  // update device info entry in dali bus device
  (*aNextDev)->setDeviceInfo(aDaliDeviceInfoPtr);
  // query hardware features
  (*aNextDev)->queryFeatureSet(boost::bind(&DaliVdc::deviceFeaturesQueried, this, aBusDevices, aNextDev, aCompletedCB));
}


void DaliVdc::deviceFeaturesQueried(DaliBusDeviceListPtr aBusDevices, DaliBusDeviceList::iterator aNextDev, StatusCB aCompletedCB)
{
  // check next
  ++aNextDev;
  queryNextDev(aBusDevices, aNextDev, aCompletedCB, ErrorPtr());
}


// MARK: ===== DALI specific methods

ErrorPtr DaliVdc::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  if (aMethod=="x-p44-groupDevices") {
    // create a composite device out of existing single-channel ones
    respErr = groupDevices(aRequest, aParams);
  }
  else if (aMethod=="x-p44-daliScan") {
    // diagnostics: scan the entire DALI bus
    respErr = daliScan(aRequest, aParams);
  }
  else if (aMethod=="x-p44-daliCmd") {
    // diagnostics: direct DALI commands
    respErr = daliCmd(aRequest, aParams);
  }
  else {
    respErr = inherited::handleMethod(aRequest, aMethod, aParams);
  }
  return respErr;
}


// MARK: ===== DALI bus diagnostics


// scan bus, return status string

ErrorPtr DaliVdc::daliScan(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  StringPtr result(new string);
  daliScanNext(aRequest, 0, result);
  return ErrorPtr(); // no result yet, but later when scan is done
}


void DaliVdc::daliScanNext(VdcApiRequestPtr aRequest, DaliAddress aShortAddress, StringPtr aResult)
{
  if (aShortAddress<64) {
    // scan next
    daliComm->daliSendQuery(
      aShortAddress, DALICMD_QUERY_CONTROL_GEAR,
      boost::bind(&DaliVdc::handleDaliScanResult, this, aRequest, aShortAddress, aResult, _1, _2, _3)
    );
  }
  else {
    // done
    ApiValuePtr answer = aRequest->newApiValue();
    answer->setType(apivalue_object);
    answer->add("busState", answer->newString(*aResult));
    aRequest->sendResult(answer);
  }
}


void DaliVdc::handleDaliScanResult(VdcApiRequestPtr aRequest, DaliAddress aShortAddress, StringPtr aResult, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  char statusChar = '.'; // default to "nothing here"
  // plain FF without error is valid device
  if (Error::isOK(aError)) {
    if (!aNoOrTimeout) {
      // data received
      if (aResponse==0xFF)
        statusChar = '*'; // ok device
      else
        statusChar = 'C'; // possibly conflict
    }
  }
  else if (Error::isError(aError, DaliCommError::domain(), DaliCommError::DALIFrame)) {
    statusChar = 'C'; // possibly conflict
  }
  else {
    statusChar = 'E'; // real error
  }
  // add to result
  *aResult += statusChar;
  // check next
  daliScanNext(aRequest, ++aShortAddress, aResult);
}



// send single device, group or broadcast commands to bus

ErrorPtr DaliVdc::daliCmd(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  ErrorPtr respErr;
  string cmd;
  ApiValuePtr addr;
  respErr = checkParam(aParams, "addr", addr);
  if (Error::isOK(respErr)) {
    DaliAddress shortAddress = addr->int8Value();
    respErr = checkStringParam(aParams, "cmd", cmd);
    if (Error::isOK(respErr)) {
      // command
      if (cmd=="max") {
        daliComm->daliSendDirectPower(shortAddress, 0xFE);
      }
      else if (cmd=="min") {
        daliComm->daliSendDirectPower(shortAddress, 0x01);
      }
      else if (cmd=="off") {
        daliComm->daliSendDirectPower(shortAddress, 0x00);
      }
      else if (cmd=="pulse") {
        daliComm->daliSendDirectPower(shortAddress, 0xFE);
        daliComm->daliSendDirectPower(shortAddress, 0x01, NULL, 1200*MilliSecond);
      }
      else {
        respErr = WebError::webErr(500, "unknown cmd");
      }
      if (Error::isOK(respErr)) {
        // send ok
        aRequest->sendResult(ApiValuePtr());
      }
    }
  }
  // done
  return respErr;
}



// MARK: ===== composite device creation


ErrorPtr DaliVdc::groupDevices(VdcApiRequestPtr aRequest, ApiValuePtr aParams)
{
  // create a composite device out of existing single-channel ones
  ErrorPtr respErr;
  ApiValuePtr components;
  long long collectionID = -1;
  int groupNo = -1;
  DeviceVector groupedDevices;
  respErr = checkParam(aParams, "members", components);
  if (Error::isOK(respErr)) {
    if (components->isType(apivalue_object)) {
      components->resetKeyIteration();
      string dimmerType;
      ApiValuePtr o;
      while (components->nextKeyValue(dimmerType, o)) {
        DsUid memberUID;
        memberUID.setAsBinary(o->binaryValue());
        bool deviceFound = false;
        // search for this device
        for (DeviceVector::iterator pos = devices.begin(); pos!=devices.end(); ++pos) {
          // only non-composite DALI devices can be grouped at all
          DaliDevicePtr dev = boost::dynamic_pointer_cast<DaliDevice>(*pos);
          if (dev && dev->daliTechnicalType()!=dalidevice_composite && dev->getDsUid() == memberUID) {
            // found this device
            // - check type of grouping
            if (dimmerType[0]=='D') {
              // only not-yet grouped dimmers can be added to group
              if (dev->daliTechnicalType()==dalidevice_single) {
                deviceFound = true;
                // determine free group No
                if (groupNo<0) {
                  uint16_t groupMask=0;
                  sqlite3pp::query qry(db);
                  if (qry.prepare("SELECT DISTINCT groupNo FROM compositeDevices WHERE dimmerType='GRP'")==SQLITE_OK) {
                    for (sqlite3pp::query::iterator i = qry.begin(); i!=qry.end(); ++i) {
                      // this is a DALI group in use
                      groupMask |= (1<<(i->get<int>(0)));
                    }
                  }
                  for (groupNo=0; groupNo<16; ++groupNo) {
                    if ((groupMask & (1<<groupNo))==0) {
                      // group number is free - use it
                      break;
                    }
                  }
                  if (groupNo>=16) {
                    // no more unused DALI groups, cannot group at all
                    respErr = WebError::webErr(500, "16 groups already exist, cannot create additional group");
                    goto error;
                  }
                }
                // - create DB entry for DALI group member
                db.executef(
                  "INSERT OR REPLACE INTO compositeDevices (dimmerUID, dimmerType, groupNo) VALUES ('%s','GRP',%d)",
                  memberUID.getString().c_str(),
                  groupNo
                );
              }
            }
            else {
              deviceFound = true;
              // - create DB entry for member of composite device
              db.executef(
                "INSERT OR REPLACE INTO compositeDevices (dimmerUID, dimmerType, collectionID) VALUES ('%s','%s',%lld)",
                memberUID.getString().c_str(),
                dimmerType.c_str(),
                collectionID
              );
              if (collectionID<0) {
                // use rowid of just inserted item as collectionID
                collectionID = db.last_insert_rowid();
                // - update already inserted first record
                db.executef(
                  "UPDATE compositeDevices SET collectionID=%lld WHERE ROWID=%lld",
                  collectionID,
                  collectionID
                );
              }
            }
            // remember
            groupedDevices.push_back(dev);
            // done
            break;
          }
        }
        if (!deviceFound) {
          respErr = WebError::webErr(404, "some devices of the group could not be found");
          break;
        }
      }
    error:
      if (Error::isOK(respErr) && groupedDevices.size()>0) {
        // all components inserted into DB
        // - remove individual devices that will become part of a DALI group or composite device now
        for (DeviceVector::iterator pos = groupedDevices.begin(); pos!=groupedDevices.end(); ++pos) {
          (*pos)->hasVanished(false); // vanish, but keep settings
        }
        // - re-collect devices to find groups and composites now, but only after a second, starting from main loop, not from here
        StatusCB cb = boost::bind(&DaliVdc::groupCollected, this, aRequest);
        MainLoop::currentMainLoop().executeOnce(boost::bind(&DaliVdc::recollectDevices, this, cb), 1*Second);
      }
    }
  }
  return respErr;
}


ErrorPtr DaliVdc::ungroupDevice(DaliDevicePtr aDevice, VdcApiRequestPtr aRequest)
{
  ErrorPtr respErr;
  if (aDevice->daliTechnicalType()==dalidevice_composite) {
    // composite device, delete grouping
    DaliCompositeDevicePtr dev = boost::dynamic_pointer_cast<DaliCompositeDevice>(aDevice);
    if (dev) {
      db.executef(
        "DELETE FROM compositeDevices WHERE dimmerType!='GRP' AND collectionID=%ld",
        (long)dev->collectionID
      );
    }
  }
  else if (aDevice->daliTechnicalType()==dalidevice_group) {
    // composite device, delete grouping
    DaliSingleControllerDevicePtr dev = boost::dynamic_pointer_cast<DaliSingleControllerDevice>(aDevice);
    if (dev) {
      int groupNo = dev->daliController->deviceInfo->shortAddress & DaliGroupMask;
      db.executef(
        "DELETE FROM compositeDevices WHERE dimmerType='GRP' AND groupNo=%d",
        groupNo
      );
    }
  }
  else {
    // error, nothing done, just return error immediately
    return WebError::webErr(500, "device is not grouped, cannot be ungrouped");
  }
  // ungrouped a device
  // - delete the previously grouped dS device
  aDevice->hasVanished(true); // delete parameters
  // - re-collect devices to find groups and composites now, but only after a second, starting from main loop, not from here
  StatusCB cb = boost::bind(&DaliVdc::groupCollected, this, aRequest);
  MainLoop::currentMainLoop().executeOnce(boost::bind(&DaliVdc::recollectDevices, this, cb), 1*Second);
  return respErr;
}




void DaliVdc::groupCollected(VdcApiRequestPtr aRequest)
{
  // devices re-collected, return ok (empty response)
  aRequest->sendResult(ApiValuePtr());
}



// MARK: ===== Self test

void DaliVdc::selfTest(StatusCB aCompletedCB)
{
  // do bus short address scan
  daliComm->daliBusScan(boost::bind(&DaliVdc::testScanDone, this, aCompletedCB, _1, _2, _3));
}


void DaliVdc::testScanDone(StatusCB aCompletedCB, DaliComm::ShortAddressListPtr aShortAddressListPtr, DaliComm::ShortAddressListPtr aUnreliableShortAddressListPtr, ErrorPtr aError)
{
  if (Error::isOK(aError) && aShortAddressListPtr && aShortAddressListPtr->size()>0) {
    // found at least one device, do a R/W test using the DTR
    DaliAddress testAddr = aShortAddressListPtr->front();
    LOG(LOG_NOTICE, "- DALI self test: switch all lights on, then do R/W tests with DTR of device short address %d",testAddr);
    daliComm->daliSendDirectPower(DaliBroadcast, 0, NULL); // off
    daliComm->daliSendDirectPower(DaliBroadcast, 254, NULL, 2*Second); // max
    testRW(aCompletedCB, testAddr, 0x55); // use first found device
  }
  else {
    // return error
    if (Error::isOK(aError)) aError = ErrorPtr(new DaliCommError(DaliCommError::DeviceSearch)); // no devices is also an error
    aCompletedCB(aError);
  }
}


void DaliVdc::testRW(StatusCB aCompletedCB, DaliAddress aShortAddr, uint8_t aTestByte)
{
  // set DTR
  daliComm->daliSend(DALICMD_SET_DTR, aTestByte);
  // query DTR again, with 200mS delay
  daliComm->daliSendQuery(aShortAddr, DALICMD_QUERY_CONTENT_DTR, boost::bind(&DaliVdc::testRWResponse, this, aCompletedCB, aShortAddr, aTestByte, _1, _2, _3), 200*MilliSecond);
}


void DaliVdc::testRWResponse(StatusCB aCompletedCB, DaliAddress aShortAddr, uint8_t aTestByte, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && !aNoOrTimeout && aResponse==aTestByte) {
    LOG(LOG_NOTICE, "  - sent 0x%02X, received 0x%02X, noOrTimeout=%d",aTestByte, aResponse, aNoOrTimeout);
    // successfully read back same value from DTR as sent before
    // - check if more tests
    switch (aTestByte) {
      case 0x55: aTestByte = 0xAA; break; // next test: inverse
      case 0xAA: aTestByte = 0x00; break; // next test: all 0
      case 0x00: aTestByte = 0xFF; break; // next test: all 1
      case 0xFF: aTestByte = 0xF0; break; // next test: half / half
      case 0xF0: aTestByte = 0x0F; break; // next test: half / half inverse
      default:
        // all tests done
        aCompletedCB(aError);
        // turn off lights
        daliComm->daliSendDirectPower(DaliBroadcast, 0); // off
        return;
    }
    // launch next test
    testRW(aCompletedCB, aShortAddr, aTestByte);
  }
  else {
    // not ok
    if (Error::isOK(aError) && aNoOrTimeout) aError = ErrorPtr(new DaliCommError(DaliCommError::MissingData));
    // report
    LOG(LOG_ERR, "DALI self test error: sent 0x%02X, error: %s",aTestByte, aError->description().c_str());
    aCompletedCB(aError);
  }
}

#endif // ENABLE_DALI

