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

#ifndef __p44vdc__sensorbehaviour__
#define __p44vdc__sensorbehaviour__

#include "device.hpp"

#include <math.h>

using namespace std;

namespace p44 {


  /// Implements the behaviour of a digitalSTROM Sensor. In particular it manages and throttles
  /// pushing updates to the dS upstream, to avoid jitter in hardware reported values to flood
  /// the system with unneded update messages
  class SensorBehaviour : public DsBehaviour, public ValueSource
  {
    typedef DsBehaviour inherited;
    friend class Device;

  protected:

    /// @name behaviour description, constants or variables
    ///   set by device implementations when adding a Behaviour.
    /// @{
    VdcSensorType sensorType; ///< type and physical unit of sensor
    VdcUsageHint sensorUsage; ///< usage for sensor (if known)
    double min; ///< minimum value (corresponding to aEngineeringValue==0)
    double max; ///< max value
    double resolution; ///< change per LSB of sensor engineering value
    MLMicroSeconds updateInterval; ///< approximate time resolution of the sensor (how fast the sensor can track values)
    MLMicroSeconds aliveSignInterval; ///< how often the sensor reports a value minimally (if it does not report for longer than that, it can be considered out of order)
    /// @}

    /// @name persistent settings
    /// @{
    DsGroup sensorGroup; ///< group this sensor belongs to
    MLMicroSeconds minPushInterval; ///< minimum time between pushes (even if we have more frequent hardware sensor updates)
    MLMicroSeconds changesOnlyInterval; ///< time span during which only actual value changes are reported. After this interval, next hardware sensor update, even without value change, will cause a push)
    /// @}


    /// @name internal volatile state
    /// @{
    double currentValue; ///< current sensor value
    MLMicroSeconds lastUpdate; ///< time of last update from hardware
    MLMicroSeconds lastPush; ///< time of last push
    /// @}


  public:

    /// constructor
    SensorBehaviour(Device &aDevice);

    /// initialisation of hardware-specific constants for this sensor
    /// @param aType the sensor type (Note: not the same as dS sensor types, needs mapping)
    /// @param aUsage how this input is normally used (indoor/outdoor etc.)
    /// @param aMin minimum value)
    /// @param aMax maximum value)
    /// @param aResolution resolution (smallest step) of this sensor's value
    /// @param aUpdateInterval how often an update can be expected from this sensor normally. If 0, this means that no
    ///   regular updates can be expected.
    /// @param aAliveSignInterval how often the sensor will send an update in all cases. If 0, this means that no regular
    ///   update interval can be expected.
    /// @param aDefaultChangesOnlyInterval the minimum time between two pushes with the same value. If the sensor hardware
    ///   sends updates more frequently, these are only pushed when the value has actually changed.
    /// @note this must be called once before the device gets added to the device container. Implementation might
    ///   also derive default values for settings from this information.
    void setHardwareSensorConfig(VdcSensorType aType, VdcUsageHint aUsage, double aMin, double aMax, double aResolution, MLMicroSeconds aUpdateInterval, MLMicroSeconds aAliveSignInterval, MLMicroSeconds aDefaultChangesOnlyInterval=0);

    /// create a hardware name including a sensor type text, the range (max/min/resolution) and the physical unit text
    /// @param aTypeText the sensor type (like "temperature", "humidity")
    /// @param aUnitText the physical unit (like "°C", "%")
    void setSensorNameFrom(const char *aTypeText, const char *aUnitText);

    /// Utility function for getting sensor description text (as used in setSensorNameFrom()) from out of sensorbehaviour context
    static string sensorDescriptionFrom(const char *aTypeText, const char *aUnitText, double aMin, double aMax, double aResolution);


    /// set group
    virtual void setGroup(DsGroup aGroup) { sensorGroup = aGroup; };

    /// @name interface towards actual device hardware (or simulation)
    /// @{

    /// current value and range
    double getCurrentValue() { return currentValue; };
    MLMicroSeconds getLastUpdateTimestamp() { return lastUpdate; };
    double getMax() { return max; };
    double getMin() { return min; };
    double getResolution() { return resolution; };

    /// get sensor type
    /// @return the sensor type
    VdcSensorType getSensorType() { return sensorType; };

    /// invalidate sensor value, i.e. indicate that current value is not known
    void invalidateSensorValue();

    /// update sensor value (when new value received from hardware)
    /// @param aValue the new value from the sensor, in physical units according to sensorType (VdcSensorType)
    /// @param aMinChange what minimum change the new value must have compared to last reported value
    ///   to be treated as a change. Default is -1, which means half the declared resolution.
    void updateSensorValue(double aValue, double aMinChange = -1);

    /// sensor value change occurred
    /// @param aEngineeringValue the engineering value from the sensor.
    ///   The state value will be adjusted and scaled according to min/resolution
    /// @note this call only works correctly if resolution relates to 1 LSB of aEngineeringValue
    ///   Use updateSensorValue if relation between engineering value and physical unit value is more complicated
    void updateEngineeringValue(long aEngineeringValue);

    /// @}

    /// check if we have a recent value
    /// @param aMaxAge how old a value we consider still "valid"
    /// @return true if the sensor has a value not older than aMaxAge
    bool hasCurrentValue(MLMicroSeconds aMaxAge);

    /// check for defined state
    /// @return true if behaviour has a defined (non-NULL) state
    virtual bool hasDefinedState();


    /// @name ValueSource interface
    /// @{

    /// get descriptive name identifying the source within the entire vdc host (for using in selection lists)
    virtual string getSourceName();

    /// get value
    virtual double getSourceValue();

    /// get time of last update
    virtual MLMicroSeconds getSourceLastUpdate();

    /// @}



    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

  protected:

    /// the behaviour type
    virtual BehaviourType getType() { return behaviour_sensor; };

    // property access implementation for descriptor/settings/states
    virtual int numDescProps();
    virtual const PropertyDescriptorPtr getDescDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    virtual int numSettingsProps();
    virtual const PropertyDescriptorPtr getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    virtual int numStateProps();
    virtual const PropertyDescriptorPtr getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    // combined field access for all types of properties
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  };
  typedef boost::intrusive_ptr<SensorBehaviour> SensorBehaviourPtr;



} // namespace p44

#endif /* defined(__p44vdc__sensorbehaviour__) */
