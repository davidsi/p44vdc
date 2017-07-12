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

#include "homeconnectdevicecoffemaker.hpp"

#if ENABLE_HOMECONNECT

namespace p44 {

HomeConnectDeviceCoffeMaker::HomeConnectDeviceCoffeMaker(HomeConnectVdc *aVdcP, JsonObjectPtr aHomeApplicanceInfoRecord) :
    standalone(false), inherited(aVdcP, aHomeApplicanceInfoRecord)
{

}

HomeConnectDeviceCoffeMaker::~HomeConnectDeviceCoffeMaker()
{
  // TODO Auto-generated destructor stub
}

bool HomeConnectDeviceCoffeMaker::configureDevice()
{
  HomeConnectActionPtr a;
  // FIXME: ugly direct model match
//    standalone = (modelGuid=="TI909701HC/03") || (modelGuid=="TI909701HC/00");
  // FIXME: got even uglier:
  standalone = (modelGuid.substr(0,10)=="TI909701HC");

  addAction("std.Espresso",          "Espresso",            "Espresso",          35,  60,  5,  40);
  addAction("std.EspressoMacchiato", "Espresso Macchiato",  "EspressoMacchiato", 40,  60,  10, 50);
  addAction("std.Coffee",            "Coffee",              "Coffee",            60,  250, 10, 120);
  addAction("std.Cappuccino",        "Cappuccino",          "Cappuccino",        100, 250, 10, 180);
  addAction("std.LatteMacchiato",    "Latte Macchiato",     "LatteMacchiato",    200, 400, 20, 300);
  addAction("std.CaffeLatte",        "Caffe Latte",         "CaffeLatte",        100, 400, 20, 250);

  beanAmountProp = EnumValueDescriptorPtr(new EnumValueDescriptor("BeanAmount", true));
  int i = 0;
  beanAmountProp->addEnum("VeryMild", i++);
  beanAmountProp->addEnum("Mild", i++);
  beanAmountProp->addEnum("Normal", i++, true);
  beanAmountProp->addEnum("Strong", i++);
  beanAmountProp->addEnum("VeryStrong", i++);
  beanAmountProp->addEnum("DoubleShot", i++);
  beanAmountProp->addEnum("DoubleShotPlus", i++);
  beanAmountProp->addEnum("DoubleShotPlusPlus", i++);

  fillQuantityProp = ValueDescriptorPtr(new NumericValueDescriptor("FillQuantity", valueType_numeric, VALUE_UNIT(valueUnit_liter, unitScaling_milli), 0, 400, 1));

  deviceProperties->addProperty(beanAmountProp);
  deviceProperties->addProperty(fillQuantityProp);

  // configure operation mode
  OperationModeConfiguration omConfig = { 0 };
  omConfig.hasInactive = true;
  omConfig.hasReady = true;
  omConfig.hasDelayedStart = false;
  omConfig.hasRun = true;
  omConfig.hasPause = false;
  omConfig.hasActionrequired = true;
  omConfig.hasFinished = true;
  omConfig.hasError = true;
  omConfig.hasAborting = true;
  configureOperationModeState(omConfig);

  // configure remote control
  RemoteControlConfiguration rcConfig = { 0 };
  rcConfig.hasControlInactive = true;
  rcConfig.hasControlActive = false;  // coffee machine do not have BSH.Common.Status.RemoteControlActive so it is either inactive or start Allowed
  rcConfig.hasStartActive = true;
  configureRemoteControlState(rcConfig);

  // configure power state
  PowerStateConfiguration psConfig = { 0 };
  psConfig.hasOff = false;
  psConfig.hasOn = true;
  psConfig.hasStandby = true;
  configurePowerState(psConfig);

  return inherited::configureDevice();
}

void HomeConnectDeviceCoffeMaker::stateChanged(DeviceStatePtr aChangedState, DeviceEventsList &aEventsToPush)
{
  inherited::stateChanged(aChangedState, aEventsToPush);
}

void HomeConnectDeviceCoffeMaker::handleEvent(string aEventType, JsonObjectPtr aEventData, ErrorPtr aError)
{
  ALOG(LOG_INFO, "CoffeMaker Event '%s' - item: %s", aEventType.c_str(), aEventData ? aEventData->c_strValue() : "<none>");

  JsonObjectPtr oKey;
  JsonObjectPtr oValue;

  if (!aEventData || !aEventData->get("key", oKey) || !aEventData->get("value", oValue) ) {
    return;
  }

  if( aEventType == "NOTIFY") {
    string key = (oKey != NULL) ? oKey->stringValue() : "";

    if (key == "ConsumerProducts.CoffeeMaker.Option.BeanAmount") {
      string value = (oValue != NULL) ? oValue->stringValue() : "";
      beanAmountProp->setStringValue(removeNamespace(value));
      return;
    }

    if (key == "ConsumerProducts.CoffeeMaker.Option.FillQuantity") {
      int32_t value = (oValue != NULL) ? oValue->int32Value() : 0;
      fillQuantityProp->setInt32Value(value);
      return;
    }
  }

  inherited::handleEvent(aEventType, aEventData, aError);
}

void HomeConnectDeviceCoffeMaker::addAction(const string& aActionName,
                                            const string& aDescription,
                                            const string& aProgramName,
                                            double aFillAmountMin,
                                            double aFillAmountMax,
                                            double aFillAmountResolution,
                                            double aFillAmountDefault)
{

  HomeConnectCommandBuilder builder("ConsumerProducts.CoffeeMaker.Program.Beverage." + aProgramName);

  if(!standalone) {
    builder.addOption("ConsumerProducts.CoffeeMaker.Option.CoffeeTemperature", "\"ConsumerProducts.CoffeeMaker.EnumType.CoffeeTemperature.@{temperatureLevel}\"");
  }

  builder.addOption("ConsumerProducts.CoffeeMaker.Option.BeanAmount", "\"ConsumerProducts.CoffeeMaker.EnumType.BeanAmount.@{beanAmount}\"");
  builder.addOption("ConsumerProducts.CoffeeMaker.Option.FillQuantity", "@{fillQuantity%%0}");

  EnumValueDescriptorPtr tempLevel = EnumValueDescriptorPtr(new EnumValueDescriptor("temperatureLevel", true));
  int i = 0;
  tempLevel->addEnum("Normal", i++, true); // default
  tempLevel->addEnum("High", i++);
  tempLevel->addEnum("VeryHigh", i++);

  EnumValueDescriptorPtr beanAmount = EnumValueDescriptorPtr(new EnumValueDescriptor("beanAmount", true));
  i = 0;
  beanAmount->addEnum("VeryMild", i++);
  beanAmount->addEnum("Mild", i++);
  beanAmount->addEnum("Normal", i++, true); // default
  beanAmount->addEnum("Strong", i++);
  beanAmount->addEnum("VeryStrong", i++);
  beanAmount->addEnum("DoubleShot", i++);
  beanAmount->addEnum("DoubleShotPlus", i++);
  beanAmount->addEnum("DoubleShotPlusPlus", i++);

  ValueDescriptorPtr fillAmount =
      ValueDescriptorPtr(new NumericValueDescriptor("fillQuantity",
                                                    valueType_numeric,
                                                    VALUE_UNIT(valueUnit_liter, unitScaling_milli),
                                                    aFillAmountMin,
                                                    aFillAmountMax,
                                                    aFillAmountResolution,
                                                    true,
                                                    aFillAmountDefault));

  HomeConnectActionPtr action = HomeConnectActionPtr(new HomeConnectAction(*this, aActionName, aDescription, builder.build()));
  action->addParameter(tempLevel);
  action->addParameter(beanAmount);
  action->addParameter(fillAmount);
  deviceActions->addAction(action);
}

string HomeConnectDeviceCoffeMaker::oemModelGUID()
{
  return "gs1:(01)7640156792096";
}

} /* namespace p44 */

#endif // ENABLE_HOMECONNECT
