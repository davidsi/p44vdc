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


// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0


#include "dalidevice.hpp"

#if ENABLE_DALI

#include "dalivdc.hpp"
#include "colorlightbehaviour.hpp"

#include <math.h>


using namespace p44;


// MARK: ===== DaliBusDevice

DaliBusDevice::DaliBusDevice(DaliVdc &aDaliVdc) :
  daliVdc(aDaliVdc),
  dimRepeaterTicket(0),
  isDummy(false),
  isPresent(false),
  lampFailure(false),
  currentTransitionTime(Infinite), // invalid
  currentDimPerMS(0), // none
  currentFadeRate(0xFF), currentFadeTime(0xFF) // unlikely values
{
  // make sure we always have at least a dummy device info
  deviceInfo = DaliDeviceInfoPtr(new DaliDeviceInfo);
}



void DaliBusDevice::setDeviceInfo(DaliDeviceInfoPtr aDeviceInfo)
{
  // store the info record
  if (!aDeviceInfo)
    aDeviceInfo = DaliDeviceInfoPtr(new DaliDeviceInfo); // always have one, even if it's only a dummy!
  deviceInfo = aDeviceInfo; // assign
  deriveDsUid(); // derive dSUID from it
}


void DaliBusDevice::clearDeviceInfo()
{
  deviceInfo->clear();
  deriveDsUid();
}


void DaliBusDevice::deriveDsUid()
{
  if (isDummy) return;
  // vDC implementation specific UUID:
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string s;
  #if OLD_BUGGY_CHKSUM_COMPATIBLE
  if (deviceInfo->devInfStatus==DaliDeviceInfo::devinf_maybe) {
    // assume we can use devInf to derive dSUID from
    deviceInfo->devInfStatus = DaliDeviceInfo::devinf_solid;
    // but only actually use it if there is no device entry for the shortaddress-based dSUID with a non-zero name
    // (as this means the device has been already actively used/configured with the shortaddr-dSUID)
    // - calculate the short address based dSUID
    s = daliVdc.vdcInstanceIdentifier();
    string_format_append(s, "::%d", deviceInfo->shortAddress);
    DsUid shortAddrBasedDsUid;
    shortAddrBasedDsUid.setNameInSpace(s, vdcNamespace);
    // - check for named device in database consisting of this dimmer with shortaddr based dSUID
    //   Note that only single dimmer device are checked for, composite devices will not have this compatibility mechanism
    sqlite3pp::query qry(daliVdc.getVdcHost().getDsParamStore());
    // Note: this is a bit ugly, as it has the device settings table name hard coded
    string sql = string_format("SELECT deviceName FROM DeviceSettings WHERE parentID='%s'", shortAddrBasedDsUid.getString().c_str());
    if (qry.prepare(sql.c_str())==SQLITE_OK) {
      sqlite3pp::query::iterator i = qry.begin();
      if (i!=qry.end()) {
        // the length of the name
        string n = nonNullCStr(i->get<const char *>(0));
        if (n.length()>0) {
          // shortAddr based device has already been named. So keep that, and don't generate a devInf based dSUID
          deviceInfo->devInfStatus = DaliDeviceInfo::devinf_notForID;
          LOG(LOG_WARNING, "DaliBusDevice shortaddr %d kept with shortaddr-based dSUID because it is already named: '%s'", deviceInfo->shortAddress, n.c_str());
        }
      }
    }
  }
  #endif // OLD_BUGGY_CHKSUM_COMPATIBLE
  if (deviceInfo->devInfStatus==DaliDeviceInfo::devinf_solid) {
    // uniquely identified by GTIN+Serial, but unknown partition value:
    // - Proceed according to dS rule 2:
    //   "vDC can determine GTIN and serial number of Device → combine GTIN and
    //    serial number to form a GS1-128 with Application Identifier 21:
    //    "(01)<GTIN>(21)<serial number>" and use the resulting string to
    //    generate a UUIDv5 in the GS1-128 name space"
    s = string_format("(01)%llu(21)%llu", deviceInfo->gtin, deviceInfo->serialNo);
  }
  else {
    // not uniquely identified by devInf (or shortaddr based version already in use):
    // - generate id in vDC namespace
    //   UUIDv5 with name = classcontainerinstanceid::daliShortAddrDecimal
    s = daliVdc.vdcInstanceIdentifier();
    string_format_append(s, "::%d", deviceInfo->shortAddress);
  }
  dSUID.setNameInSpace(s, vdcNamespace);
}



void DaliBusDevice::getGroupMemberShip(DaliGroupsCB aDaliGroupsCB, DaliAddress aShortAddress)
{
  daliVdc.daliComm->daliSendQuery(
    aShortAddress,
    DALICMD_QUERY_GROUPS_0_TO_7,
    boost::bind(&DaliBusDevice::queryGroup0to7Response, this, aDaliGroupsCB, aShortAddress, _1, _2, _3)
  );
}


void DaliBusDevice::queryGroup0to7Response(DaliGroupsCB aDaliGroupsCB, DaliAddress aShortAddress, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  uint16_t groupBitMask = 0; // no groups yet
  if (Error::isOK(aError) && !aNoOrTimeout) {
    groupBitMask = aResponse;
  }
  // anyway, query other half
  daliVdc.daliComm->daliSendQuery(
    aShortAddress,
    DALICMD_QUERY_GROUPS_8_TO_15,
    boost::bind(&DaliBusDevice::queryGroup8to15Response, this, aDaliGroupsCB, aShortAddress, groupBitMask, _1, _2, _3)
  );
}


void DaliBusDevice::queryGroup8to15Response(DaliGroupsCB aDaliGroupsCB, DaliAddress aShortAddress, uint16_t aGroupBitMask, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  // group 8..15 membership result
  if (Error::isOK(aError) && !aNoOrTimeout) {
    aGroupBitMask |= ((uint16_t)aResponse)<<8;
  }
  if (aDaliGroupsCB) aDaliGroupsCB(aGroupBitMask, aError);
}



void DaliBusDevice::registerDeviceType(uint8_t aDeviceType)
{
  LOG(LOG_INFO, "DALI bus device with shortaddr %d supports device type %d", deviceInfo->shortAddress, aDeviceType);
  switch (aDeviceType) {
    case 6: // DALI DT6 is LED support
      supportsLED = true;
      break;
    case 8: // DALI DT8 is color support
      supportsColor = true;
      break;
  }
}



void DaliBusDevice::initialize(StatusCB aCompletedCB, uint16_t aUsedGroupsMask)
{
  // query device type(s) - i.e. availability of extended command sets
  daliVdc.daliComm->daliSendQuery(
    deviceInfo->shortAddress,
    DALICMD_QUERY_DEVICE_TYPE,
    boost::bind(&DaliBusDevice::deviceTypeResponse, this, aCompletedCB, aUsedGroupsMask, _1, _2, _3)
  );
}


void DaliBusDevice::deviceTypeResponse(StatusCB aCompletedCB, uint16_t aUsedGroupsMask, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  // device type query response.
  if (Error::isOK(aError) && !aNoOrTimeout) {
    // special case is 0xFF, which means device supports multiple types
    if (aResponse==0xFF) {
      // need to query all possible types
      probeDeviceType(aCompletedCB, aUsedGroupsMask, 0); // start with 0
      return;
    }
    // check device type
    registerDeviceType(aResponse);
  }
  // done with device type, check groups now
  checkGroupMembership(aCompletedCB, aUsedGroupsMask);
}


void DaliBusDevice::probeDeviceType(StatusCB aCompletedCB, uint16_t aUsedGroupsMask, uint8_t aNextDT)
{
  if (aNextDT>10) {
    // all device types checked
    // done with device type, check groups now
    checkGroupMembership(aCompletedCB, aUsedGroupsMask);
    return;
  }
  // query next device type
  // - extended command
  daliVdc.daliComm->daliEnableDeviceType(aNextDT);
  daliVdc.daliComm->daliSendQuery(
    deviceInfo->shortAddress,
    DALICMD_QUERY_EXTENDED_VERSION,
    boost::bind(&DaliBusDevice::probeDeviceTypeResponse, this, aCompletedCB, aUsedGroupsMask, aNextDT, _1, _2, _3)
  );
}


void DaliBusDevice::probeDeviceTypeResponse(StatusCB aCompletedCB, uint16_t aUsedGroupsMask, uint8_t aNextDT, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  // extended version type query response.
  if (Error::isOK(aError) && !aNoOrTimeout) {
    // extended version response
    // check device type
    registerDeviceType(aNextDT);
  }
  // query next device type
  aNextDT++;
  probeDeviceType(aCompletedCB, aUsedGroupsMask, aNextDT);
}


void DaliBusDevice::checkGroupMembership(StatusCB aCompletedCB, uint16_t aUsedGroupsMask)
{
  // make sure device is in none of the used groups
  if (aUsedGroupsMask==0) {
    // no groups in use at all, just return
    if (aCompletedCB) aCompletedCB(ErrorPtr());
    return;
  }
  // need to query current groups
  getGroupMemberShip(boost::bind(&DaliBusDevice::groupMembershipResponse, this, aCompletedCB, aUsedGroupsMask, deviceInfo->shortAddress, _1, _2), deviceInfo->shortAddress);
}


void DaliBusDevice::groupMembershipResponse(StatusCB aCompletedCB, uint16_t aUsedGroupsMask, DaliAddress aShortAddress, uint16_t aGroups, ErrorPtr aError)
{
  // remove groups that are in use on the bus
  if (Error::isOK(aError)) {
    for (int g=0; g<16; ++g) {
      if (aUsedGroupsMask & aGroups & (1<<g)) {
        // single device is member of a group in use -> remove it
        LOG(LOG_INFO, "- removing single DALI bus device with shortaddr %d from group %d", aShortAddress, g);
        daliVdc.daliComm->daliSendConfigCommand(aShortAddress, DALICMD_REMOVE_FROM_GROUP|g);
      }
    }
  }
  if (aCompletedCB) aCompletedCB(aError);
}



void DaliBusDevice::updateParams(StatusCB aCompletedCB)
{
  if (isDummy) aCompletedCB(ErrorPtr());
  // query actual arc power level
  daliVdc.daliComm->daliSendQuery(
    addressForQuery(),
    DALICMD_QUERY_ACTUAL_LEVEL,
    boost::bind(&DaliBusDevice::queryActualLevelResponse,this, aCompletedCB, _1, _2, _3)
  );
}


void DaliBusDevice::queryActualLevelResponse(StatusCB aCompletedCB, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  currentBrightness = 0; // default to 0
  if (Error::isOK(aError) && !aNoOrTimeout) {
    isPresent = true; // answering a query means presence
    // this is my current arc power, save it as brightness for dS system side queries
    currentBrightness = arcpowerToBrightness(aResponse);
    LOG(LOG_DEBUG, "DaliBusDevice: retrieved current dimming level: arc power = %d, brightness = %0.1f", aResponse, currentBrightness);
  }
  // next: query the minimum dimming level
  daliVdc.daliComm->daliSendQuery(
    addressForQuery(),
    DALICMD_QUERY_MIN_LEVEL,
    boost::bind(&DaliBusDevice::queryMinLevelResponse,this, aCompletedCB, _1, _2, _3)
  );
}


void DaliBusDevice::queryMinLevelResponse(StatusCB aCompletedCB, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  minBrightness = 0; // default to 0
  if (Error::isOK(aError) && !aNoOrTimeout) {
    isPresent = true; // answering a query means presence
    // this is my current arc power, save it as brightness for dS system side queries
    minBrightness = arcpowerToBrightness(aResponse);
    LOG(LOG_DEBUG, "DaliBusDevice: retrieved minimum dimming level: arc power = %d, brightness = %0.1f", aResponse, minBrightness);
  }
  // done updating parameters
  aCompletedCB(aError);
}


void DaliBusDevice::updateStatus(StatusCB aCompletedCB)
{
  if (isDummy) {
    if (aCompletedCB) aCompletedCB(ErrorPtr());
    return;
  }
  // query the device for status
  daliVdc.daliComm->daliSendQuery(
    addressForQuery(),
    DALICMD_QUERY_STATUS,
    boost::bind(&DaliBusDevice::queryStatusResponse, this, aCompletedCB, _1, _2, _3)
  );
}


void DaliBusDevice::queryStatusResponse(StatusCB aCompletedCB, bool aNoOrTimeout, uint8_t aResponse, ErrorPtr aError)
{
  if (Error::isOK(aError) && !aNoOrTimeout) {
    isPresent = true; // answering a query means presence
    // check status bits
    // - bit1 = lamp failure
    lampFailure = aResponse & 0x02;
  }
  else {
    isPresent = false; // no correct status -> not present
  }
  // done updating status
  if (aCompletedCB) aCompletedCB(aError);
}



void DaliBusDevice::setTransitionTime(MLMicroSeconds aTransitionTime)
{
  if (isDummy) return;
  if (currentTransitionTime==Infinite || currentTransitionTime!=aTransitionTime) {
    uint8_t tr = 0; // default to 0
    if (aTransitionTime>0) {
      // Fade time: T = 0.5 * SQRT(2^X) [seconds] -> x = ln2((T/0.5)^2) : T=0.25 [sec] -> x = -2, T=10 -> 8.64
      double h = (((double)aTransitionTime/Second)/0.5);
      h = h*h;
      h = log(h)/log(2);
      tr = h>1 ? (uint8_t)h : 1;
      LOG(LOG_DEBUG, "DaliDevice: new transition time = %.1f mS, calculated FADE_TIME setting = %f (rounded %d)", (double)aTransitionTime/MilliSecond, h, (int)tr);
    }
    if (tr!=currentFadeTime || currentTransitionTime==Infinite) {
      LOG(LOG_DEBUG, "DaliDevice: setting DALI FADE_TIME to %d", (int)tr);
      daliVdc.daliComm->daliSendDtrAndConfigCommand(deviceInfo->shortAddress, DALICMD_STORE_DTR_AS_FADE_TIME, tr);
      currentFadeTime = tr;
    }
    currentTransitionTime = aTransitionTime;
  }
}


void DaliBusDevice::setBrightness(Brightness aBrightness)
{
  if (isDummy) return;
  if (currentBrightness!=aBrightness) {
    currentBrightness = aBrightness;
    uint8_t power = brightnessToArcpower(aBrightness);
    LOG(LOG_INFO, "Dali dimmer at shortaddr=%d: setting new brightness = %0.2f, arc power = %d", (int)deviceInfo->shortAddress, aBrightness, (int)power);
    daliVdc.daliComm->daliSendDirectPower(deviceInfo->shortAddress, power);
  }
}


void DaliBusDevice::setDefaultBrightness(Brightness aBrightness)
{
  if (isDummy) return;
  if (aBrightness<0) aBrightness = currentBrightness; // use current brightness
  uint8_t power = brightnessToArcpower(aBrightness);
  LOG(LOG_INFO, "Dali dimmer at shortaddr=%d: setting default/failure brightness = %0.2f, arc power = %d", (int)deviceInfo->shortAddress, aBrightness, (int)power);
  daliVdc.daliComm->daliSendDtrAndConfigCommand(deviceInfo->shortAddress, DALICMD_STORE_DTR_AS_POWER_ON_LEVEL, power);
  daliVdc.daliComm->daliSendDtrAndConfigCommand(deviceInfo->shortAddress, DALICMD_STORE_DTR_AS_FAILURE_LEVEL, power);
}




uint8_t DaliBusDevice::brightnessToArcpower(Brightness aBrightness)
{
  double intensity = (double)aBrightness/100;
  if (intensity<0) intensity = 0;
  if (intensity>1) intensity = 1;
  return log10((intensity*9)+1)*254; // 0..254, 255 is MASK and is reserved to stop fading
}



Brightness DaliBusDevice::arcpowerToBrightness(int aArcpower)
{
  double intensity = (pow(10, aArcpower/254.0)-1)/9;
  return intensity*100;
}


// optimized DALI dimming implementation
void DaliBusDevice::dim(VdcDimMode aDimMode, double aDimPerMS)
{
  if (isDummy) return;
  MainLoop::currentMainLoop().cancelExecutionTicket(dimRepeaterTicket); // stop any previous dimming activity
  // Use DALI UP/DOWN dimming commands
  if (aDimMode==dimmode_stop) {
    // stop dimming - send MASK
    daliVdc.daliComm->daliSendDirectPower(deviceInfo->shortAddress, DALIVALUE_MASK);
  }
  else {
    // start dimming
    // - configure new fade rate if current does not match
    if (aDimPerMS!=currentDimPerMS) {
      currentDimPerMS = aDimPerMS;
      //   Fade rate: R = 506/SQRT(2^X) [steps/second] -> x = ln2((506/R)^2) : R=44 [steps/sec] -> x = 7
      double h = 506.0/(currentDimPerMS*1000);
      h = log(h*h)/log(2);
      uint8_t fr = h>0 ? (uint8_t)h : 0;
      LOG(LOG_DEBUG, "DaliDevice: new dimming rate = %f Steps/second, calculated FADE_RATE setting = %f (rounded %d)", currentDimPerMS*1000, h, fr);
      if (fr!=currentFadeRate) {
        LOG(LOG_DEBUG, "DaliDevice: setting DALI FADE_RATE to %d", fr);
        daliVdc.daliComm->daliSendDtrAndConfigCommand(deviceInfo->shortAddress, DALICMD_STORE_DTR_AS_FADE_RATE, fr);
        currentFadeRate = fr;
      }
    }
    // - use repeated UP and DOWN commands
    dimRepeater(deviceInfo->shortAddress, aDimMode==dimmode_up ? DALICMD_UP : DALICMD_DOWN, MainLoop::now());
  }
}


void DaliBusDevice::dimRepeater(DaliAddress aDaliAddress, uint8_t aCommand, MLMicroSeconds aCycleStartTime)
{
  daliVdc.daliComm->daliSendCommand(aDaliAddress, aCommand);
  // schedule next command
  // - DALI UP and DOWN run 200mS, but can be repeated earlier, so we use 150mS to make sure we don't have hickups
  //   Note: DALI bus speed limits commands to 120Bytes/sec max, i.e. about 20 per 150mS, i.e. max 10 lamps dimming
  dimRepeaterTicket = MainLoop::currentMainLoop().executeOnceAt(boost::bind(&DaliBusDevice::dimRepeater, this, aDaliAddress, aCommand, _1), aCycleStartTime+200*MilliSecond);
}



// MARK: ===== DaliBusDeviceGroup (multiple DALI devices, addressed as a group, forming single channel dimmer)


DaliBusDeviceGroup::DaliBusDeviceGroup(DaliVdc &aDaliVdc, uint8_t aGroupNo) :
  inherited(aDaliVdc),
  groupMaster(DaliBroadcast)
{
  mixID.erase(); // no members yet
  // set the group address to use
  deviceInfo->shortAddress = aGroupNo|DaliGroup;
}


void DaliBusDeviceGroup::addDaliBusDevice(DaliBusDevicePtr aDaliBusDevice)
{
  // add the ID to the mix
  LOG(LOG_NOTICE, "- DALI bus device with shortaddr %d is grouped in DALI group %d", aDaliBusDevice->deviceInfo->shortAddress, deviceInfo->shortAddress & DaliGroupMask);
  aDaliBusDevice->dSUID.xorDsUidIntoMix(mixID);
  // if this is the first valid device, use it as master
  if (groupMaster==DaliBroadcast && !aDaliBusDevice->isDummy) {
    // this is the master device
    LOG(LOG_INFO, "- DALI bus device with shortaddr %d is master of the group (queried for brightness, mindim)", aDaliBusDevice->deviceInfo->shortAddress);
    groupMaster = aDaliBusDevice->deviceInfo->shortAddress;
  }
  // add member
  groupMembers.push_back(aDaliBusDevice->deviceInfo->shortAddress);
}


void DaliBusDeviceGroup::initialize(StatusCB aCompletedCB, uint16_t aUsedGroupsMask)
{
  DaliComm::ShortAddressList::iterator pos = groupMembers.begin();
  initNextGroupMember(aCompletedCB, pos);
}


void DaliBusDeviceGroup::initNextGroupMember(StatusCB aCompletedCB, DaliComm::ShortAddressList::iterator aNextMember)
{
  if (aNextMember!=groupMembers.end()) {
    // another member, query group membership, then adjust if needed
    // need to query current groups
    getGroupMemberShip(
      boost::bind(&DaliBusDeviceGroup::groupMembershipResponse, this, aCompletedCB, aNextMember, _1, _2),
      *aNextMember
    );
  }
  else {
    // all done
    if (aCompletedCB) aCompletedCB(ErrorPtr());
  }
}

void DaliBusDeviceGroup::groupMembershipResponse(StatusCB aCompletedCB, DaliComm::ShortAddressList::iterator aNextMember, uint16_t aGroups, ErrorPtr aError)
{
  uint8_t groupNo = deviceInfo->shortAddress & DaliGroupMask;
  // make sure device is member of the group
  if ((aGroups & (1<<groupNo))==0) {
    // is not yet member of this group -> add it
    LOG(LOG_INFO, "- making DALI bus device with shortaddr %d member of group %d", *aNextMember, groupNo);
    daliVdc.daliComm->daliSendConfigCommand(*aNextMember, DALICMD_ADD_TO_GROUP|groupNo);
  }
  // remove from all other groups
  aGroups &= ~(1<<groupNo); // do not remove again from target group
  for (groupNo=0; groupNo<16; groupNo++) {
    if (aGroups & (1<<groupNo)) {
      // device is member of a group it shouldn't be in -> remove it
      LOG(LOG_INFO, "- removing DALI bus device with shortaddr %d from group %d", *aNextMember, groupNo);
      daliVdc.daliComm->daliSendConfigCommand(*aNextMember, DALICMD_REMOVE_FROM_GROUP|groupNo);
    }
  }
  // done adding this member to group
  // - check if more to process
  ++aNextMember;
  initNextGroupMember(aCompletedCB, aNextMember);
}




/// derive the dSUID from collected device info
void DaliBusDeviceGroup::deriveDsUid()
{
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  // use xored IDs of group members as base for creating UUIDv5 in vdcNamespace
  dSUID.setNameInSpace("daligroup:"+mixID, vdcNamespace);
}


// MARK: ===== DaliDevice (base class)


DaliDevice::DaliDevice(DaliVdc *aVdcP) :
  Device((Vdc *)aVdcP)
{
  // DALI devices are always light (in this implementation, at least)
  setColorClass(class_yellow_light);
}


DaliVdc &DaliDevice::daliVdc()
{
  return *(static_cast<DaliVdc *>(vdcP));
}


ErrorPtr DaliDevice::handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams)
{
  if (aMethod=="x-p44-ungroupDevice") {
    // Remove this device from the installation, forget the settings
    return daliVdc().ungroupDevice(this, aRequest);
  }
  if (aMethod=="x-p44-saveAsDefault") {
    // save the current brightness as default DALI brightness (at powerup or failure)
    saveAsDefaultBrightness();
    // confirm done
    aRequest->sendResult(ApiValuePtr());
    return ErrorPtr();
  }
  else {
    return inherited::handleMethod(aRequest, aMethod, aParams);
  }
}



// MARK: ===== DaliDimmerDevice (single channel)


DaliDimmerDevice::DaliDimmerDevice(DaliVdc *aVdcP) :
  DaliDevice(aVdcP)
{
}


void DaliDimmerDevice::willBeAdded()
{
  // Note: setting up behaviours late, because we want the brightness dimmer already assigned for the hardware name
  // set up dS behaviour for simple single DALI channel dimmer
  // - use light settings, which include a scene table
  installSettings(DeviceSettingsPtr(new LightDeviceSettings(*this)));
  // - set the behaviour
  LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
  l->setHardwareOutputConfig(outputFunction_dimmer, outputmode_gradual, usage_undefined, true, 160); // DALI ballasts are always dimmable, // TODO: %%% somewhat arbitrary 2*80W max wattage
  if (daliTechnicalType()==dalidevice_group)
    l->setHardwareName(string_format("DALI dimmer group # %d",brightnessDimmer->deviceInfo->shortAddress & DaliGroupMask));
  else
    l->setHardwareName(string_format("DALI dimmer @ %d",brightnessDimmer->deviceInfo->shortAddress));
  addBehaviour(l);
  // - derive the DsUid
  deriveDsUid();
}


bool DaliDimmerDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("dali_dimmer", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string DaliDimmerDevice::getExtraInfo()
{
  if (daliTechnicalType()==dalidevice_group) {
    return string_format(
      "DALI group address: %d",
      brightnessDimmer->deviceInfo->shortAddress & DaliGroupMask
    );
  }
  else {
    return string_format(
      "DALI short address: %d",
      brightnessDimmer->deviceInfo->shortAddress
    );
  }
}


void DaliDimmerDevice::initializeDevice(StatusCB aCompletedCB, bool aFactoryReset)
{
  // - sync cached channel values from actual device
  brightnessDimmer->updateParams(boost::bind(&DaliDimmerDevice::brightnessDimmerSynced, this, aCompletedCB, aFactoryReset, _1));
}


void DaliDimmerDevice::brightnessDimmerSynced(StatusCB aCompletedCB, bool aFactoryReset, ErrorPtr aError)
{
  if (Error::isOK(aError)) {
    // save brightness now
    output->getChannelByIndex(0)->syncChannelValue(brightnessDimmer->currentBrightness);
    // initialize the light behaviour with the minimal dimming level
    LightBehaviourPtr l = boost::static_pointer_cast<LightBehaviour>(output);
    l->initMinBrightness(brightnessDimmer->minBrightness);
  }
  else {
    LOG(LOG_ERR, "DaliDevice: error getting state/params from dimmer: %s", aError->description().c_str());
  }
  // continue with initialisation in superclasses
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}





void DaliDimmerDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  // query the device
  brightnessDimmer->updateStatus(boost::bind(&DaliDimmerDevice::checkPresenceResponse, this, aPresenceResultHandler));
}


void DaliDimmerDevice::checkPresenceResponse(PresenceCB aPresenceResultHandler)
{
  // present if a proper YES (without collision) received
  aPresenceResultHandler(brightnessDimmer->isPresent);
}



void DaliDimmerDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  checkPresence(boost::bind(&DaliDimmerDevice::disconnectableHandler, this, aForgetParams, aDisconnectResultHandler, _1));
}

void DaliDimmerDevice::disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent)
{
  if (!aPresent) {
    // call inherited disconnect
    inherited::disconnect(aForgetParams, aDisconnectResultHandler);
  }
  else {
    // not disconnectable
    if (aDisconnectResultHandler) {
      aDisconnectResultHandler(false);
    }
  }
}


void DaliDimmerDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  LightBehaviourPtr lightBehaviour = boost::dynamic_pointer_cast<LightBehaviour>(output);
  if (lightBehaviour && lightBehaviour->brightnessNeedsApplying()) {
    brightnessDimmer->setTransitionTime(lightBehaviour->transitionTimeToNewBrightness());
    // update actual dimmer value
    brightnessDimmer->setBrightness(lightBehaviour->brightnessForHardware());
    lightBehaviour->brightnessApplied(); // confirm having applied the value
  }
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


// optimized DALI dimming implementation
void DaliDimmerDevice::dimChannel(DsChannelType aChannelType, VdcDimMode aDimMode)
{
  // start dimming
  if (aChannelType==channeltype_brightness) {
    // start dimming
    ALOG(LOG_INFO,
      "dimChannel (DALI): channel type %d (brightness) %s",
      aChannelType,
      aDimMode==dimmode_stop ? "STOPS dimming" : (aDimMode==dimmode_up ? "starts dimming UP" : "starts dimming DOWN")
    );
    ChannelBehaviourPtr ch = getChannelByType(aChannelType);
    brightnessDimmer->dim(aDimMode, ch->getDimPerMS());
  }
  else {
    // not my channel, use generic implementation
    inherited::dimChannel(aChannelType, aDimMode);
  }
}



/// save brightness as default for DALI dimmer to use after powerup and at failure
void DaliDimmerDevice::saveAsDefaultBrightness()
{
  brightnessDimmer->setDefaultBrightness(-1);
}




void DaliDimmerDevice::deriveDsUid()
{
  // single channel dimmer just uses dSUID derived from single DALI bus device
  dSUID = brightnessDimmer->dSUID;
}


string DaliDimmerDevice::hardwareGUID()
{
  if (brightnessDimmer->deviceInfo->devInfStatus<=DaliDeviceInfo::devinf_only_gtin)
    return ""; // none
  // return as GS1 element strings
  // Note: GTIN/Serial will be reported even if it could not be used for deriving dSUID (e.g. devinf_maybe/devinf_notForID cases)
  return string_format("gs1:(01)%llu(21)%llu", brightnessDimmer->deviceInfo->gtin, brightnessDimmer->deviceInfo->serialNo);
}


string DaliDimmerDevice::hardwareModelGUID()
{
  if (brightnessDimmer->deviceInfo->gtin==0)
    return ""; // none
  // return as GS1 element strings with Application Identifier 01=GTIN
  return string_format("gs1:(01)%llu", brightnessDimmer->deviceInfo->gtin);
}


string DaliDimmerDevice::oemGUID()
{
  if (brightnessDimmer->deviceInfo->oem_gtin==0 || brightnessDimmer->deviceInfo->oem_serialNo==0)
    return ""; // none
  // return as GS1 element strings with Application Identifiers 01=GTIN and 21=Serial
  return string_format("gs1:(01)%llu(21)%llu", brightnessDimmer->deviceInfo->oem_gtin, brightnessDimmer->deviceInfo->oem_serialNo);
}


string DaliDimmerDevice::oemModelGUID()
{
  if (brightnessDimmer->deviceInfo->oem_gtin==0)
    return ""; // none
  // return as GS1 element strings with Application Identifier 01=GTIN
  return string_format("gs1:(01)%llu", brightnessDimmer->deviceInfo->oem_gtin);
}


string DaliDimmerDevice::description()
{
  string s = inherited::description();
  s.append(brightnessDimmer->deviceInfo->description());
  return s;
}


// MARK: ===== DaliRGBWDevice (multi-channel color lamp)


DaliRGBWDevice::DaliRGBWDevice(DaliVdc *aVdcP) :
  DaliDevice(aVdcP)
{
}


void DaliRGBWDevice::willBeAdded()
{
  // Note: setting up behaviours late, because we want the brightness dimmer already assigned for the hardware name
  // set up dS behaviour for color lights, which include a color scene table
  installSettings(DeviceSettingsPtr(new ColorLightDeviceSettings(*this)));
  // set the behaviour
  RGBColorLightBehaviourPtr cl = RGBColorLightBehaviourPtr(new RGBColorLightBehaviour(*this));
  cl->setHardwareOutputConfig(outputFunction_colordimmer, outputmode_gradual, usage_undefined, true, 0); // DALI lights are always dimmable, no power known
  cl->setHardwareName(string_format("DALI color light"));
  cl->initMinBrightness(0.4); // min brightness is 0.4 (~= 1/256)
  addBehaviour(cl);
  // now derive dSUID
  deriveDsUid();
}


bool DaliRGBWDevice::getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix)
{
  if (getIcon("dali_color", aIcon, aWithData, aResolutionPrefix))
    return true;
  else
    return inherited::getDeviceIcon(aIcon, aWithData, aResolutionPrefix);
}


string DaliRGBWDevice::getExtraInfo()
{
  string s = string_format(
    "DALI short addresses: Red:%d, Green:%d, Blue:%d",
    dimmers[dimmer_red]->deviceInfo->shortAddress,
    dimmers[dimmer_green]->deviceInfo->shortAddress,
    dimmers[dimmer_blue]->deviceInfo->shortAddress
  );
  if (dimmers[dimmer_white]) {
    string_format_append(s, ", White:%d", dimmers[dimmer_white]->deviceInfo->shortAddress);
  }
  return s;
}


bool DaliRGBWDevice::addDimmer(DaliBusDevicePtr aDimmerBusDevice, string aDimmerType)
{
  if (aDimmerType=="R")
    dimmers[dimmer_red] = aDimmerBusDevice;
  else if (aDimmerType=="G")
    dimmers[dimmer_green] = aDimmerBusDevice;
  else if (aDimmerType=="B")
    dimmers[dimmer_blue] = aDimmerBusDevice;
  else if (aDimmerType=="W")
    dimmers[dimmer_white] = aDimmerBusDevice;
  else
    return false; // cannot add
  return true; // added ok
}



void DaliRGBWDevice::initializeDevice(StatusCB aCompletedCB, bool aFactoryReset)
{
  // - sync cached channel values from actual devices
  updateNextDimmer(aCompletedCB, aFactoryReset, dimmer_red, ErrorPtr());
}


void DaliRGBWDevice::updateNextDimmer(StatusCB aCompletedCB, bool aFactoryReset, DimmerIndex aDimmerIndex, ErrorPtr aError)
{
  if (!Error::isOK(aError)) {
    LOG(LOG_ERR, "DaliRGBWDevice: error getting state/params from dimmer#%d: %s", aDimmerIndex-1, aError->description().c_str());
  }
  while (aDimmerIndex<numDimmers) {
    DaliBusDevicePtr di = dimmers[aDimmerIndex];
    // process this dimmer if it exists
    if (di) {
      di->updateParams(boost::bind(&DaliRGBWDevice::updateNextDimmer, this, aCompletedCB, aFactoryReset, aDimmerIndex+1, _1));
      return; // return now, will be called again when update is complete
    }
    aDimmerIndex++; // next
  }
  // all updated (not necessarily successfully) if we land here
  RGBColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<RGBColorLightBehaviour>(output);
  double r = dimmers[dimmer_red] ? dimmers[dimmer_red]->currentBrightness : 0;
  double g = dimmers[dimmer_green] ? dimmers[dimmer_green]->currentBrightness : 0;
  double b = dimmers[dimmer_blue] ? dimmers[dimmer_blue]->currentBrightness : 0;
  if (dimmers[dimmer_white]) {
    double w = dimmers[dimmer_white]->currentBrightness;
    cl->setRGBW(r, g, b, w, 255);
  }
  else {
    cl->setRGB(r, g, b, 255);
  }
  // complete - continue with initialisation in superclasses
  inherited::initializeDevice(aCompletedCB, aFactoryReset);
}



DaliBusDevicePtr DaliRGBWDevice::firstBusDevice()
{
  for (DimmerIndex idx=dimmer_red; idx<numDimmers; idx++) {
    if (dimmers[idx]) {
      return dimmers[idx];
    }
  }
  return DaliBusDevicePtr(); // none
}



void DaliRGBWDevice::checkPresence(PresenceCB aPresenceResultHandler)
{
  // assuming all channels in the same physical device, check only first one
  DaliBusDevicePtr dimmer = firstBusDevice();
  if (dimmer) {
    dimmer->updateStatus(boost::bind(&DaliRGBWDevice::checkPresenceResponse, this, aPresenceResultHandler, dimmer));
    return;
  }
  // no dimmer -> not present
  aPresenceResultHandler(false);
}


void DaliRGBWDevice::checkPresenceResponse(PresenceCB aPresenceResultHandler, DaliBusDevicePtr aDimmer)
{
  // present if a proper YES (without collision) received
  aPresenceResultHandler(aDimmer->isPresent);
}



void DaliRGBWDevice::disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler)
{
  checkPresence(boost::bind(&DaliRGBWDevice::disconnectableHandler, this, aForgetParams, aDisconnectResultHandler, _1));
}

void DaliRGBWDevice::disconnectableHandler(bool aForgetParams, DisconnectCB aDisconnectResultHandler, bool aPresent)
{
  if (!aPresent) {
    // call inherited disconnect
    inherited::disconnect(aForgetParams, aDisconnectResultHandler);
  }
  else {
    // not disconnectable
    if (aDisconnectResultHandler) {
      aDisconnectResultHandler(false);
    }
  }
}


void DaliRGBWDevice::applyChannelValues(SimpleCB aDoneCB, bool aForDimming)
{
  RGBColorLightBehaviourPtr cl = boost::dynamic_pointer_cast<RGBColorLightBehaviour>(output);
  if (cl) {
    if (needsToApplyChannels()) {
      // needs update
      // - derive (possibly new) color mode from changed channels
      cl->deriveColorMode();
      // transition time is that of the brightness channel
      MLMicroSeconds tt = cl->transitionTimeToNewBrightness();
      // RGB lamp, get components
      double r,g,b,w;
      if (dimmers[dimmer_white]) {
        // RGBW
        cl->getRGBW(r, g, b, w, 100); // dali dimmers use abstracted 0..100% brightness as input
        if (!aForDimming) {
          ALOG(LOG_INFO,
            "DALI composite RGB: R=%d, G=%d, B=%d, W=%d",
            (int)r, (int)g, (int)b, (int)w
          );
        }
        dimmers[dimmer_white]->setTransitionTime(tt);
      }
      else {
        // RGB
        cl->getRGB(r, g, b, 100); // dali dimmers use abstracted 0..100% brightness as input
        if (!aForDimming) {
          ALOG(LOG_INFO,
            "DALI composite: R=%d, G=%d, B=%d",
            (int)r, (int)g, (int)b
          );
        }
      }
      // set transition time for all dimmers to brightness transition time
      dimmers[dimmer_red]->setTransitionTime(tt);
      dimmers[dimmer_green]->setTransitionTime(tt);
      dimmers[dimmer_blue]->setTransitionTime(tt);
      // apply new values
      dimmers[dimmer_red]->setBrightness(r);
      dimmers[dimmer_green]->setBrightness(g);
      dimmers[dimmer_blue]->setBrightness(b);
      if (dimmers[dimmer_white]) dimmers[dimmer_white]->setBrightness(w);
    } // if needs update
    // anyway, applied now
    cl->appliedColorValues();
  }
  // confirm done
  inherited::applyChannelValues(aDoneCB, aForDimming);
}


/// save brightness as default for DALI dimmer to use after powerup and at failure
/// @param aBrightness new brightness to set
void DaliRGBWDevice::saveAsDefaultBrightness()
{
  dimmers[dimmer_red]->setDefaultBrightness(-1);
  dimmers[dimmer_green]->setDefaultBrightness(-1);
  dimmers[dimmer_blue]->setDefaultBrightness(-1);
  if (dimmers[dimmer_white]) dimmers[dimmer_white]->setDefaultBrightness(-1);
}



void DaliRGBWDevice::deriveDsUid()
{
  // Multi-channel DALI devices construct their ID from UUIDs of the DALI devices involved,
  // but in a way that allows re-assignment of R/G/B without changing the dSUID
  DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
  string mixID;
  for (DimmerIndex idx=dimmer_red; idx<numDimmers; idx++) {
    if (dimmers[idx]) {
      // use this dimmer's dSUID as part of the mix
      dimmers[idx]->dSUID.xorDsUidIntoMix(mixID);
    }
  }
  // use xored ID as base for creating UUIDv5 in vdcNamespace
  dSUID.setNameInSpace("dalicombi:"+mixID, vdcNamespace);
}


string DaliRGBWDevice::hardwareGUID()
{
  DaliBusDevicePtr dimmer = firstBusDevice();
  if (!dimmer || dimmer->deviceInfo->gtin==0 || dimmer->deviceInfo->serialNo==0)
    return ""; // none
  // return as GS1 element strings
  return string_format("gs1:(01)%llu(21)%llu", dimmer->deviceInfo->gtin, dimmer->deviceInfo->serialNo);
}


string DaliRGBWDevice::hardwareModelGUID()
{
  DaliBusDevicePtr dimmer = firstBusDevice();
  if (!dimmer || dimmer->deviceInfo->gtin==0)
    return ""; // none
  // return as GS1 element strings with Application Identifier 01=GTIN
  return string_format("gs1:(01)%llu", dimmer->deviceInfo->gtin);
}


string DaliRGBWDevice::oemGUID()
{
  DaliBusDevicePtr dimmer = firstBusDevice();
  if (!dimmer || dimmer->deviceInfo->oem_gtin==0|| dimmer->deviceInfo->oem_serialNo==0)
    return ""; // none
  // return as GS1 element strings with Application Identifiers 01=GTIN and 21=Serial
  return string_format("gs1:(01)%llu(21)%llu", dimmer->deviceInfo->oem_gtin, dimmer->deviceInfo->oem_serialNo);
}


string DaliRGBWDevice::oemModelGUID()
{
  DaliBusDevicePtr dimmer = firstBusDevice();
  if (!dimmer || dimmer->deviceInfo->oem_gtin==0)
    return ""; // none
  // return as GS1 element strings with Application Identifier 01=GTIN
  return string_format("gs1:(01)%llu", dimmer->deviceInfo->oem_gtin);
}



string DaliRGBWDevice::description()
{
  string s = inherited::description();
  DaliBusDevicePtr dimmer = firstBusDevice();
  if (dimmer) s.append(dimmer->deviceInfo->description());
  return s;
}

#endif // ENABLE_DALI

