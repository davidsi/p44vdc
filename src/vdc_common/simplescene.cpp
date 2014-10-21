//
//  SimpleScene.cpp
//  vdcd
//
//  Created by Lukas Zeller on 21.10.14.
//  Copyright (c) 2014 plan44.ch. All rights reserved.
//

#include "SimpleScene.hpp"


#pragma mark - SimpleScene

using namespace p44;


SimpleScene::SimpleScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo) :
  inherited(aSceneDeviceSettings, aSceneNo)
{
  value = 0;
  effect = scene_effect_smooth;
}


#pragma mark - scene values/channels


double SimpleScene::sceneValue(size_t aOutputIndex)
{
  return value;
}


void SimpleScene::setSceneValue(size_t aOutputIndex, double aValue)
{
  if (aOutputIndex==0) {
    value = aValue;
  }
}


#pragma mark - Scene persistence

const char *SimpleScene::tableName()
{
  return "LightScenes"; // named "LightScenes" for historical reasons - initially only lights used this
}

// data field definitions

static const size_t numSceneFields = 2;

size_t SimpleScene::numFieldDefs()
{
  return inherited::numFieldDefs()+numSceneFields;
}


const FieldDefinition *SimpleScene::getFieldDef(size_t aIndex)
{
  static const FieldDefinition dataDefs[numSceneFields] = {
    { "brightness", SQLITE_FLOAT }, // named "brightness" for historical reasons - initially only lights used this
    { "effect", SQLITE_INTEGER }
  };
  if (aIndex<inherited::numFieldDefs())
    return inherited::getFieldDef(aIndex);
  aIndex -= inherited::numFieldDefs();
  if (aIndex<numSceneFields)
    return &dataDefs[aIndex];
  return NULL;
}


/// load values from passed row
void SimpleScene::loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex)
{
  inherited::loadFromRow(aRow, aIndex);
  // get the fields
  value = aRow->get<double>(aIndex++);
  effect = (DsSceneEffect)aRow->get<int>(aIndex++);
}


/// bind values to passed statement
void SimpleScene::bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier)
{
  inherited::bindToStatement(aStatement, aIndex, aParentIdentifier);
  // bind the fields
  aStatement.bind(aIndex++, value);
  aStatement.bind(aIndex++, effect);
}


#pragma mark - SimpleScene property access


static char lightscene_key;

enum {
  effect_key,
  numSimpleSceneProperties
};


int SimpleScene::numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  return inherited::numProps(aDomain, aParentDescriptor)+numSimpleSceneProperties;
}


PropertyDescriptorPtr SimpleScene::getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor)
{
  static const PropertyDescription properties[numSimpleSceneProperties] = {
    { "effect", apivalue_uint64, effect_key, OKEY(lightscene_key) },
  };
  int n = inherited::numProps(aDomain, aParentDescriptor);
  if (aPropIndex<n)
    return inherited::getDescriptorByIndex(aPropIndex, aDomain, aParentDescriptor); // base class' property
  aPropIndex -= n; // rebase to 0 for my own first property
  return PropertyDescriptorPtr(new StaticPropertyDescriptor(&properties[aPropIndex], aParentDescriptor));
}


bool SimpleScene::accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor)
{
  if (aPropertyDescriptor->hasObjectKey(lightscene_key)) {
    if (aMode==access_read) {
      // read properties
      switch (aPropertyDescriptor->fieldKey()) {
        case effect_key:
          aPropValue->setUint8Value(effect);
          return true;
      }
    }
    else {
      // write properties
      switch (aPropertyDescriptor->fieldKey()) {
        case effect_key:
          effect = (DsSceneEffect)aPropValue->uint8Value();
          markDirty();
          return true;
      }
    }
  }
  return inherited::accessField(aMode, aPropValue, aPropertyDescriptor);
}


#pragma mark - default scene values

typedef struct {
  uint8_t value; ///< output value for this scene, uint_8 to save footprint
  DsSceneEffect effect;
  bool ignoreLocalPriority; ///< if set, local priority is ignored when calling this scene
  bool dontCare; ///< if set, applying this scene does not change the output value
} DefaultSceneParams;

#define NUMDEFAULTSCENES 80 ///< Number of default scenes

// General rules

//  Rule 1 A digitalSTROM Ready Device has to be preconfigured in the right functional group. This is essential to ensure that all electrical devices in one functional group can be orchestrated together.
//  Rule 2 A digitalSTROM Ready Device must be configured for exactly one digitalSTROM functional group. The assigned functional group must be non- ambiguous and is part of the static device configuration (see Function-ID 9.4).
//  Rule 3 The function of a devices output is the basis of its group member- ship. For devices without actuator the target function of the switch button decides about the group membership.
//  Rule 4 digitalSTROM Devices have to implement a default behavior for all 128 scene commands. The system behavior and default values are defined in the particular documents for each functional group.
//  Rule 5 When applications send a scene command to a set of digitalSTROM Devices with more than one target device they have to use scene calls di- rected to a group, splitting into multiple calls to single devices has to be avoided due to latency and statemachine consistency issues.

//  Rule 6 digitalSTROM Ready Devices must ignore stepping commands if their output value is zero.

//  Rule 7 digitalSTROM Device have to complete the identification action on the command Programming Mode Start within 4 seconds.
//  Rule 8 Application processes that do automatic cyclic reads or writes of device parameters are subject to a request limit: at maximum one request per minute and circuit is allowed.
//  Rule 9 Application processes that do automatic cyclic reads of measured values are subject to a request limit: at maximum one request per minute and circuit is allowed.
//  Rule 10 The action command "SetOutputValue" must not be used for other than device configuration purposes.
//  Rule 11 digitalSTROM Ready Devices must not send upstream events continously and must stop sending Low-Level-Event data even if the event is still or repeatedly valid. Transmission of pushbutton events must be abondoned after a maximum time of 2.5 minutes. Automatically genereated events must not exceed a rate limit of 5 events per 5 minutes.
//  Rule 12 Applications shall use the digitalSTROM Server webservice inter- face for communication with the digitalSTROM system. Directly interfacing the dSM-API shall be avoided because it is an internal interface and its API may change in the future.

//  Rule 13 Applications that automatically generate Call Scene action commands (see 5.1.1) must not execute the action commands at a rate faster than one request per second.


static const DefaultSceneParams defaultScenes[NUMDEFAULTSCENES+1] = {
  // group related scenes
  // { brightness, effect, ignoreLocalPriority, dontCare }
  {   0, scene_effect_smooth, false, false }, // 0 : Preset 0 - T0_S0
  {   0, scene_effect_smooth, true , false }, // 1 : Area 1 Off - T1_S0
  {   0, scene_effect_smooth, true , false }, // 2 : Area 2 Off - T2_S0
  {   0, scene_effect_smooth, true , false }, // 3 : Area 3 Off - T3_S0
  {   0, scene_effect_smooth, true , false }, // 4 : Area 4 Off - T4_S0
  { 100, scene_effect_smooth, false, false }, // 5 : Preset 1 - T0_S1
  { 100, scene_effect_smooth, true , false }, // 6 : Area 1 On - T1_S1
  { 100, scene_effect_smooth, true , false }, // 7 : Area 2 On - T1_S1
  { 100, scene_effect_smooth, true , false }, // 8 : Area 3 On - T1_S1
  { 100, scene_effect_smooth, true , false }, // 9 : Area 4 On - T1_S1
  {   0, scene_effect_smooth, true , false }, // 10 : Area Stepping continue - T1234_CONT
  {   0, scene_effect_smooth, false, false }, // 11 : Decrement - DEC_S
  {   0, scene_effect_smooth, false, false }, // 12 : Increment - INC_S
  {   0, scene_effect_smooth, true , false }, // 13 : Minimum - MIN_S
  { 100, scene_effect_smooth, true , false }, // 14 : Maximum - MAX_S
  {   0, scene_effect_smooth, true , false }, // 15 : Stop - STOP_S
  {   0, scene_effect_smooth, false, true  }, // 16 : Reserved
  {  75, scene_effect_smooth, false, false }, // 17 : Preset 2 - T0_S2
  {  50, scene_effect_smooth, false, false }, // 18 : Preset 3 - T0_S3
  {  25, scene_effect_smooth, false, false }, // 19 : Preset 4 - T0_S4
  {  75, scene_effect_smooth, false, false }, // 20 : Preset 12 - T1_S2
  {  50, scene_effect_smooth, false, false }, // 21 : Preset 13 - T1_S3
  {  25, scene_effect_smooth, false, false }, // 22 : Preset 14 - T1_S4
  {  75, scene_effect_smooth, false, false }, // 23 : Preset 22 - T2_S2
  {  65, scene_effect_smooth, false, false }, // 24 : Preset 23 - T2_S3
  {  64, scene_effect_smooth, false, false }, // 25 : Preset 24 - T2_S4
  {  75, scene_effect_smooth, false, false }, // 26 : Preset 32 - T3_S2
  {  65, scene_effect_smooth, false, false }, // 27 : Preset 33 - T3_S3
  {  25, scene_effect_smooth, false, false }, // 28 : Preset 34 - T3_S4
  {  75, scene_effect_smooth, false, false }, // 29 : Preset 42 - T4_S2
  {  65, scene_effect_smooth, false, false }, // 30 : Preset 43 - T4_S3
  {  25, scene_effect_smooth, false, false }, // 31 : Preset 44 - T4_S4
  {   0, scene_effect_smooth, false, false }, // 32 : Preset 10 - T1E_S0
  { 100, scene_effect_smooth, false, false }, // 33 : Preset 11 - T1E_S1
  {   0, scene_effect_smooth, false, false }, // 34 : Preset 20 - T2E_S0
  { 100, scene_effect_smooth, false, false }, // 35 : Preset 21 - T2E_S1
  {   0, scene_effect_smooth, false, false }, // 36 : Preset 30 - T3E_S0
  { 100, scene_effect_smooth, false, false }, // 37 : Preset 31 - T3E_S1
  {   0, scene_effect_smooth, false, false }, // 38 : Preset 40 - T4E_S0
  { 100, scene_effect_smooth, false, false }, // 39 : Preset 41 - T4E_S1
  {   0, scene_effect_smooth, false, false }, // 40 : Fade down to 0 in 1min - AUTO_OFF
  {   0, scene_effect_smooth, false, true  }, // 41 : Reserved
  {   0, scene_effect_smooth, true , false }, // 42 : Area 1 Decrement - T1_DEC
  {   0, scene_effect_smooth, true , false }, // 43 : Area 1 Increment - T1_INC
  {   0, scene_effect_smooth, true , false }, // 44 : Area 2 Decrement - T2_DEC
  {   0, scene_effect_smooth, true , false }, // 45 : Area 2 Increment - T2_INC
  {   0, scene_effect_smooth, true , false }, // 46 : Area 3 Decrement - T3_DEC
  {   0, scene_effect_smooth, true , false }, // 47 : Area 3 Increment - T3_INC
  {   0, scene_effect_smooth, true , false }, // 48 : Area 4 Decrement - T4_DEC
  {   0, scene_effect_smooth, true , false }, // 49 : Area 4 Increment - T4_INC
  {   0, scene_effect_smooth, true , false }, // 50 : Device (Local Button) on : LOCAL_OFF
  { 100, scene_effect_smooth, true , false }, // 51 : Device (Local Button) on : LOCAL_ON
  {   0, scene_effect_smooth, true , false }, // 52 : Area 1 Stop - T1_STOP_S
  {   0, scene_effect_smooth, true , false }, // 53 : Area 2 Stop - T2_STOP_S
  {   0, scene_effect_smooth, true , false }, // 54 : Area 3 Stop - T3_STOP_S
  {   0, scene_effect_smooth, true , false }, // 55 : Area 4 Stop - T4_STOP_S
  {   0, scene_effect_smooth, false, true  }, // 56 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 57 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 58 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 59 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 60 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 61 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 62 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 63 : Reserved
  // global, appartment-wide, group independent scenes
  {   0, scene_effect_slow  , true , false }, // 64 : Auto Standby - AUTO_STANDBY
  { 100, scene_effect_smooth, true , false }, // 65 : Panic - SIG_PANIC
  {   0, scene_effect_smooth, false, true  }, // 66 : Reserved (ENERGY_OL)
  {   0, scene_effect_smooth, true , false }, // 67 : Standby - STANDBY
  {   0, scene_effect_smooth, true , false }, // 68 : Deep Off - DEEP_OFF
  {   0, scene_effect_smooth, true , false }, // 69 : Sleeping - SLEEPING
  { 100, scene_effect_smooth, true , true  }, // 70 : Wakeup - WAKE_UP
  { 100, scene_effect_smooth, true , true  }, // 71 : Present - PRESENT
  {   0, scene_effect_smooth, true , false }, // 72 : Absent - ABSENT
  {   0, scene_effect_smooth, true , true  }, // 73 : Door Bell - SIG_BELL
  {   0, scene_effect_smooth, false, true  }, // 74 : Reserved (SIG_ALARM)
  { 100, scene_effect_smooth, false, true  }, // 75 : Zone Active
  { 100, scene_effect_smooth, false, true  }, // 76 : Reserved
  { 100, scene_effect_smooth, false, true  }, // 77 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 78 : Reserved
  {   0, scene_effect_smooth, false, true  }, // 79 : Reserved
  // all other scenes equal or higher
  {   0, scene_effect_smooth, false, true  }, // 80..n : Reserved
};


void SimpleScene::setDefaultSceneValues(SceneNo aSceneNo)
{
  // fetch from defaults
  if (aSceneNo>NUMDEFAULTSCENES)
    aSceneNo = NUMDEFAULTSCENES; // last entry in the table is the default for all higher scene numbers
  const DefaultSceneParams &p = defaultScenes[aSceneNo];
  // now set default values
  // - common scene flags
  setIgnoreLocalPriority(p.ignoreLocalPriority);
  setDontCare(p.dontCare);
  // - light scene specifics
  value = p.value;
  effect = p.effect;
}


