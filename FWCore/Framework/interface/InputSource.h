#ifndef FWCore_Framework_InputSource_h
#define FWCore_Framework_InputSource_h

/*----------------------------------------------------------------------

InputSource: Abstract interface for all input sources.

Some examples of InputSource subclasses are:

 1) PoolSource: Handles things related to reading from an EDM/ROOT file.
 This source provides for delayed loading of data.
 2) EmptySource: Handles similar tasks for the case where there is no
 data in the input.

----------------------------------------------------------------------*/

#include "DataFormats/Provenance/interface/LuminosityBlockAuxiliary.h"
#include "DataFormats/Provenance/interface/LuminosityBlockID.h"
#include "DataFormats/Provenance/interface/ModuleDescription.h"
#include "DataFormats/Provenance/interface/RunAuxiliary.h"
#include "DataFormats/Provenance/interface/RunID.h"
#include "DataFormats/Provenance/interface/Timestamp.h"
#include "DataFormats/Provenance/interface/ProductRegistry.h"
#include "FWCore/Common/interface/FWCoreCommonFwd.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/ProcessingController.h"
#include "FWCore/Utilities/interface/LuminosityBlockIndex.h"
#include "FWCore/Utilities/interface/RunIndex.h"
#include "FWCore/Utilities/interface/Signal.h"
#include "FWCore/Utilities/interface/get_underlying_safe.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include <memory>
#include <string>
#include <chrono>
#include <mutex>

namespace edm {
  class ActivityRegistry;
  class BranchIDListHelper;
  class ConfigurationDescriptions;
  class HistoryAppender;
  class ParameterSet;
  class ParameterSetDescription;
  class ProcessContext;
  class ProcessHistoryRegistry;
  class StreamContext;
  class ModuleCallingContext;
  class SharedResourcesAcquirer;
  class ThinnedAssociationsHelper;

  class InputSource {
  public:
    enum class ItemType : char { IsInvalid, IsStop, IsFile, IsRun, IsLumi, IsEvent, IsRepeat, IsSynchronize };
    enum class ItemPosition : char { Invalid, LastItemToBeMerged, NotLastItemToBeMerged };

    class ItemTypeInfo {
    public:
      constexpr ItemTypeInfo(ItemType type = ItemType::IsInvalid, ItemPosition position = ItemPosition::Invalid)
          : type_(type), position_(position) {}
      ItemType itemType() const { return type_; }
      ItemPosition itemPosition() const { return position_; }

      // Note that conversion to ItemType is defined and often used to
      // compare an ItemTypeInfo with an ItemType.
      // operator== of two ItemTypeInfo's is intentionally NOT defined.
      // The constructor also allows implicit conversion from ItemType and
      // often assignment from ItemType to ItemTypeInfo occurs.
      operator ItemType() const { return type_; }

    private:
      ItemType type_;

      // position_ should always be Invalid if the itemType_ is not IsRun or IsLumi.
      // Even for runs and lumis, it is OK to leave it Invalid because the
      // Framework can figure this out based on the next item. Offline it is
      // simplest to always leave it Invalid. For online sources, there are
      // optimizations that the Framework can use when it knows that a run or
      // lumi is the last to be merged before the following item is known. This
      // is useful in cases where the function named getNextItemType
      // might take a long time to return.
      ItemPosition position_;
    };

    enum ProcessingMode { Runs, RunsAndLumis, RunsLumisAndEvents };

    /// Constructor
    explicit InputSource(ParameterSet const&, InputSourceDescription const&);

    /// Destructor
    virtual ~InputSource() noexcept(false);

    InputSource(InputSource const&) = delete;             // Disallow copying and moving
    InputSource& operator=(InputSource const&) = delete;  // Disallow copying and moving

    static void fillDescriptions(ConfigurationDescriptions& descriptions);
    static const std::string& baseType();
    static void fillDescription(ParameterSetDescription& desc);
    static void prevalidate(ConfigurationDescriptions&);

    /// Advances the source to the next item
    ItemTypeInfo nextItemType();

    /// Read next event
    void readEvent(EventPrincipal& ep, StreamContext&);

    /// Read a specific event
    bool readEvent(EventPrincipal& ep, EventID const&, StreamContext&);

    /// Read next luminosity block Auxilary
    std::shared_ptr<LuminosityBlockAuxiliary> readLuminosityBlockAuxiliary();

    /// Read next run Auxiliary
    std::shared_ptr<RunAuxiliary> readRunAuxiliary();

    /// Read next run (new run)
    void readRun(RunPrincipal& runPrincipal, HistoryAppender& historyAppender);

    /// Read next run (same as a prior run)
    void readAndMergeRun(RunPrincipal& rp);

    /// Read next luminosity block (new lumi)
    void readLuminosityBlock(LuminosityBlockPrincipal& lumiPrincipal, HistoryAppender& historyAppender);

    /// Read next luminosity block (same as a prior lumi)
    void readAndMergeLumi(LuminosityBlockPrincipal& lbp);

    /// Fill the ProcessBlockHelper with info for the current file
    void fillProcessBlockHelper();

    /// Next process block, return false if there is none, sets the processName in the principal
    bool nextProcessBlock(ProcessBlockPrincipal&);

    /// Read next process block
    void readProcessBlock(ProcessBlockPrincipal&);

    /// Read next file
    std::shared_ptr<FileBlock> readFile();

    /// close current file
    void closeFile(FileBlock*, bool cleaningUpAfterException);

    /// Skip the number of events specified.
    /// Offset may be negative.
    void skipEvents(int offset);

    bool goToEvent(EventID const& eventID);

    /// Begin again at the first event
    void rewind();

    /// Set the run number
    void setRunNumber(RunNumber_t r) { setRun(r); }

    /// Set the luminosity block ID
    void setLuminosityBlockNumber_t(LuminosityBlockNumber_t lb) { setLumi(lb); }

    /// issue an event report
    void issueReports(EventID const& eventID, StreamID streamID);

    /// Register any produced products into source's registry
    virtual void registerProducts();

    /// Accessors for product registry
    ProductRegistry const& productRegistry() const { return productRegistry_; }

    /// Accessors for process history registry.
    ProcessHistoryRegistry const& processHistoryRegistry() const { return *processHistoryRegistry_; }
    ProcessHistoryRegistry& processHistoryRegistry() { return *processHistoryRegistry_; }

    /// Accessors for branchIDListHelper
    std::shared_ptr<BranchIDListHelper const> branchIDListHelper() const {
      return get_underlying_safe(branchIDListHelper_);
    }
    std::shared_ptr<BranchIDListHelper>& branchIDListHelper() { return get_underlying_safe(branchIDListHelper_); }

    /// Accessors for processBlockHelper
    std::shared_ptr<ProcessBlockHelper const> processBlockHelper() const {
      return get_underlying_safe(processBlockHelper_);
    }
    std::shared_ptr<ProcessBlockHelper>& processBlockHelper() { return get_underlying_safe(processBlockHelper_); }

    /// Accessors for thinnedAssociationsHelper
    std::shared_ptr<ThinnedAssociationsHelper const> thinnedAssociationsHelper() const {
      return get_underlying_safe(thinnedAssociationsHelper_);
    }
    std::shared_ptr<ThinnedAssociationsHelper>& thinnedAssociationsHelper() {
      return get_underlying_safe(thinnedAssociationsHelper_);
    }

    /// Reset the remaining number of events/lumis to the maximum number.
    void repeat() {
      remainingEvents_ = maxEvents_;
      remainingLumis_ = maxLumis_;
    }

    /// Returns nullptr if no resource shared between the Source and a DelayedReader
    std::pair<SharedResourcesAcquirer*, std::recursive_mutex*> resourceSharedWithDelayedReader();

    /// Accessor for maximum number of events to be read.
    /// -1 is used for unlimited.
    int maxEvents() const { return maxEvents_; }

    /// Accessor for remaining number of events to be read.
    /// -1 is used for unlimited.
    int remainingEvents() const { return remainingEvents_; }

    /// Accessor for maximum number of lumis to be read.
    /// -1 is used for unlimited.
    int maxLuminosityBlocks() const { return maxLumis_; }

    /// Accessor for remaining number of lumis to be read.
    /// -1 is used for unlimited.
    int remainingLuminosityBlocks() const { return remainingLumis_; }

    /// Accessor for 'module' description.
    ModuleDescription const& moduleDescription() const { return moduleDescription_; }

    /// Accessor for Process Configuration
    ProcessConfiguration const& processConfiguration() const { return moduleDescription().processConfiguration(); }

    /// Accessor for global process identifier
    std::string const& processGUID() const { return processGUID_; }

    /// Called by framework at beginning of job. The argument is the full product registry
    void doBeginJob(edm::ProductRegistry const&);

    /// Called by framework at end of job
    void doEndJob();

    /// Called by framework at beginning of lumi block
    virtual void doBeginLumi(LuminosityBlockPrincipal& lbp, ProcessContext const*);

    /// Called by framework at beginning of run
    virtual void doBeginRun(RunPrincipal& rp, ProcessContext const*);

    /// Accessor for the current time, as seen by the input source
    Timestamp const& timestamp() const { return time_; }

    /// Accessor for the reduced process history ID of the current run.
    /// This is the ID of the input process history which does not include
    /// the current process.
    ProcessHistoryID const& reducedProcessHistoryID() const;

    /// Accessor for current run number
    RunNumber_t run() const;

    /// Accessor for current luminosity block number
    LuminosityBlockNumber_t luminosityBlock() const;

    /// RunsLumisAndEvents (default), RunsAndLumis, or Runs.
    ProcessingMode processingMode() const { return processingMode_; }

    /// Accessor for Activity Registry
    std::shared_ptr<ActivityRegistry> actReg() const { return actReg_; }

    /// Called by the framework to merge or insert run in principal cache.
    std::shared_ptr<RunAuxiliary> runAuxiliary() const { return runAuxiliary_; }

    /// Called by the framework to merge or insert lumi in principal cache.
    std::shared_ptr<LuminosityBlockAuxiliary> luminosityBlockAuxiliary() const { return lumiAuxiliary_; }

    bool randomAccess() const;
    ProcessingController::ForwardState forwardState() const;
    ProcessingController::ReverseState reverseState() const;

    class EventSourceSentry {
    public:
      EventSourceSentry(InputSource const& source, StreamContext& sc);
      ~EventSourceSentry();

      EventSourceSentry(EventSourceSentry const&) = delete;             // Disallow copying and moving
      EventSourceSentry& operator=(EventSourceSentry const&) = delete;  // Disallow copying and moving

    private:
      InputSource const& source_;
      StreamContext& sc_;
    };

    class LumiSourceSentry {
    public:
      LumiSourceSentry(InputSource const& source, LuminosityBlockIndex id);
      ~LumiSourceSentry();

      LumiSourceSentry(LumiSourceSentry const&) = delete;             // Disallow copying and moving
      LumiSourceSentry& operator=(LumiSourceSentry const&) = delete;  // Disallow copying and moving

    private:
      InputSource const& source_;
      LuminosityBlockIndex index_;
    };

    class RunSourceSentry {
    public:
      RunSourceSentry(InputSource const& source, RunIndex id);
      ~RunSourceSentry();

      RunSourceSentry(RunSourceSentry const&) = delete;             // Disallow copying and moving
      RunSourceSentry& operator=(RunSourceSentry const&) = delete;  // Disallow copying and moving

    private:
      InputSource const& source_;
      RunIndex index_;
    };

    class ProcessBlockSourceSentry {
    public:
      ProcessBlockSourceSentry(InputSource const&, std::string const&);
      ~ProcessBlockSourceSentry();

      ProcessBlockSourceSentry(ProcessBlockSourceSentry const&) = delete;             // Disallow copying and moving
      ProcessBlockSourceSentry& operator=(ProcessBlockSourceSentry const&) = delete;  // Disallow copying and moving

    private:
      InputSource const& source_;
      std::string const& processName_;
    };

    class FileOpenSentry {
    public:
      typedef signalslot::Signal<void(std::string const&)> Sig;
      explicit FileOpenSentry(InputSource const& source, std::string const& lfn);
      ~FileOpenSentry();

      FileOpenSentry(FileOpenSentry const&) = delete;             // Disallow copying and moving
      FileOpenSentry& operator=(FileOpenSentry const&) = delete;  // Disallow copying and moving

    private:
      Sig& post_;
      std::string const& lfn_;
    };

    class FileCloseSentry {
    public:
      typedef signalslot::Signal<void(std::string const&)> Sig;
      explicit FileCloseSentry(InputSource const& source, std::string const& lfn);
      ~FileCloseSentry();

      FileCloseSentry(FileCloseSentry const&) = delete;             // Disallow copying and moving
      FileCloseSentry& operator=(FileCloseSentry const&) = delete;  // Disallow copying and moving

    private:
      Sig& post_;
      std::string const& lfn_;
    };

    signalslot::Signal<void(StreamContext const&, ModuleCallingContext const&)> preEventReadFromSourceSignal_;
    signalslot::Signal<void(StreamContext const&, ModuleCallingContext const&)> postEventReadFromSourceSignal_;

  protected:
    virtual void skip(int offset);

    /// To set the current time, as seen by the input source
    void setTimestamp(Timestamp const& theTime) { time_ = theTime; }

    ProductRegistry& productRegistryUpdate() { return productRegistry_; }
    ProcessHistoryRegistry& processHistoryRegistryForUpdate() { return *processHistoryRegistry_; }
    ItemTypeInfo state() const { return state_; }
    void setRunAuxiliary(RunAuxiliary* rp) {
      runAuxiliary_.reset(rp);
      newRun_ = newLumi_ = true;
    }
    void setLuminosityBlockAuxiliary(LuminosityBlockAuxiliary* lbp) {
      lumiAuxiliary_.reset(lbp);
      newLumi_ = true;
    }
    void resetRunAuxiliary(bool isNewRun = true) const {
      runAuxiliary_.reset();
      newRun_ = newLumi_ = isNewRun;
    }
    void resetLuminosityBlockAuxiliary(bool isNewLumi = true) const {
      lumiAuxiliary_.reset();
      newLumi_ = isNewLumi;
    }
    void reset() const {
      resetLuminosityBlockAuxiliary();
      resetRunAuxiliary();
      state_ = ItemTypeInfo();
    }
    bool newRun() const { return newRun_; }
    void setNewRun() { newRun_ = true; }
    void resetNewRun() { newRun_ = false; }
    bool newLumi() const { return newLumi_; }
    void setNewLumi() { newLumi_ = true; }
    void resetNewLumi() { newLumi_ = false; }
    bool eventCached() const { return eventCached_; }
    /// Called by the framework to merge or ached() const {return eventCached_;}
    void setEventCached() { eventCached_ = true; }
    void resetEventCached() { eventCached_ = false; }

    ///Called by inheriting classes when running multicore when the receiver has told them to
    /// skip some events.
    void decreaseRemainingEventsBy(int iSkipped);

    ///Begin protected makes it easier to do template programming
    virtual void beginJob(edm::ProductRegistry const&);

  private:
    bool eventLimitReached() const { return remainingEvents_ == 0; }
    bool lumiLimitReached() const {
      if (remainingLumis_ == 0) {
        return true;
      }
      if (maxSecondsUntilRampdown_ <= 0) {
        return false;
      }
      auto end = std::chrono::steady_clock::now();
      auto elapsed = end - processingStart_;
      if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > maxSecondsUntilRampdown_) {
        return true;
      }
      return false;
    }
    bool limitReached() const { return eventLimitReached() || lumiLimitReached(); }
    virtual ItemTypeInfo getNextItemType() = 0;
    ItemTypeInfo nextItemType_();
    virtual std::shared_ptr<RunAuxiliary> readRunAuxiliary_() = 0;
    virtual std::shared_ptr<LuminosityBlockAuxiliary> readLuminosityBlockAuxiliary_() = 0;
    virtual void fillProcessBlockHelper_();
    virtual bool nextProcessBlock_(ProcessBlockPrincipal&);
    virtual void readProcessBlock_(ProcessBlockPrincipal&);
    virtual void readRun_(RunPrincipal& runPrincipal);
    virtual void readLuminosityBlock_(LuminosityBlockPrincipal& lumiPrincipal);
    virtual void readEvent_(EventPrincipal& eventPrincipal) = 0;
    virtual bool readIt(EventID const& id, EventPrincipal& eventPrincipal, StreamContext& streamContext);
    virtual std::shared_ptr<FileBlock> readFile_();
    virtual void closeFile_() {}
    virtual bool goToEvent_(EventID const& eventID);
    virtual void setRun(RunNumber_t r);
    virtual void setLumi(LuminosityBlockNumber_t lb);
    virtual void rewind_();
    virtual void endJob();
    virtual std::pair<SharedResourcesAcquirer*, std::recursive_mutex*> resourceSharedWithDelayedReader_();

    virtual bool randomAccess_() const;
    virtual ProcessingController::ForwardState forwardState_() const;
    virtual ProcessingController::ReverseState reverseState_() const;

  private:
    std::shared_ptr<ActivityRegistry> actReg_;  // We do not use propagate_const because the registry itself is mutable.
    int maxEvents_;
    int remainingEvents_;
    int maxLumis_;
    int remainingLumis_;
    int readCount_;
    int maxSecondsUntilRampdown_;
    std::chrono::time_point<std::chrono::steady_clock> processingStart_;
    ProcessingMode processingMode_;
    ModuleDescription const moduleDescription_;
    ProductRegistry productRegistry_;
    edm::propagate_const<std::unique_ptr<ProcessHistoryRegistry>> processHistoryRegistry_;
    edm::propagate_const<std::shared_ptr<BranchIDListHelper>> branchIDListHelper_;
    edm::propagate_const<std::shared_ptr<ProcessBlockHelper>> processBlockHelper_;
    edm::propagate_const<std::shared_ptr<ThinnedAssociationsHelper>> thinnedAssociationsHelper_;
    std::string processGUID_;
    Timestamp time_;
    mutable bool newRun_;
    mutable bool newLumi_;
    bool eventCached_;
    mutable ItemTypeInfo state_;
    mutable std::shared_ptr<RunAuxiliary> runAuxiliary_;
    mutable std::shared_ptr<LuminosityBlockAuxiliary> lumiAuxiliary_;
    std::string statusFileName_;

    unsigned int numberOfEventsBeforeBigSkip_;
  };
}  // namespace edm

#endif
