#include <cassert>
#include <limits>
#include "common_includes.h"


using namespace std;


void scanTrees(TString strSampleSet, TString period, TString prodVersion){
  SampleHelpers::configure(period, "hadoop:"+prodVersion);

  std::vector<TString> validDataPeriods = SampleHelpers::getValidDataPeriods();
  size_t nValidDataPeriods = validDataPeriods.size();

  // Get handlers
  // Some of these are needed only to enable the branches
  SimEventHandler simEventHandler;
  GenInfoHandler genInfoHandler;
  EventFilterHandler eventFilter;
  PFCandidateHandler pfcandidateHandler;
  MuonHandler muonHandler;
  ElectronHandler electronHandler;
  PhotonHandler photonHandler;
  SuperclusterHandler superclusterHandler;
  FSRHandler fsrHandler;
  JetMETHandler jetHandler;
  IsotrackHandler isotrackHandler;
  VertexHandler vertexHandler;
  ParticleDisambiguator particleDisambiguator;

  OverlapMapHandler<MuonObject, AK4JetObject> overlapMap_muons_ak4jets;
  OverlapMapHandler<MuonObject, AK8JetObject> overlapMap_muons_ak8jets;
  OverlapMapHandler<ElectronObject, AK4JetObject> overlapMap_electrons_ak4jets;
  OverlapMapHandler<ElectronObject, AK8JetObject> overlapMap_electrons_ak8jets;
  OverlapMapHandler<PhotonObject, AK4JetObject> overlapMap_photons_ak4jets;
  OverlapMapHandler<PhotonObject, AK8JetObject> overlapMap_photons_ak8jets;
  // No need to register jet overlaps because we do not reconstruct jets in the skimmer.
  /*
  muonHandler.registerOverlapMaps(
    overlapMap_muons_ak4jets,
    overlapMap_muons_ak8jets
  );
  electronHandler.registerOverlapMaps(
    overlapMap_electrons_ak4jets,
    overlapMap_electrons_ak8jets
  );
  photonHandler.registerOverlapMaps(
    overlapMap_photons_ak4jets,
    overlapMap_photons_ak8jets
  );
  jetHandler.registerOverlapMaps(
    overlapMap_muons_ak4jets,
    overlapMap_muons_ak8jets,
    overlapMap_electrons_ak4jets,
    overlapMap_electrons_ak8jets,
    overlapMap_photons_ak4jets,
    overlapMap_photons_ak8jets
  );
  */

  genInfoHandler.setAllowLargeGenWeightRemoval(true);

  eventFilter.setTrackDataEvents(false);
  eventFilter.setCheckUniqueDataEvent(false);

  std::vector<TString> sampleList;
  SampleHelpers::constructSamplesList(strSampleSet, SystematicsHelpers::sNominal, sampleList);

  MELAout << "List of samples to process: " << sampleList << endl;
  for (auto const& strSample:sampleList){
    bool const isData = SampleHelpers::checkSampleIsData(strSample);

    TString const cinputcore = SampleHelpers::getDatasetDirectoryName(strSample);

    auto sfiles = SampleHelpers::lsdir(cinputcore);
    for (auto const& fname:sfiles){
      TString cinput = cinputcore + '/' + fname;
      MELAout << "Extracting input " << cinput << endl;

      BaseTree sample_tree(cinput, EVENTS_TREE_NAME, "", "");
      sample_tree.sampleIdentifier = SampleHelpers::getSampleIdentifier(strSample);

      std::vector<TString> branchnames;
      sample_tree.getValidBranchNamesWithoutAlias(branchnames, false);

      const int nEntries = sample_tree.getSelectedNEvents();
      int ev_start = 0;
      int ev_end = nEntries;
      MELAout << "Looping over " << nEntries << " events, starting from " << ev_start << " and ending at " << ev_end << "..." << endl;

      if (!isData){
        sample_tree.bookBranch<float>("xsec", 0.f);

        simEventHandler.bookBranches(&sample_tree);
        simEventHandler.wrapTree(&sample_tree);

        {
          bool has_lheMEweights = false;
          bool has_lheparticles = false;
          bool has_genparticles = false;
          std::vector<TString> allbranchnames; sample_tree.getValidBranchNamesWithoutAlias(allbranchnames, false);
          for (auto const& bname:allbranchnames){
            if (bname.Contains("p_Gen") || bname.Contains("LHECandMass")) has_lheMEweights=true;
            else if (bname.Contains(GenInfoHandler::colName_lheparticles)) has_lheparticles = true;
            else if (bname.Contains(GenInfoHandler::colName_genparticles)) has_genparticles = true;
          }
          genInfoHandler.setAcquireLHEMEWeights(has_lheMEweights);
          genInfoHandler.setAcquireLHEParticles(has_lheparticles);
          genInfoHandler.setAcquireGenParticles(has_genparticles);
        }
        genInfoHandler.bookBranches(&sample_tree);
        genInfoHandler.wrapTree(&sample_tree);
      }

      pfcandidateHandler.bookBranches(&sample_tree);
      pfcandidateHandler.wrapTree(&sample_tree);

      muonHandler.bookBranches(&sample_tree);
      muonHandler.wrapTree(&sample_tree);

      electronHandler.bookBranches(&sample_tree);
      electronHandler.wrapTree(&sample_tree);

      photonHandler.bookBranches(&sample_tree);
      photonHandler.wrapTree(&sample_tree);

      bool hasSuperclusters = false;
      for (auto const& bname:branchnames){
        if (bname.BeginsWith(SuperclusterHandler::colName.data())){
          hasSuperclusters = true;
          break;
        }
      }
      if (hasSuperclusters){
        superclusterHandler.bookBranches(&sample_tree);
        superclusterHandler.wrapTree(&sample_tree);
      }

      fsrHandler.bookBranches(&sample_tree);
      fsrHandler.wrapTree(&sample_tree);

      jetHandler.bookBranches(&sample_tree);
      jetHandler.wrapTree(&sample_tree);

      isotrackHandler.bookBranches(&sample_tree);
      isotrackHandler.wrapTree(&sample_tree);

      vertexHandler.bookBranches(&sample_tree);
      vertexHandler.wrapTree(&sample_tree);

      overlapMap_muons_ak4jets.bookBranches(&sample_tree); overlapMap_muons_ak4jets.wrapTree(&sample_tree);
      overlapMap_muons_ak8jets.bookBranches(&sample_tree); overlapMap_muons_ak8jets.wrapTree(&sample_tree);

      overlapMap_electrons_ak4jets.bookBranches(&sample_tree); overlapMap_electrons_ak4jets.wrapTree(&sample_tree);
      overlapMap_electrons_ak8jets.bookBranches(&sample_tree); overlapMap_electrons_ak8jets.wrapTree(&sample_tree);

      overlapMap_photons_ak4jets.bookBranches(&sample_tree); overlapMap_photons_ak4jets.wrapTree(&sample_tree);
      overlapMap_photons_ak8jets.bookBranches(&sample_tree); overlapMap_photons_ak8jets.wrapTree(&sample_tree);

      eventFilter.bookBranches(&sample_tree);
      eventFilter.wrapTree(&sample_tree);

      MELAout << "Completed getting the rest of the handles..." << endl;
      sample_tree.silenceUnused();
      sample_tree.getSelectedTree()->SetBranchStatus("triggerObjects*", 1); // Needed for trigger matching
    }
  }
}
