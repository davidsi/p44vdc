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

#ifndef __p44vdc__colorlightbehaviour__
#define __p44vdc__colorlightbehaviour__

#include "device.hpp"
#include "dsscene.hpp"
#include "lightbehaviour.hpp"
#include "colorutils.hpp"

using namespace std;

namespace p44 {


  typedef enum {
    colorLightModeNone, ///< no color information stored, only brightness
    colorLightModeHueSaturation, ///< "hs" - hue & saturation
    colorLightModeXY, ///< "xy" - CIE color space coordinates
    colorLightModeCt, ///< "ct" - Mired color temperature: 153 (6500K) to 500 (2000K)
  } ColorLightMode;



  class ColorChannel : public ChannelBehaviour
  {
    typedef ChannelBehaviour inherited;

  public:

    ColorChannel(OutputBehaviour &aOutput) : inherited(aOutput) {};

    virtual ColorLightMode colorMode() = 0;

    /// get current value of this channel - and calculate it if it is not set in the device, but must be calculated from other channels
    virtual double getChannelValueCalculated();

  };


  class HueChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    HueChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.1; /* 0.1 degree */ };

    virtual DsChannelType getChannelType() P44_OVERRIDE { return channeltype_hue; }; ///< the dS channel type
    virtual ValueUnit getChannelUnit() P44_OVERRIDE { return VALUE_UNIT(valueUnit_degree, unitScaling_1); };
    virtual ColorLightMode colorMode() P44_OVERRIDE { return colorLightModeHueSaturation; };
    virtual const char *getName() P44_OVERRIDE { return "hue"; };
    virtual double getMin() P44_OVERRIDE { return 0; }; // hue goes from 0 to (almost) 360 degrees
    virtual double getMax() P44_OVERRIDE { return 358.6; };
    virtual bool wrapsAround() P44_OVERRIDE { return true; }; ///< hue wraps around
  };


  class SaturationChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    SaturationChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.1; /* 0.1 percent */ };

    virtual DsChannelType getChannelType() P44_OVERRIDE { return channeltype_saturation; }; ///< the dS channel type
    virtual ValueUnit getChannelUnit() P44_OVERRIDE { return VALUE_UNIT(valueUnit_percent, unitScaling_1); };
    virtual ColorLightMode colorMode() P44_OVERRIDE { return colorLightModeHueSaturation; };
    virtual const char *getName() P44_OVERRIDE { return "saturation"; };
    virtual double getMin() P44_OVERRIDE { return 0; }; // saturation goes from 0 to 100 percent
    virtual double getMax() P44_OVERRIDE { return 100; };
  };


  class ColorTempChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    ColorTempChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 1; /* 1 mired */ };

    virtual DsChannelType getChannelType() P44_OVERRIDE { return channeltype_colortemp; }; ///< the dS channel type
    virtual ValueUnit getChannelUnit() P44_OVERRIDE { return VALUE_UNIT(ValueUnit_mired, unitScaling_1); };
    virtual ColorLightMode colorMode() P44_OVERRIDE { return colorLightModeCt; };
    virtual const char *getName() P44_OVERRIDE { return "color temperature"; };
    virtual double getMin() P44_OVERRIDE { return 100; }; // CT goes from 100 to 1000 mired (10000K to 1000K)
    virtual double getMax() P44_OVERRIDE { return 1000; };
  };


  class CieXChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    CieXChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.01; /* 1% of full scale */ };

    virtual DsChannelType getChannelType() P44_OVERRIDE { return channeltype_cie_x; }; ///< the dS channel type
    virtual ValueUnit getChannelUnit() P44_OVERRIDE { return VALUE_UNIT(valueUnit_none, unitScaling_1); };
    virtual ColorLightMode colorMode() P44_OVERRIDE { return colorLightModeXY; };
    virtual const char *getName() P44_OVERRIDE { return "CIE X"; };
    virtual double getMin() P44_OVERRIDE { return 0; }; // CIE x and y have 0..1 range
    virtual double getMax() P44_OVERRIDE { return 1; };
  };


  class CieYChannel : public ColorChannel
  {
    typedef ColorChannel inherited;

  public:
    CieYChannel(OutputBehaviour &aOutput) : inherited(aOutput) { resolution = 0.01; /* 1% of full scale */ };

    virtual DsChannelType getChannelType() P44_OVERRIDE { return channeltype_cie_y; }; ///< the dS channel type
    virtual ValueUnit getChannelUnit() P44_OVERRIDE { return VALUE_UNIT(valueUnit_none, unitScaling_1); };
    virtual ColorLightMode colorMode() P44_OVERRIDE { return colorLightModeXY; };
    virtual const char *getName() P44_OVERRIDE { return "CIE Y"; };
    virtual double getMin() P44_OVERRIDE { return 0; }; // CIE x and y have 0..1 range
    virtual double getMax() P44_OVERRIDE { return 1; };
  };




  class ColorLightScene : public LightScene
  {
    typedef LightScene inherited;
    
  public:
    ColorLightScene(SceneDeviceSettings &aSceneDeviceSettings, SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values

    /// @name color light scene specific values
    /// @{

    ColorLightMode colorMode; ///< color mode (hue+Saturation or CIE xy or color temperature)
    double XOrHueOrCt; ///< X or hue or ct, depending on colorMode
    double YOrSat; ///< Y or saturation, depending on colorMode

    /// @}

    /// Set default scene values for a specified scene number
    /// @param aSceneNo the scene number to set default values
    virtual void setDefaultSceneValues(SceneNo aSceneNo);

    // scene values implementation
    virtual double sceneValue(size_t aChannelIndex);
    virtual void setSceneValue(size_t aChannelIndex, double aValue);

  protected:

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  };
  typedef boost::intrusive_ptr<ColorLightScene> ColorLightScenePtr;



  /// the persistent parameters of a light scene device (including scene table)
  class ColorLightDeviceSettings : public LightDeviceSettings
  {
    typedef LightDeviceSettings inherited;

  public:
    ColorLightDeviceSettings(Device &aDevice);

    /// factory method to create the correct subclass type of DsScene
    /// @param aSceneNo the scene number to create a scene object for.
    /// @note setDefaultSceneValues() must be called to set default scene values
    virtual DsScenePtr newDefaultScene(SceneNo aSceneNo);

  };


  class ColorLightBehaviour : public LightBehaviour
  {
    typedef LightBehaviour inherited;
    friend class ColorLightScene;

  public:

    /// @name internal volatile state
    /// @{
    ColorLightMode colorMode;
    bool derivedValuesComplete;
    /// @}


    /// @name channels
    /// @{
    ChannelBehaviourPtr hue;
    ChannelBehaviourPtr saturation;
    ChannelBehaviourPtr ct;
    ChannelBehaviourPtr cieX;
    ChannelBehaviourPtr cieY;
    /// @}


    ColorLightBehaviour(Device &aDevice);

    /// device type identifier
    /// @return constant identifier for this type of behaviour
    virtual const char *behaviourTypeIdentifier() { return "colorlight"; };

    /// @name color services for implementing color lights
    /// @{

    /// @return true if light is not full color, but color temperature only
    bool isCtOnly() { return false; }

    /// derives the color mode from channel values that need to be applied to hardware
    /// @return true if mode could be found
    bool deriveColorMode();

    /// derives the values for the not-current color representations' channels
    /// by converting between representations
    void deriveMissingColorChannels();

    /// mark Color Light values applied (flags channels applied depending on colormode)
    void appliedColorValues();

    /// step through transitions
    /// @param aStepSize how much to step. Default is zero and means starting transition
    /// @return true if there's another step to take, false if end of transition already reached
    bool colorTransitionStep(double aStepSize = 0);

    /// @}

    /// check for presence of model feature (flag in dSS visibility matrix)
    /// @param aFeatureIndex the feature to check for
    /// @return yes if this output behaviour has the feature, no if (explicitly) not, undefined if asked entity does not know
    virtual Tristate hasModelFeature(DsModelFeatures aFeatureIndex);

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    /// called by applyScene to load channel values from a scene.
    /// @param aScene the scene to load channel values from
    /// @note Scenes don't have 1:1 representation of all channel values for footprint and logic reasons, so this method
    ///   is implemented in the specific behaviours according to the scene layout for that behaviour.
    virtual void loadChannelsFromScene(DsScenePtr aScene);

    /// called by captureScene to save channel values to a scene.
    /// @param aScene the scene to save channel values to
    /// @note Scenes don't have 1:1 representation of all channel values for footprint and logic reasons, so this method
    ///   is implemented in the specific behaviours according to the scene layout for that behaviour.
    /// @note call markDirty on aScene in case it is changed (otherwise captured values will not be saved)
    virtual void saveChannelsToScene(DsScenePtr aScene);


    /// utility function, adjusts channel-level dontCare flags to current color mode
    void adjustChannelDontCareToColorMode(ColorLightScenePtr aColorLightScene);

  };

  typedef boost::intrusive_ptr<ColorLightBehaviour> ColorLightBehaviourPtr;



  class RGBColorLightBehaviour : public ColorLightBehaviour
  {
    typedef ColorLightBehaviour inherited;

  public:

    /// @name settings (color calibration)
    /// @{
    Matrix3x3 calibration; ///< calibration matrix: [[Xr,Xg,Xb],[Yr,Yg,Yb],[Zr,Zg,Zb]]
    Row3 whiteRGB; ///< R,G,B relative intensities that can be replaced by a extra white channel
    Row3 amberRGB; ///< R,G,B relative intensities that can be replaced by a extra amber channel
    /// @}

    RGBColorLightBehaviour(Device &aDevice);

    /// device type identifier
    /// @return constant identifier for this type of behaviour
    virtual const char *behaviourTypeIdentifier() { return "rgblight"; };

    /// @name color services for implementing color lights
    /// @{

    /// @param aPWM will receive the PWM value corresponding to current brightness from 0..aMax
    /// @param aMax max PWM duty cycle value
    /// @param aPWM PWM value to be converted back to brightness
    /// @param aMax max PWM duty cycle value


    /// get RGB colors (from current channel settings, HSV, CIE, CT + brightness) for applying to lamp
    /// @param aRed,aGreen,aBlue will receive the R,G,B values corresponding to current channels
    /// @param aMax max value for aRed,aGreen,aBlue
    void getRGB(double &aRed, double &aGreen, double &aBlue, double aMax);

    /// set RGB values from lamp (to update channel values from actual lamp setting)
    /// @param aRed,aGreen,aBlue current R,G,B values to be converted to color channel settings
    /// @param aMax max value for aRed,aGreen,aBlue
    void setRGB(double aRed, double aGreen, double aBlue, double aMax);

    /// get RGB colors (from current channel settings, HSV, CIE, CT + brightness) for applying to lamp
    /// @param aRed,aGreen,aBlue,aWhite will receive the R,G,B,W values corresponding to current channels
    /// @param aMax max value for aRed,aGreen,aBlue,aWhite
    void getRGBW(double &aRed, double &aGreen, double &aBlue, double &aWhite, double aMax);

    /// set RGB values from lamp (to update channel values from actual lamp setting)
    /// @param aRed,aGreen,aBlue,aWhite current R,G,B,W values to be converted to color channel settings
    /// @param aMax max value for aRed,aGreen,aBlue,aWhite
    void setRGBW(double aRed, double aGreen, double aBlue, double aWhite, double aMax);

    /// get RGB colors (from current channel settings, HSV, CIE, CT + brightness) for applying to lamp
    /// @param aRed,aGreen,aBlue,aWhite,aAmber will receive the R,G,B,W,A values corresponding to current channels
    /// @param aMax max value for aRed,aGreen,aBlue,aWhite,aAmber
    void getRGBWA(double &aRed, double &aGreen, double &aBlue, double &aWhite, double &aAmber, double aMax);

    /// @}

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    // property access implementation for descriptor/settings/states
    virtual int numSettingsProps();
    virtual const PropertyDescriptorPtr getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor);
    // combined field access for all types of properties
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    // persistence implementation
    virtual const char *tableName();
    virtual size_t numFieldDefs();
    virtual const FieldDefinition *getFieldDef(size_t aIndex);
    virtual void loadFromRow(sqlite3pp::query::iterator &aRow, int &aIndex, uint64_t *aCommonFlagsP);
    virtual void bindToStatement(sqlite3pp::statement &aStatement, int &aIndex, const char *aParentIdentifier, uint64_t aCommonFlags);

  };

  typedef boost::intrusive_ptr<RGBColorLightBehaviour> RGBColorLightBehaviourPtr;

} // namespace p44

#endif /* defined(__p44vdc__colorlightbehaviour__) */
