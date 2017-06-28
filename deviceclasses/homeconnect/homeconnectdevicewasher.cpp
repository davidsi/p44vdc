//
//  Copyright (c) 2017 digitalSTROM.org, Zurich, Switzerland
//
//  Author: Pawel Kochanowski <pawel.kochanowski@digitalstrom.com>
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

#include "homeconnectdevicewasher.hpp"

#if ENABLE_HOMECONNECT

namespace p44 {

HomeConnectDeviceWasher::HomeConnectDeviceWasher(HomeConnectVdc *aVdcP, JsonObjectPtr aHomeApplicanceInfoRecord) :
    inherited(aVdcP, aHomeApplicanceInfoRecord)
{
  hcDevType = homeconnect_washer;
}

HomeConnectDeviceWasher::~HomeConnectDeviceWasher()
{
  // TODO Auto-generated destructor stub
}

bool HomeConnectDeviceWasher::configureDevice()
{
  // create states
  configureOperationModeState(false, true, true, true, true, false);
  configureRemoteControlState();
  configureRemoteStartState();
  configureDoorState(true);
  configurePowerState(false, false);

  return inherited::configureDevice();
}

void HomeConnectDeviceWasher::stateChanged(DeviceStatePtr aChangedState, DeviceEventsList &aEventsToPush)
{
  inherited::stateChanged(aChangedState, aEventsToPush);
}

void HomeConnectDeviceWasher::handleEvent(string aEventType, JsonObjectPtr aEventData, ErrorPtr aError)
{
  ALOG(LOG_INFO, "Washer Event '%s' - item: %s", aEventType.c_str(), aEventData ? aEventData->c_strValue() : "<none>");
  inherited::handleEvent(aEventType, aEventData, aError);
}

} /* namespace p44 */

#endif // ENABLE_HOMECONNECT
