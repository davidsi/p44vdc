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

#ifndef __p44vdc__homeconnectdevicedishwasher__
#define __p44vdc__homeconnectdevicedishwasher__

#include "homeconnectdevice.hpp"

#if ENABLE_HOMECONNECT

namespace p44 {

class HomeConnectDeviceDishWasher: public HomeConnectDevice
{
  typedef HomeConnectDevice inherited;

  ValueDescriptorPtr delayedStartProp;

  virtual void configureDevice(StatusCB aStatusCB) P44_OVERRIDE;
  virtual void stateChanged(DeviceStatePtr aChangedState, DeviceEventsList &aEventsToPush) P44_OVERRIDE;
  virtual void handleEventTypeNotify(const string& aKey, JsonObjectPtr aValue) P44_OVERRIDE;
  void handleStartInRelativeChange(int32_t aValue);

  void addAction(const string& aActionName, const string& aDescription, const string& aProgramName, ValueDescriptorPtr aParameter);
  void gotAvailablePrograms(JsonObjectPtr aResult, ErrorPtr aError, ValueDescriptorPtr aDelayedStart, StatusCB aStatusCB);

public:
  HomeConnectDeviceDishWasher(HomeConnectVdc *aVdcP, JsonObjectPtr aHomeApplicanceInfoRecord);
  virtual ~HomeConnectDeviceDishWasher();

  virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix) P44_OVERRIDE;
};

} /* namespace p44 */

#endif // ENABLE_HOMECONNECT

#endif /* __p44vdc__homeconnectdevicedishwasher__ */
