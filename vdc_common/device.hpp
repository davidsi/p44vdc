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

#ifndef __p44vdc__device__
#define __p44vdc__device__

#include "dsbehaviour.hpp"

#include "dsscene.hpp"

using namespace std;

// originally, all behaviours were accessed by index, channels were accessed by channeltype
// when ACCESS_BY_ID is set, all behaviours and channels are accessed by their getId().
#define ACCESS_BY_ID 1

namespace p44 {

  class Device;
  typedef boost::intrusive_ptr<Device> DevicePtr;

  class ChannelBehaviour;
  typedef boost::intrusive_ptr<ChannelBehaviour> ChannelBehaviourPtr;

  typedef vector<DsBehaviourPtr> BehaviourVector;

  typedef boost::intrusive_ptr<OutputBehaviour> OutputBehaviourPtr;

  /// base class representing a virtual digitalSTROM device.
  /// For each type of subsystem (EnOcean, DALI, ...) this class is subclassed to implement
  /// the vDC' specifics, in particular the interface with the hardware.
  class Device : public DsAddressable
  {
    typedef DsAddressable inherited;

    friend class VdcHost;
    friend class VdcCollector;
    friend class DsBehaviour;
    friend class DsScene;
    friend class SceneChannels;
    friend class SceneDeviceSettings;
    friend class ButtonBehaviour;

  protected:

    /// the class container
    Vdc *vdcP;

    /// @name behaviours
    /// @{
    BehaviourVector buttons; ///< buttons and switches (user interaction)
    BehaviourVector binaryInputs; ///< binary inputs (not for user interaction)
    BehaviourVector sensors; ///< sensors (measurements)
    OutputBehaviourPtr output; ///< the output (if any)
    /// @}

    /// device global parameters (for all behaviours), in particular the scene table
    /// @note devices assign this with a derived class which is specialized
    ///   for the device type and, if needed, proper type of scenes (light, blinds, RGB light etc. have different scene tables)
    DeviceSettingsPtr deviceSettings;

    // volatile r/w properties
    bool progMode; ///< if set, device is in programming mode
    DsScenePtr previousState; ///< a pseudo scene which holds the device state before the last applyScene() call, used to do undoScene()

    // variables set by concrete devices (=hardware dependent)
    DsClass colorClass; ///< basic color of the device (can be black)

    // volatile internal state
    long dimTimeoutTicket; ///< for timing out dimming operations (autostop when no INC/DEC is received)
    VdcDimMode currentDimMode; ///< current dimming in progress
    DsChannelType currentDimChannel; ///< currently dimmed channel (if dimming in progress)
    long dimHandlerTicket; ///< for standard dimming
    bool isDimming; ///< if set, dimming is in progress
    uint8_t areaDimmed; ///< last dimmed area (so continue know which dimming command to re-start in case it comes late)
    VdcDimMode areaDimMode; ///< last area dim mode

    // hardware access serializer/pacer
    SimpleCB appliedOrSupersededCB; ///< will be called when values are either applied or ignored because a subsequent change is already pending
    SimpleCB applyCompleteCB; ///< will be called when apply is complete (set by waitForApplyComplete())
    bool applyInProgress; ///< set when applying values is in progress
    int missedApplyAttempts; ///< number of apply attempts that could not be executed. If>0, completing next apply will trigger a re-apply to finalize values
    SimpleCB updatedOrCachedCB; ///< will be called when current values are either read from hardware, or new values have been requested for applying
    bool updateInProgress; ///< set when updating channel values from hardware is in progress
    long serializerWatchdogTicket; ///< watchdog terminating non-responding hardware requests

  public:


    /// @name Device lifecycle management
    /// @{

    /// constructor
    /// @note the constructor only needs to do very basic initialisation. The process of querying device details up to the
    ///   point where it knows its own dSUID is handled in identifyDevice(), which can also swap a placeholder base class device
    ///   by a more specialized subclass if needed, AFTER accessing some device APIs etc. So the constructor must make the
    ///   device ready for identifyDevice(), but nothing else.
    Device(Vdc *aVdcP);

    /// identify a device up to the point that it knows its dSUID and internal structure. Possibly swap device object for a more specialized subclass.
    /// @param aIdentifyCB must be called when the identification or setup is not instant, but can only be confirmed later. In this
    ///   case, identifyDevice() must return false, indicating the identification is not yet complete.
    ///   This is useful for devices that require API calls to determine device models, serial numbers etc.)
    ///   At the time when the callback is called, the device must have a stable dSUID and internal setup.
    /// @return true can be returned when the device identification/setup is instant. This means that aIdentifyCB is not called, and
    ///   the device is immediately used. When returning true, the device must already have a stable dSUID and internal setup.
    ///   In a vdc which uses simpleIdentifyAndAddDevice(), returning false will cause the device not be added but immediately deleted. 
    /// @note identifyDevice() will only be called on a new Device object during the process of adding a device to a vDC.
    /// @note identifyDevice() MAY NOT perform any action on the (hardware) device that would modify its state. When re-scanning
    ///   for hardware devices, identifyDevice() will often target already known and registered devices. identifyDevice()'s only
    ///   allowed interaction with the hardware device is to query enough information to derive a henceforth invariable dSUID and
    ///   and to construct a suitable Device object, including all contained settings, scene and behaviour objects.
    ///   This object is then *possibly* used to operate the device later (in this case it will receive load() and initializeDevice() calls),
    //    but also might get discarded without further calls when it turns out there is already a Device with the same dSUID.
    /// @note When idenify finishes, the device must be ready to load persistent settings (which means all behaviours and settings objects
    ///   need to be in place, as these define the device structure to load settings for), and then for being initialized for operation.
    virtual bool identifyDevice(IdentifyDeviceCB aIdentifyCB) = 0;

    /// utility: report identification status
    void identificationDone(IdentifyDeviceCB aIdentifyCB, ErrorPtr aError, Device *aActualDevice = NULL);

    /// utility: return from identification with error
    void identificationFailed(IdentifyDeviceCB aIdentifyCB, ErrorPtr aError);

    /// utility: confirm identification
    void identificationOK(IdentifyDeviceCB aIdentifyCB, Device *aActualDevice = NULL);



    /// load parameters from persistent DB
    /// @note this is usually called from the device container when device is added (detected), before initializeDevice() and after identifyDevice()
    virtual ErrorPtr load();

    // load additional settings from files
    void loadSettingsFromFiles();

    /// initializes the physical device for being used
    /// @param aFactoryReset if set, the device will be inititalized as thoroughly as possible (factory reset, default settings etc.)
    /// @note this is called after persistent settings have been loaded
    /// @note this is called before interaction with dS system starts
    /// @note implementation should call inherited when complete, so superclasses could chain further activity
    virtual void initializeDevice(StatusCB aCompletedCB, bool aFactoryReset) { aCompletedCB(ErrorPtr()); /* NOP in base class */ };

    /// save unsaved parameters to persistent DB
    /// @note this is usually called from the device container in regular intervals
    virtual ErrorPtr save();

    /// forget any parameters stored in persistent DB
    virtual ErrorPtr forget();

    // check if any settings are dirty
    virtual bool isDirty();

    // make all settings clean (not to be saved to DB)
    virtual void markClean();

    /// callback for disconnect()
    /// @param aDisconnected returns true if device could be disconnected, false if disconnection by software is not possible
    ///   in general or could not be performed in the current operational state of the device
    typedef boost::function<void (bool aDisconnected)> DisconnectCB;

    /// disconnect device. If presence is represented by data stored in the vDC rather than
    /// detection of real physical presence on a bus, this call must clear the data that marks
    /// the device as connected to this vDC (such as a learned-in EnOcean button).
    /// For devices where the vDC can be *absolutely certain* that they are still connected
    /// to the vDC AND cannot possibly be connected to another vDC as well, this call should
    /// return false.
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDisconnectResultHandler will be called to report true if device could be disconnected,
    ///   false in case it is certain that the device is still connected to this and only this vDC
    /// @note at the time aDisconnectResultHandler is called, the only owner left for the device object might be the
    ///   aDevice argument to the DisconnectCB handler.
    virtual void disconnect(bool aForgetParams, DisconnectCB aDisconnectResultHandler);

    /// destructor
    virtual ~Device();

    /// @}


    /// @name identification and invariable properties of the device (can be overriden in subclasses)
    /// @{

    /// device type identifier
		/// @return constant identifier for this type of device (one container might contain more than one type)
    virtual string deviceTypeIdentifier() const { return "unspecified"; };

    /// @return human readable model name/short description
    virtual string modelName() P44_OVERRIDE { return "vdSD - virtual device"; }

    /// @return unique ID for the functional model of this entity
    /// @note this is usually created as a hash including the relevant vDC-API visible representation of the device 
    virtual string modelUID() P44_OVERRIDE;

    /// device class (for grouping functionally equivalent single devices)
    /// @note usually, only single devices do have a deviceClass
    /// @return name of the device class, such as "washingmachine" or "kettle" or "oven". Empty string if no device class exists.
    virtual string deviceClass() { return ""; }

    /// device class version number.
    /// @note This allows different versions of the functional representation of the device class
    ///   to coexist in a system.
    /// @return version or 0 if no version exists
    virtual uint32_t deviceClassVersion() { return 0; }


    /// @return the entity type (one of dSD|vdSD|vDC|dSM|vdSM|dSS|*)
    virtual const char *entityType() P44_OVERRIDE { return "vdSD"; }

    /// @return Vendor name for display purposes
    /// @note if not empty, value will be used by vendorId() default implementation to create vendorname:xxx URN schema id
    virtual string vendorName() P44_OVERRIDE;

    /// Get icon data or name
    /// @param aIcon string to put result into (when method returns true)
    /// - if aWithData is set, binary PNG icon data for given resolution prefix is returned
    /// - if aWithData is not set, only the icon name (without file extension) is returned
    /// @param aWithData if set, PNG data is returned, otherwise only name
    /// @return true if there is an icon, false if not
    virtual bool getDeviceIcon(string &aIcon, bool aWithData, const char *aResolutionPrefix) P44_OVERRIDE;

    /// check for presence of model feature (flag in dSS visibility matrix)
    /// @param aFeatureIndex the feature to check for
    /// @return yes if this output behaviour has the feature, no if (explicitly) not, undefined if asked entity does not know
    virtual Tristate hasModelFeature(DsModelFeatures aFeatureIndex);

    /// @}



    /// @name other device level methods
    /// @{

    /// set basic device color
    /// @param aColorClass color group number
    void setColorClass(DsClass aColorClass);

    /// get basic device color group
    /// @return color class number
    DsClass getColorClass() { return colorClass; };

    /// get default class for given group
    /// @param aGroup group number
    /// @return color class number
    static DsClass colorClassFromGroup(DsGroup aGroup);

    /// get dominant class (i.e. the class that should color the icon)
    /// @return color class number
    DsClass getDominantColorClass();

    /// set user assignable name
    /// @param aName name of the addressable entity
    virtual void setName(const string &aName) P44_OVERRIDE;

    /// get reference to vDC host
    VdcHost &getVdcHost() const { return vdcP->getVdcHost(); };

    /// install specific or standard device settings
    /// @param aDeviceSettings specific device settings, if NULL, standard minimal settings will be used
    void installSettings(DeviceSettingsPtr aDeviceSettings = DeviceSettingsPtr());

    /// add a behaviour and set its index
    /// @param aBehaviour a newly created behaviour, will get added to the correct button/binaryInput/sensor/output
    ///   array and given the correct index value.
    void addBehaviour(DsBehaviourPtr aBehaviour);

    /// get scenes
    /// @return NULL if device has no scenes, scene device settings otherwise 
    SceneDeviceSettingsPtr getScenes() { return boost::dynamic_pointer_cast<SceneDeviceSettings>(deviceSettings); };

    /// send a signal needed for some devices to get learned into other devices, or query availability of teach-in signals
    /// @param aVariant -1 to just get number of available teach-in variants. 0..n to send teach-in signal;
    ///   some devices may have different teach-in signals (like: one for ON, one for OFF).
    /// @return number of teach-in signal variants the device can send
    virtual uint8_t teachInSignal(int8_t aVariant) { return 0; /* has no teach-in signals */ };

    /// check if device can be disconnected by software (i.e. Web-UI)
    /// @return true if device might be disconnectable by the user via software (i.e. web UI)
    /// @note devices returning true here might still refuse disconnection on a case by case basis when
    ///   operational state does not allow disconnection.
    /// @note devices returning false here might still be disconnectable using disconnect() triggered
    ///   by vDC API "remove" method.
    virtual bool isSoftwareDisconnectable() { return false; }; // by default, devices cannot be removed via Web-UI

    /// report that device has vanished (disconnected without being told so via vDC API)
    /// This will call disconnect() on the device, and remove it from all vDC container lists
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @note this method should be called when bus scanning or other HW-side events detect disconnection
    ///   of a device, such that it can be reported to the dS system.
    /// @note use scheduleVanish() when the current calling chain might rely on the device to still exist.
    /// @note calling hasVanished() might delete the object, so don't rely on 'this' after calling it unless you
    ///   still hold a DevicePtr to it
    void hasVanished(bool aForgetParams);

    /// same as hasVanished, but will break caller chain first by queueing actual vanish into mainloop
    /// @param aForgetParams if set, not only the connection to the device is removed, but also all parameters related to it
    ///   such that in case the same device is re-connected later, it will not use previous configuration settings, but defaults.
    /// @param aDelay how long to wait until deleting the device
    void scheduleVanish(bool aForgetParams, MLMicroSeconds aDelay = 0);


    /// @}


    /// @name API implementation
    /// @{

    /// called to let device handle device-level methods
    /// @param aMethod the method
    /// @param aRequest the request to be passed to answering methods
    /// @param aParams the parameters object
    /// @return NULL if method implementation has or will take care of sending a reply (but make sure it
    ///   actually does, otherwise API clients will hang or run into timeouts)
    ///   Returning any Error object, even if ErrorOK, will cause a generic response to be returned.
    /// @note the parameters object always contains the dSUID parameter which has been
    ///   used already to route the method call to this device.
    virtual ErrorPtr handleMethod(VdcApiRequestPtr aRequest, const string &aMethod, ApiValuePtr aParams) P44_OVERRIDE;

    /// called to let device handle device-level notification
    /// @param aMethod the notification
    /// @param aParams the parameters object
    /// @note the parameters object always contains the dSUID parameter which has been
    ///   used already to route the notification to this device.
    virtual void handleNotification(const string &aMethod, ApiValuePtr aParams) P44_OVERRIDE;

    /// call scene on this device
    /// @param aSceneNo the scene to call.
    void callScene(SceneNo aSceneNo, bool aForce);

    /// undo scene call on this device (i.e. revert outputs to values present immediately before calling that scene)
    /// @param aSceneNo the scene call to undo (needs to be specified to prevent undoing the wrong scene)
    void undoScene(SceneNo aSceneNo);

    /// save scene on this device
    /// @param aSceneNo the scene to save current state into
    void saveScene(SceneNo aSceneNo);

    /// store updated version of a scene for this device
    /// @param aScene the updated scene object that should be stored
    /// @note only updates the scene if aScene is marked dirty
    void updateSceneIfDirty(DsScenePtr aScene);


    /// start or stop dimming channel of this device.
    /// @param aChannel the channelType to start or stop dimming for
    /// @param aDimMode according to VdcDimMode: 1=start dimming up, -1=start dimming down, 0=stop dimming
    /// @param aArea the area (1..4, 0=room) to restrict dimming to. Can be -1 to override local priority
    /// @param aAutoStopAfter max dimming time, dimming will stop when this time has passed
    /// @note this method is internally used to implement vDC API dimChannel and dim-related scene calls, but
    ///    it is exposed as directly controlling dimming might be useful for other purposes (e.g. identify)
    void dimChannelForArea(DsChannelType aChannel, VdcDimMode aDimMode, int aArea, MLMicroSeconds aAutoStopAfter);


    /// Process a named control value. The type, group membership and settings of the device determine if at all,
    /// and if, how the value affects physical outputs of the device or general device operation
    /// @note if this method adjusts channel values, it must not directly update the hardware, but just
    ///   prepare channel values such that these can be applied using requestApplyingChannels().
    /// @param aName the name of the control value, which describes the purpose
    /// @param aValue the control value to process
    /// @note base class by default forwards the control value to all of its output behaviours.
    /// @return true if value processing caused channel changes so channel values should be applied.
    virtual bool processControlValue(const string &aName, double aValue);

    /// @}


    /// @name high level hardware access
    /// @note these methods provide a level of abstraction for accessing hardware (especially output functionality)
    ///   by providing a generic base implementation for functionality. Only in very specialized cases, subclasses may
    ///   still want to derive these methods to provide device hardware specific optimization.
    ///   However, for normal devices it is recommended NOT to derive these methods, but only the low level access
    ///   methods.
    /// @{

    /// request applying channels changes now, but actual execution might get postponed if hardware is laggy
    /// @param aAppliedOrSupersededCB will called when values are either applied, or applying has been superseded by
    ///   even newer values set requested to be applied.
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    /// @param aModeChange if set, channels will be applied in all cases, even if output mode is set to "disabled".
    ///   This flag is set in calls when output mode has changed
    /// @note this internally calls applyChannelValues() to perform actual work, but serializes the behaviour towards the caller
    ///   such that aAppliedOrSupersededCB of the previous request is always called BEFORE initiating subsequent
    ///   channel updates in the hardware. It also may discard requests (but still calling aAppliedOrSupersededCB) to
    ///   avoid stacking up delayed requests.
    void requestApplyingChannels(SimpleCB aAppliedOrSupersededCB, bool aForDimming, bool aModeChange = false);

    /// request callback when apply is really complete (all pending applies done)
    /// @param aApplyCompleteCB will called when values are applied and no other change is pending
    void waitForApplyComplete(SimpleCB aApplyCompleteCB);

    /// request that channel values are updated by reading them back from the device's hardware
    /// @param aUpdatedOrCachedCB will be called when values are updated with actual hardware values
    ///   or pending values are in process to be applied to the hardware and thus these cached values can be considered current.
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    void requestUpdatingChannels(SimpleCB aUpdatedOrCachedCB);

    /// start or stop dimming channel of this device. Usually implemented in device specific manner in subclasses.
    /// @param aChannelType the channelType to start or stop dimming for
    /// @param aDimMode according to VdcDimMode: 1=start dimming up, -1=start dimming down, 0=stop dimming
    /// @note unlike the vDC API "dimChannel" command, which must be repeated for dimming operations >5sec, this
    ///   method MUST NOT terminate dimming automatically except when reaching the minimum or maximum level
    ///   available for the device. Dimming timeouts are implemented at the device level and cause calling
    ///   dimChannel() with aDimMode=0 when timeout happens.
    /// @note this method can rely on a clean start-stop sequence in all cases, which means it will be called once to
    ///   start a dimming process, and once again to stop it. There are no repeated start commands or missing stops - Device
    ///   class makes sure these cases (which may occur at the vDC API level) are not passed on to dimChannel()
    virtual void dimChannel(DsChannelType aChannelType, VdcDimMode aDimMode);

    /// identify the device to the user
    /// @note for lights, this is usually implemented as a blink operation, but depending on the device type,
    ///   this can be anything.
    /// @note base class delegates this to the output behaviour (if any)
    virtual void identifyToUser();

    /// @}


    /// @name channels
    /// @{

    /// @return number of output channels in this device
    int numChannels();

    /// @return true if any channel needs to be applied to hardware
    bool needsToApplyChannels();

    /// get channel by index
    /// @param aChannelIndex the channel index (0=primary channel, 1..n other channels)
    /// @param aPendingApplyOnly if true, only channels with pending values to be applied are returned
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByIndex(size_t aChannelIndex, bool aPendingApplyOnly = false);

    /// get output index by channelType
    /// @param aChannelType the channel type, can be channeltype_default to get primary/default channel
    /// @return NULL for unknown channel
    ChannelBehaviourPtr getChannelByType(DsChannelType aChannelType, bool aPendingApplyOnly = false);

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description() P44_OVERRIDE;

    /// Get short text for a "first glance" status of the device
    /// @return string, really short, intended to be shown as a narrow column in a device/vdc list
    virtual string getStatusText() P44_OVERRIDE;


  protected:


    /// @name low level hardware access
    /// @note actual hardware specific implementation is in derived methods in subclasses.
    ///   Base class uses these methods to access the hardware in a generic way.
    ///   These methods should never be called directly!
    /// @note base class implementations are NOP
    /// @{

    /// @param aHashedString append model relevant strings to this value for creating modelUID() hash
    virtual void addToModelUIDHash(string &aHashedString);

    /// prepare for calling a scene on the device level
    /// @param aScene the scene that is to be called
    /// @return true if scene preparation is ok and call can continue. If false, no further action will be taken
    /// @note this is called BEFORE scene values are recalled
    virtual bool prepareSceneCall(DsScenePtr aScene);

    /// prepare for applying a scene on the device level
    /// @param aScene the scene that is to be applied
    /// @return true if channel values should be applied, false if not
    /// @note this is called AFTER scene values are already loaded and prepareSceneCall() has already been called, but before
    ///   channels are applied (or not, if method returns false)
    virtual bool prepareSceneApply(DsScenePtr aScene);

    /// perform special scene actions (like flashing) which are independent of dontCare flag.
    /// @param aScene the scene that was called (if not dontCare, applyScene() has already been called)
    /// @param aDoneCB will be called when scene actions have completed (but not necessarily when stopped by stopSceneActions())
    /// @note base class implementation just calls performSceneActions() on output
    /// @note this is called after scene values have been applied already (or as only action if dontCare has prevented applying values)
    virtual void performSceneActions(DsScenePtr aScene, SimpleCB aDoneCB);

    /// abort any currently ongoing scene action
    /// @note base class just calls stopSceneActions() on the output
    virtual void stopSceneActions();


    /// apply all pending channel value updates to the device's hardware
    /// @param aDoneCB will called when values are actually applied, or hardware reports an error/timeout
    /// @param aForDimming hint for implementations to optimize dimming, indicating that change is only an increment/decrement
    ///   in a single channel (and not switching between color modes etc.)
    /// @note this is the only routine that should trigger actual changes in output values. It must consult all of the device's
    ///   ChannelBehaviours and check isChannelUpdatePending(), and send new values to the device hardware. After successfully
    ///   updating the device hardware, channelValueApplied() must be called on the channels that had isChannelUpdatePending().
    ///   In addition, if the device output hardware has distinct disabled/enabled states, output->isEnabled() must be checked and applied.
    /// @note the implementation must capture the channel values to be applied before returning from this method call,
    ///   because channel values might change again before a delayed apply mechanism calls aDoneCB.
    /// @note this method will NOT be called again until aCompletedCB is called, even if that takes a long time.
    ///   Device::requestApplyingChannels() provides an implementation that serializes calls to applyChannelValues and syncChannelValues
    virtual void applyChannelValues(SimpleCB aDoneCB, bool aForDimming) { if (aDoneCB) aDoneCB(); /* just call completed in base class */ };

    /// synchronize channel values by reading them back from the device's hardware (if possible)
    /// @param aDoneCB will be called when values are updated with actual hardware values
    /// @note this method is only called at startup and before saving scenes to make sure changes done to the outputs directly (e.g. using
    ///   a direct remote control for a lamp) are included. Just reading a channel state does not call this method.
    /// @note implementation must use channel's syncChannelValue() method
    virtual void syncChannelValues(SimpleCB aDoneCB) { if (aDoneCB) aDoneCB(); /* assume caches up-to-date */ };
    
    /// @}


    // property access implementation
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor) P44_OVERRIDE;
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor) P44_OVERRIDE;
    virtual PropertyDescriptorPtr getDescriptorByName(string aPropMatch, int &aStartIndex, int aDomain, PropertyAccessMode aMode, PropertyDescriptorPtr aParentDescriptor) P44_OVERRIDE;
    virtual PropertyContainerPtr getContainer(const PropertyDescriptorPtr &aPropertyDescriptor, int &aDomain) P44_OVERRIDE;
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor) P44_OVERRIDE;
    virtual ErrorPtr writtenProperty(PropertyAccessMode aMode, PropertyDescriptorPtr aPropertyDescriptor, int aDomain, PropertyContainerPtr aContainer) P44_OVERRIDE;

    /// set local priority of the device if specified scene does not have dontCare set.
    /// @param aSceneNo the scene to check don't care for
    void setLocalPriority(SceneNo aSceneNo);

    /// switch outputs on that are off, and set minmum (logical) output value
    /// @param aSceneNo the scene to check don't care for
    void callSceneMin(SceneNo aSceneNo);


  private:

    DsGroupMask behaviourGroups();

    void dimAutostopHandler(DsChannelType aChannel);
    void dimHandler(ChannelBehaviourPtr aChannel, double aIncrement, MLMicroSeconds aNow);
    void dimDoneHandler(ChannelBehaviourPtr aChannel, double aIncrement, MLMicroSeconds aNextDimAt);
    void outputSceneValueSaved(DsScenePtr aScene);
    void outputUndoStateSaved(DsBehaviourPtr aOutput, DsScenePtr aScene);
    void sceneValuesApplied(DsScenePtr aScene);
    void sceneActionsComplete(DsScenePtr aScene);

    void applyingChannelsComplete();
    void updatingChannelsComplete();
    void serializerWatchdog();
    bool checkForReapply();
    void forkDoneCB(SimpleCB aOriginalCB, SimpleCB aNewCallback);

  };


} // namespace p44


#endif /* defined(__p44vdc__device__) */
