#include <CommonTools/UtilAlgos/interface/TFileService.h>

#include <CMS3/NtupleMaker/interface/plugins/MCUtilities.h>
#include <CMS3/NtupleMaker/interface/plugins/CMS3Ntuplizer.h>
#include "CMS3/NtupleMaker/interface/VertexSelectionHelpers.h"
#include "CMS3/NtupleMaker/interface/MuonSelectionHelpers.h"
#include "CMS3/NtupleMaker/interface/ElectronSelectionHelpers.h"
#include "CMS3/NtupleMaker/interface/PhotonSelectionHelpers.h"
#include "CMS3/NtupleMaker/interface/AK4JetSelectionHelpers.h"
#include "CMS3/NtupleMaker/interface/AK8JetSelectionHelpers.h"
#include <CMS3/NtupleMaker/interface/CMS3ObjectHelpers.h>

#include "MELAStreamHelpers.hh"


using namespace std;
using namespace MELAStreamHelpers;
using namespace edm;


CMS3Ntuplizer::CMS3Ntuplizer(const edm::ParameterSet& pset_) :
  pset(pset_),
  outtree(nullptr),
  firstEvent(true),

  year(pset.getParameter<int>("year")),
  treename(pset.getUntrackedParameter<std::string>("treename")),
  isMC(pset.getParameter<bool>("isMC")),

  prefiringWeightsTag(pset.getUntrackedParameter<std::string>("prefiringWeightsTag")),
  applyPrefiringWeights(prefiringWeightsTag!=""),

  keepGenParticles(pset.getParameter<bool>("keepGenParticles")),
  keepGenJets(pset.getParameter<bool>("keepGenJets"))
{
  if (year!=2016 && year!=2017 && year!=2018) throw cms::Exception("CMS3Ntuplizer::CMS3Ntuplizer: Year is undefined!");

  electronsToken  = consumes< edm::View<pat::Electron> >(pset.getParameter<edm::InputTag>("electronSrc"));
  photonsToken  = consumes< edm::View<pat::Photon> >(pset.getParameter<edm::InputTag>("photonSrc"));
  muonsToken  = consumes< edm::View<pat::Muon> >(pset.getParameter<edm::InputTag>("muonSrc"));
  ak4jetsToken  = consumes< edm::View<pat::Jet> >(pset.getParameter<edm::InputTag>("ak4jetSrc"));
  ak8jetsToken  = consumes< edm::View<pat::Jet> >(pset.getParameter<edm::InputTag>("ak8jetSrc"));

  pfmetToken = consumes< METInfo >(pset.getParameter<edm::InputTag>("pfmetSrc"));
  puppimetToken = consumes< METInfo >(pset.getParameter<edm::InputTag>("puppimetSrc"));

  vtxToken = consumes<reco::VertexCollection>(pset.getParameter<edm::InputTag>("vtxSrc"));

  rhoToken  = consumes< double >(pset.getParameter<edm::InputTag>("rhoSrc"));
  triggerInfoToken = consumes< edm::View<TriggerInfo> >(pset.getParameter<edm::InputTag>("triggerInfoSrc"));
  puInfoToken = consumes< std::vector<PileupSummaryInfo> >(pset.getParameter<edm::InputTag>("puInfoSrc"));
  metFilterInfoToken = consumes< METFilterInfo >(pset.getParameter<edm::InputTag>("metFilterInfoSrc"));

  if (applyPrefiringWeights){
    prefiringWeightToken = consumes< double >(edm::InputTag(prefiringWeightsTag, "nonPrefiringProb"));
    prefiringWeightToken_Dn = consumes< double >(edm::InputTag(prefiringWeightsTag, "nonPrefiringProbDown"));
    prefiringWeightToken_Up = consumes< double >(edm::InputTag(prefiringWeightsTag, "nonPrefiringProbUp"));
  }

  genInfoToken = consumes< GenInfo >(pset.getParameter<edm::InputTag>("genInfoSrc"));
  prunedGenParticlesToken = consumes< reco::GenParticleCollection >(pset.getParameter<edm::InputTag>("prunedGenParticlesSrc"));
  packedGenParticlesToken = consumes< pat::PackedGenParticleCollection >(pset.getParameter<edm::InputTag>("packedGenParticlesSrc"));
  genAK4JetsToken = consumes< edm::View<reco::GenJet> >(pset.getParameter<edm::InputTag>("genAK4JetsSrc"));
  genAK8JetsToken = consumes< edm::View<reco::GenJet> >(pset.getParameter<edm::InputTag>("genAK8JetsSrc"));

}
CMS3Ntuplizer::~CMS3Ntuplizer(){
  //delete pileUpReweight;
  //delete metCorrHandler;
}


void CMS3Ntuplizer::beginJob(){
  edm::Service<TFileService> fs;
  TTree* tout = fs->make<TTree>(treename, "Selected event summary");
  outtree = new BaseTree(nullptr, tout, nullptr, nullptr, false);
  outtree->setAcquireTreePossession(false);
}
void CMS3Ntuplizer::endJob(){
  delete outtree;
}

void CMS3Ntuplizer::beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&){}
void CMS3Ntuplizer::endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&){}

void CMS3Ntuplizer::beginRun(edm::Run const&, edm::EventSetup const&){}
void CMS3Ntuplizer::endRun(edm::Run const&, edm::EventSetup const&){}


// Convenience macros to easily make and push vector values
#define MAKE_VECTOR_WITH_RESERVE(type_, name_, size_) std::vector<type_> name_; name_.reserve(size_);
#define PUSH_USERINT_INTO_VECTOR(name_) name_.push_back(obj->userInt(#name_));
#define PUSH_USERFLOAT_INTO_VECTOR(name_) name_.push_back(obj->userFloat(#name_));
#define PUSH_VECTOR_WITH_NAME(name_, var_) commonEntry.setNamedVal(TString(name_)+"_"+#var_, var_);


void CMS3Ntuplizer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup){
  bool isSelected = true;

  /********************************/
  /* Set the communicator entries */
  /********************************/
  /*
  When naeing variables, try to be conscious of the nanoAOD naming conventions, but do not make a big fuss about them either!
  The latest list of variables are documented at https://cms-nanoaod-integration.web.cern.ch/integration/master-102X/mc102X_doc.html
  */

  // Gen. variables
  std::vector<reco::GenParticle const*> filledPrunedGenParts;
  std::vector<pat::PackedGenParticle const*> filledPackedGenParts;
  std::vector<reco::GenJet const*> filledGenAK4Jets;
  std::vector<reco::GenJet const*> filledGenAK8Jets;
  if (this->isMC) isSelected &= this->fillGenVariables(
    iEvent,
    &filledPrunedGenParts, &filledPackedGenParts,
    &filledGenAK4Jets, &filledGenAK8Jets
  );

  // Vertices
  size_t n_vtxs = this->fillVertices(iEvent, nullptr);
  isSelected &= (n_vtxs>0);

  // Electrons
  size_t n_electrons = this->fillElectrons(iEvent, nullptr);

  // Photons
  size_t n_photons = this->fillPhotons(iEvent, nullptr);

  // Muons
  size_t n_muons = this->fillMuons(iEvent, nullptr);

  // ak4 jets
  /*size_t n_ak4jets = */this->fillAK4Jets(iEvent, nullptr);

  // ak8 jets
  /*size_t n_ak8jets = */this->fillAK8Jets(iEvent, nullptr);

  // The (data) event should have at least one electron, muon, or photon.
  isSelected &= ((n_muons + n_electrons + n_photons)>0);

  // MET info
  isSelected &= this->fillMETVariables(iEvent);

  // Event info
  isSelected &= this->fillEventVariables(iEvent);

  // Trigger info
  isSelected &= this->fillTriggerInfo(iEvent);

  // MET filters
  isSelected &= this->fillMETFilterVariables(iEvent);


  /************************************************/
  /* Record the communicator values into the tree */
  /************************************************/

  // If this is the first event, create the tree branches based on what is available in the commonEntry.
  if (firstEvent){
#define SIMPLE_DATA_OUTPUT_DIRECTIVE(name_t, type) for (auto itb=commonEntry.named##name_t##s.begin(); itb!=commonEntry.named##name_t##s.end(); itb++) outtree->putBranch(itb->first, itb->second);
#define VECTOR_DATA_OUTPUT_DIRECTIVE(name_t, type) for (auto itb=commonEntry.namedV##name_t##s.begin(); itb!=commonEntry.namedV##name_t##s.end(); itb++) outtree->putBranch(itb->first, &(itb->second));
#define DOUBLEVECTOR_DATA_OUTPUT_DIRECTIVE(name_t, type) for (auto itb=commonEntry.namedVV##name_t##s.begin(); itb!=commonEntry.namedVV##name_t##s.end(); itb++) outtree->putBranch(itb->first, &(itb->second));
    SIMPLE_DATA_OUTPUT_DIRECTIVES
    VECTOR_DATA_OUTPUT_DIRECTIVES
    DOUBLEVECTOR_DATA_OUTPUT_DIRECTIVES
#undef SIMPLE_DATA_OUTPUT_DIRECTIVE
#undef VECTOR_DATA_OUTPUT_DIRECTIVE
#undef DOUBLEVECTOR_DATA_OUTPUT_DIRECTIVE

    outtree->getSelectedTree()->SetBasketSize("triggers_*", 16384*23);
  }

  // Record whatever is in commonEntry into the tree.
#define SIMPLE_DATA_OUTPUT_DIRECTIVE(name_t, type) for (auto itb=commonEntry.named##name_t##s.begin(); itb!=commonEntry.named##name_t##s.end(); itb++) outtree->setVal(itb->first, itb->second);
#define VECTOR_DATA_OUTPUT_DIRECTIVE(name_t, type) for (auto itb=commonEntry.namedV##name_t##s.begin(); itb!=commonEntry.namedV##name_t##s.end(); itb++) outtree->setVal(itb->first, &(itb->second));
#define DOUBLEVECTOR_DATA_OUTPUT_DIRECTIVE(name_t, type) for (auto itb=commonEntry.namedVV##name_t##s.begin(); itb!=commonEntry.namedVV##name_t##s.end(); itb++) outtree->setVal(itb->first, &(itb->second));
  SIMPLE_DATA_OUTPUT_DIRECTIVES
  VECTOR_DATA_OUTPUT_DIRECTIVES
  DOUBLEVECTOR_DATA_OUTPUT_DIRECTIVES
#undef SIMPLE_DATA_OUTPUT_DIRECTIVE
#undef VECTOR_DATA_OUTPUT_DIRECTIVE
#undef DOUBLEVECTOR_DATA_OUTPUT_DIRECTIVE

  // Fill the tree
  if (isMC || isSelected) outtree->fill();

  // No longer the first event...
  if (firstEvent) firstEvent = false;
}

void CMS3Ntuplizer::recordGenInfo(const edm::Event& iEvent){
  edm::Handle< GenInfo > genInfoHandle;
  iEvent.getByToken(genInfoToken, genInfoHandle);
  if (!genInfoHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::recordGenInfo: Error getting the gen. info. from the event...");
  const GenInfo& genInfo = *genInfoHandle;

#define SET_GENINFO_VARIABLE(var) commonEntry.setNamedVal(#var, genInfo.var);

  SET_GENINFO_VARIABLE(xsec)
  SET_GENINFO_VARIABLE(xsecerr)

  SET_GENINFO_VARIABLE(qscale)
  SET_GENINFO_VARIABLE(alphaS)

  SET_GENINFO_VARIABLE(genmet_met)
  SET_GENINFO_VARIABLE(genmet_metPhi)

  SET_GENINFO_VARIABLE(sumEt)
  SET_GENINFO_VARIABLE(pThat)

  // LHE variations
  SET_GENINFO_VARIABLE(genHEPMCweight_default)
  SET_GENINFO_VARIABLE(genHEPMCweight_NNPDF30)

  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR1_muF1)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR1_muF2)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR1_muF0p5)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR2_muF1)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR2_muF2)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR2_muF0p5)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR0p5_muF1)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR0p5_muF2)
  SET_GENINFO_VARIABLE(LHEweight_QCDscale_muR0p5_muF0p5)

  SET_GENINFO_VARIABLE(LHEweight_PDFVariation_Up_default)
  SET_GENINFO_VARIABLE(LHEweight_PDFVariation_Dn_default)
  SET_GENINFO_VARIABLE(LHEweight_AsMZ_Up_default)
  SET_GENINFO_VARIABLE(LHEweight_AsMZ_Dn_default)

  SET_GENINFO_VARIABLE(LHEweight_PDFVariation_Up_NNPDF30)
  SET_GENINFO_VARIABLE(LHEweight_PDFVariation_Dn_NNPDF30)
  SET_GENINFO_VARIABLE(LHEweight_AsMZ_Up_NNPDF30)
  SET_GENINFO_VARIABLE(LHEweight_AsMZ_Dn_NNPDF30)

  // Pythis PS weights
  SET_GENINFO_VARIABLE(PythiaWeight_isr_muRoneoversqrt2)
  SET_GENINFO_VARIABLE(PythiaWeight_fsr_muRoneoversqrt2)
  SET_GENINFO_VARIABLE(PythiaWeight_isr_muRsqrt2)
  SET_GENINFO_VARIABLE(PythiaWeight_fsr_muRsqrt2)
  SET_GENINFO_VARIABLE(PythiaWeight_isr_muR0p5)
  SET_GENINFO_VARIABLE(PythiaWeight_fsr_muR0p5)
  SET_GENINFO_VARIABLE(PythiaWeight_isr_muR2)
  SET_GENINFO_VARIABLE(PythiaWeight_fsr_muR2)
  SET_GENINFO_VARIABLE(PythiaWeight_isr_muR0p25)
  SET_GENINFO_VARIABLE(PythiaWeight_fsr_muR0p25)
  SET_GENINFO_VARIABLE(PythiaWeight_isr_muR4)
  SET_GENINFO_VARIABLE(PythiaWeight_fsr_muR4)

#undef SET_GENINFO_VARIABLE

  for (auto const it:genInfo.LHE_ME_weights) commonEntry.setNamedVal(it.first, it.second);
}
void CMS3Ntuplizer::recordGenParticles(const edm::Event& iEvent, std::vector<reco::GenParticle const*>* filledGenParts, std::vector<pat::PackedGenParticle const*>* filledPackedGenParts){
  const char colName[] = "genparticles";

  edm::Handle<reco::GenParticleCollection> prunedGenParticlesHandle;
  iEvent.getByToken(prunedGenParticlesToken, prunedGenParticlesHandle);
  if (!prunedGenParticlesHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::recordGenParticles: Error getting the pruned gen. particles from the event...");
  std::vector<reco::GenParticle> const* prunedGenParticles = prunedGenParticlesHandle.product();

  edm::Handle<pat::PackedGenParticleCollection> packedGenParticlesHandle;
  iEvent.getByToken(packedGenParticlesToken, packedGenParticlesHandle);
  if (!prunedGenParticlesHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::recordGenParticles: Error getting the packed gen. particles from the event...");
  std::vector<pat::PackedGenParticle> const* packedGenParticles = packedGenParticlesHandle.product();

  // Make a collection of all unique reco::GenParticle pointers
  std::vector<reco::GenParticle const*> allGenParticles; allGenParticles.reserve(prunedGenParticles->size() + packedGenParticles->size());
  // Fill with the pruned collection first
  for (reco::GenParticle const& part:(*prunedGenParticles)) allGenParticles.push_back(&part);

  // Get the packed gen. particles unique from the pruned collection
  // Adapted from GeneratorInterface/RivetInterface/plugins/MergedGenParticleProducer.cc
  std::vector<pat::PackedGenParticle const*> uniquePackedGenParticles; uniquePackedGenParticles.reserve(packedGenParticles->size());
  for (pat::PackedGenParticle const& packedGenParticle:(*packedGenParticlesHandle)){
    double match_ref = -1;

    for (reco::GenParticle const& prunedGenParticle:(*prunedGenParticles)){
      if (prunedGenParticle.status() != 1 || packedGenParticle.pdgId() != prunedGenParticle.pdgId()) continue;

      double euc_dot_prod = packedGenParticle.px()*prunedGenParticle.px() + packedGenParticle.py()*prunedGenParticle.py() + packedGenParticle.pz()*prunedGenParticle.pz() + packedGenParticle.energy()*prunedGenParticle.energy();
      double comp_ref = prunedGenParticle.px()*prunedGenParticle.px() + prunedGenParticle.py()*prunedGenParticle.py() + prunedGenParticle.pz()*prunedGenParticle.pz() + prunedGenParticle.energy()*prunedGenParticle.energy();
      double match_ref_tmp = std::abs(euc_dot_prod/comp_ref - 1.);
      if (match_ref_tmp<1e-5 && (match_ref<0. || match_ref_tmp<match_ref)) match_ref = match_ref_tmp;
    }
    if (match_ref>=0.) uniquePackedGenParticles.push_back(&packedGenParticle);
  }

  // Add the mothers of the packed gen. particles to the bigger collection
  for (pat::PackedGenParticle const* part:uniquePackedGenParticles) MCUtilities::getAllMothers(part, allGenParticles, false);

  // Make the variables to record
  // Size of the variable collections are known at this point.
  size_t n_objects = allGenParticles.size() + uniquePackedGenParticles.size();
  if (filledGenParts) filledGenParts->reserve(allGenParticles.size());
  if (filledPackedGenParts) filledPackedGenParts->reserve(uniquePackedGenParticles.size());

  MAKE_VECTOR_WITH_RESERVE(float, pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, mass, n_objects);

  MAKE_VECTOR_WITH_RESERVE(bool, is_packed, n_objects);

  // a) isPromptFinalState(): is particle prompt (not from hadron, muon, or tau decay) and final state
  // b) isPromptDecayed(): is particle prompt (not from hadron, muon, or tau decay) and decayed
  // c) isDirectPromptTauDecayProductFinalState(): this particle is a direct decay product of a prompt tau and is final state
  //    (eg an electron or muon from a leptonic decay of a prompt tau)
  // d) isHardProcess(): this particle is part of the hard process
  // e) fromHardProcessFinalState(): this particle is the final state direct descendant of a hard process particle
  // f) fromHardProcessDecayed(): this particle is the decayed direct descendant of a hard process particle such as a tau from the hard process
  // g) isDirectHardProcessTauDecayProductFinalState(): this particle is a direct decay product of a hardprocess tau and is final state
  //    (eg an electron or muon from a leptonic decay of a tau from the hard process)
  // h) fromHardProcessBeforeFSR(): this particle is the direct descendant of a hard process particle of the same pdg id.
  //    For outgoing particles the kinematics are those before QCD or QED FSR
  //    This corresponds roughly to status code 3 in pythia 6
  //    This is the most complex and error prone of all the flags and you are strongly encouraged
  //    to consider using the others to fill your needs.
  // i) isLastCopy(): this particle is the last copy of the particle in the chain  with the same pdg id
  //    (and therefore is more likely, but not guaranteed, to carry the final physical momentum)
  // j) isLastCopyBeforeFSR(): this particle is the last copy of the particle in the chain with the same pdg id
  //    before QED or QCD FSR (and therefore is more likely, but not guaranteed, to carry the momentum after ISR)
  MAKE_VECTOR_WITH_RESERVE(bool, isPromptFinalState, n_objects); // (a)
  MAKE_VECTOR_WITH_RESERVE(bool, isPromptDecayed, n_objects); // (b)
  MAKE_VECTOR_WITH_RESERVE(bool, isDirectPromptTauDecayProductFinalState, n_objects); // (c)
  MAKE_VECTOR_WITH_RESERVE(bool, isHardProcess, n_objects); // (d)
  MAKE_VECTOR_WITH_RESERVE(bool, fromHardProcessFinalState, n_objects); // (e)
  MAKE_VECTOR_WITH_RESERVE(bool, fromHardProcessDecayed, n_objects); // (f)
  MAKE_VECTOR_WITH_RESERVE(bool, isDirectHardProcessTauDecayProductFinalState, n_objects); // (g)
  MAKE_VECTOR_WITH_RESERVE(bool, fromHardProcessBeforeFSR, n_objects); // (h)
  MAKE_VECTOR_WITH_RESERVE(bool, isLastCopy, n_objects); // (i)
  MAKE_VECTOR_WITH_RESERVE(bool, isLastCopyBeforeFSR, n_objects); // (j)

  MAKE_VECTOR_WITH_RESERVE(int, id, n_objects);
  MAKE_VECTOR_WITH_RESERVE(int, status, n_objects);
  MAKE_VECTOR_WITH_RESERVE(int, mom0_index, n_objects);
  MAKE_VECTOR_WITH_RESERVE(int, mom1_index, n_objects);

  // Record all reco::GenParticle objects
  for (reco::GenParticle const* obj:allGenParticles){
    if (filledGenParts && obj->status()==1) filledGenParts->push_back(obj);

    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());

    is_packed.push_back(false);

    isPromptFinalState.push_back(obj->isPromptFinalState()); // (a)
    isPromptDecayed.push_back(obj->isPromptDecayed()); // (b)
    isDirectPromptTauDecayProductFinalState.push_back(obj->isDirectPromptTauDecayProductFinalState()); // (c)
    isHardProcess.push_back(obj->isHardProcess()); // (d)
    fromHardProcessFinalState.push_back(obj->fromHardProcessFinalState()); // (e)
    fromHardProcessDecayed.push_back(obj->fromHardProcessDecayed()); // (f)
    isDirectHardProcessTauDecayProductFinalState.push_back(obj->isDirectHardProcessTauDecayProductFinalState()); // (g)
    fromHardProcessBeforeFSR.push_back(obj->fromHardProcessBeforeFSR()); // (h)
    isLastCopy.push_back(obj->isLastCopy()); // (i)
    isLastCopyBeforeFSR.push_back(obj->isLastCopyBeforeFSR()); // (j)

    id.push_back(obj->pdgId());
    status.push_back(obj->status());

    std::vector<const reco::GenParticle*> mothers;
    MCUtilities::getAllMothers(obj, mothers, false);
    if (mothers.size()>0){
      const reco::GenParticle* mom = mothers.at(0);
      int index=-1;
      for (reco::GenParticle const* tmpobj:allGenParticles){
        index++;
        if (tmpobj == obj) continue;
        if (mom == tmpobj) break;
      }
      mom0_index.push_back(index);
    }
    else mom0_index.push_back(-1);
    if (mothers.size()>1){
      const reco::GenParticle* mom = mothers.at(1);
      int index=-1;
      for (reco::GenParticle const* tmpobj:allGenParticles){
        index++;
        if (tmpobj == obj) continue;
        if (mom == tmpobj) break;
      }
      mom1_index.push_back(index);
    }
    else mom1_index.push_back(-1);
  }
  // Record the remaining unique pat::PackedGenParticle objects
  for (pat::PackedGenParticle const* obj:uniquePackedGenParticles){
    if (filledPackedGenParts && obj->status()==1) filledPackedGenParts->push_back(obj);

    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());

    is_packed.push_back(true);

    isPromptFinalState.push_back(false); // (a)
    isPromptDecayed.push_back(false); // (b)
    isDirectPromptTauDecayProductFinalState.push_back(false); // (c)
    isHardProcess.push_back(false); // (d)
    fromHardProcessFinalState.push_back(false); // (e)
    fromHardProcessDecayed.push_back(false); // (f)
    isDirectHardProcessTauDecayProductFinalState.push_back(false); // (g)
    fromHardProcessBeforeFSR.push_back(false); // (h)
    isLastCopy.push_back(false); // (i)
    isLastCopyBeforeFSR.push_back(false); // (j)

    id.push_back(obj->pdgId());
    status.push_back(obj->status());

    std::vector<const reco::GenParticle*> mothers;
    MCUtilities::getAllMothers(obj, mothers, false);
    if (mothers.size()>0){
      const reco::GenParticle* mom = mothers.at(0);
      int index=-1;
      for (reco::GenParticle const* tmpobj:allGenParticles){
        index++;
        if (mom == tmpobj) break;
      }
      mom0_index.push_back(index);
    }
    else mom0_index.push_back(-1);
    if (mothers.size()>1){
      const reco::GenParticle* mom = mothers.at(1);
      int index=-1;
      for (reco::GenParticle const* tmpobj:allGenParticles){
        index++;
        if (mom == tmpobj) break;
      }
      mom1_index.push_back(index);
    }
    else mom1_index.push_back(-1);
  }

  PUSH_VECTOR_WITH_NAME(colName, pt);
  PUSH_VECTOR_WITH_NAME(colName, eta);
  PUSH_VECTOR_WITH_NAME(colName, phi);
  PUSH_VECTOR_WITH_NAME(colName, mass);

  PUSH_VECTOR_WITH_NAME(colName, is_packed);

  PUSH_VECTOR_WITH_NAME(colName, isPromptFinalState); // (a)
  PUSH_VECTOR_WITH_NAME(colName, isPromptDecayed); // (b)
  PUSH_VECTOR_WITH_NAME(colName, isDirectPromptTauDecayProductFinalState); // (c)
  PUSH_VECTOR_WITH_NAME(colName, isHardProcess); // (d)
  PUSH_VECTOR_WITH_NAME(colName, fromHardProcessFinalState); // (e)
  PUSH_VECTOR_WITH_NAME(colName, fromHardProcessDecayed); // (f)
  PUSH_VECTOR_WITH_NAME(colName, isDirectHardProcessTauDecayProductFinalState); // (g)
  PUSH_VECTOR_WITH_NAME(colName, fromHardProcessBeforeFSR); // (h)
  PUSH_VECTOR_WITH_NAME(colName, isLastCopy); // (i)
  PUSH_VECTOR_WITH_NAME(colName, isLastCopyBeforeFSR); // (j)

  PUSH_VECTOR_WITH_NAME(colName, id);
  PUSH_VECTOR_WITH_NAME(colName, status);
  PUSH_VECTOR_WITH_NAME(colName, mom0_index);
  PUSH_VECTOR_WITH_NAME(colName, mom1_index);

}
void CMS3Ntuplizer::recordGenJets(const edm::Event& iEvent, bool const& isFatJet, std::vector<reco::GenJet const*>* filledObjects){
  std::string strColName = (isFatJet ? "genak4jets" : "genak8jets");
  const char* colName = strColName.data();
  edm::Handle< edm::View<reco::GenJet> > genJetsHandle;
  iEvent.getByToken((isFatJet ? genAK4JetsToken : genAK8JetsToken), genJetsHandle);
  if (!genJetsHandle.isValid()) throw cms::Exception((std::string("CMS3Ntuplizer::recordGenJets: Error getting the gen. ") + (isFatJet ? "ak4" : "ak8") + " jets from the event...").data());

  size_t n_objects = genJetsHandle->size();
  if (filledObjects) filledObjects->reserve(n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, mass, n_objects);

  for (edm::View<reco::GenJet>::const_iterator obj = genJetsHandle->begin(); obj != genJetsHandle->end(); obj++){
    if (filledObjects) filledObjects->push_back(&(*obj));

    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());
  }

  // Pass collections to the communicator
  PUSH_VECTOR_WITH_NAME(colName, pt);
  PUSH_VECTOR_WITH_NAME(colName, eta);
  PUSH_VECTOR_WITH_NAME(colName, phi);
  PUSH_VECTOR_WITH_NAME(colName, mass);
}

size_t CMS3Ntuplizer::fillElectrons(const edm::Event& iEvent, std::vector<pat::Electron const*>* filledObjects){
  const char colName[] = "electrons";
  edm::Handle< edm::View<pat::Electron> > electronsHandle;
  iEvent.getByToken(electronsToken, electronsHandle);
  if (!electronsHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillElectrons: Error getting the electron collection from the event...");
  size_t n_objects = electronsHandle->size();

  if (filledObjects) filledObjects->reserve(n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, mass, n_objects);

  MAKE_VECTOR_WITH_RESERVE(int, charge, n_objects);

  // Has no convention correspondence in nanoAOD
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_scale_totalUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_scale_totalDn, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_smear_totalUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_smear_totalDn, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, id_MVA_Fall17V2_Iso_Val, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_MVA_Fall17V2_Iso_Cat, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, id_MVA_Fall17V2_NoIso_Val, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_MVA_Fall17V2_NoIso_Cat, n_objects);

  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V2_Veto_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V2_Loose_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V2_Medium_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V2_Tight_Bits, n_objects);

  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V1_Veto_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V1_Loose_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V1_Medium_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V1_Tight_Bits, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pfIso03_comb_nofsr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pfIso04_comb_nofsr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, miniIso_comb_nofsr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, miniIso_comb_nofsr_uncorrected, n_objects);

  MAKE_VECTOR_WITH_RESERVE(unsigned int, fid_mask, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, type_mask, n_objects);

  for (View<pat::Electron>::const_iterator obj = electronsHandle->begin(); obj != electronsHandle->end(); obj++){
    if (!ElectronSelectionHelpers::testSkimElectron(*obj, this->year)) continue;

    // Core particle quantities
    // Uncorrected p4
    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());

    // Charge: Can obtain pdgId from this, so no need to record pdgId again
    PUSH_USERINT_INTO_VECTOR(charge);

    // Scale and smear
    // Nominal value: Needs to multiply the uncorrected p4 at analysis level
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr);
    // Uncertainties: Only store total up/dn for the moment
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_scale_totalUp);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_scale_totalDn);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_smear_totalUp);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_smear_totalDn);

    // Id variables
    // Fall17V2_Iso MVA id
    PUSH_USERFLOAT_INTO_VECTOR(id_MVA_Fall17V2_Iso_Val);
    PUSH_USERINT_INTO_VECTOR(id_MVA_Fall17V2_Iso_Cat);

    // Fall17V2_NoIso MVA id
    PUSH_USERFLOAT_INTO_VECTOR(id_MVA_Fall17V2_NoIso_Val);
    PUSH_USERINT_INTO_VECTOR(id_MVA_Fall17V2_NoIso_Cat);

    // Fall17V2 cut-based ids
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V2_Veto_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V2_Loose_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V2_Medium_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V2_Tight_Bits);

    // Fall17V1 cut-based ids
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V1_Veto_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V1_Loose_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V1_Medium_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V1_Tight_Bits);

    // Isolation variables
    PUSH_USERFLOAT_INTO_VECTOR(pfIso03_comb_nofsr);
    PUSH_USERFLOAT_INTO_VECTOR(pfIso04_comb_nofsr);
    PUSH_USERFLOAT_INTO_VECTOR(miniIso_comb_nofsr);
    PUSH_USERFLOAT_INTO_VECTOR(miniIso_comb_nofsr_uncorrected);

    // Masks
    PUSH_USERINT_INTO_VECTOR(fid_mask);
    PUSH_USERINT_INTO_VECTOR(type_mask);

    if (filledObjects) filledObjects->push_back(&(*obj));
  }

  // Pass collections to the communicator
  PUSH_VECTOR_WITH_NAME(colName, pt);
  PUSH_VECTOR_WITH_NAME(colName, eta);
  PUSH_VECTOR_WITH_NAME(colName, phi);
  PUSH_VECTOR_WITH_NAME(colName, mass);

  PUSH_VECTOR_WITH_NAME(colName, charge);

  // Has no convention correspondence in nanoAOD
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_scale_totalUp);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_scale_totalDn);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_smear_totalUp);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_smear_totalDn);

  PUSH_VECTOR_WITH_NAME(colName, id_MVA_Fall17V2_Iso_Val);
  PUSH_VECTOR_WITH_NAME(colName, id_MVA_Fall17V2_Iso_Cat);
  PUSH_VECTOR_WITH_NAME(colName, id_MVA_Fall17V2_NoIso_Val);
  PUSH_VECTOR_WITH_NAME(colName, id_MVA_Fall17V2_NoIso_Cat);

  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V2_Veto_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V2_Loose_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V2_Medium_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V2_Tight_Bits);

  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V1_Veto_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V1_Loose_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V1_Medium_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V1_Tight_Bits);

  PUSH_VECTOR_WITH_NAME(colName, pfIso03_comb_nofsr);
  PUSH_VECTOR_WITH_NAME(colName, pfIso04_comb_nofsr);
  PUSH_VECTOR_WITH_NAME(colName, miniIso_comb_nofsr);
  PUSH_VECTOR_WITH_NAME(colName, miniIso_comb_nofsr_uncorrected);

  PUSH_VECTOR_WITH_NAME(colName, fid_mask);
  PUSH_VECTOR_WITH_NAME(colName, type_mask);

  return n_objects;
}
size_t CMS3Ntuplizer::fillPhotons(const edm::Event& iEvent, std::vector<pat::Photon const*>* filledObjects){
  const char colName[] = "photons";
  edm::Handle< edm::View<pat::Photon> > photonsHandle;
  iEvent.getByToken(photonsToken, photonsHandle);
  if (!photonsHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillPhotons: Error getting the photon collection from the event...");
  size_t n_objects = photonsHandle->size();

  if (filledObjects) filledObjects->reserve(n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, mass, n_objects);

  // Has no convention correspondence in nanoAOD
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_scale_totalUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_scale_totalDn, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_smear_totalUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_corr_smear_totalDn, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, id_MVA_Fall17V2_Val, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_MVA_Fall17V2_Cat, n_objects);

  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V2_Loose_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V2_Medium_Bits, n_objects);
  MAKE_VECTOR_WITH_RESERVE(unsigned int, id_cutBased_Fall17V2_Tight_Bits, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pfIso_comb, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pfChargedHadronIso_EAcorr, n_objects);

  for (View<pat::Photon>::const_iterator obj = photonsHandle->begin(); obj != photonsHandle->end(); obj++){
    // Core particle quantities
    // Uncorrected p4
    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());

    // Scale and smear
    // Nominal value: Needs to multiply the uncorrected p4 at analysis level
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr);
    // Uncertainties: Only store total up/dn for the moment
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_scale_totalUp);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_scale_totalDn);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_smear_totalUp);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_corr_smear_totalDn);

    // Id variables
    // Fall17V2 MVA id
    PUSH_USERFLOAT_INTO_VECTOR(id_MVA_Fall17V2_Val);
    PUSH_USERINT_INTO_VECTOR(id_MVA_Fall17V2_Cat);

    // Fall17V2 cut-based ids
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V2_Loose_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V2_Medium_Bits);
    PUSH_USERINT_INTO_VECTOR(id_cutBased_Fall17V2_Tight_Bits);

    PUSH_USERFLOAT_INTO_VECTOR(pfIso_comb);
    PUSH_USERFLOAT_INTO_VECTOR(pfChargedHadronIso_EAcorr);

    if (filledObjects) filledObjects->push_back(&(*obj));
  }

  // Pass collections to the communicator
  PUSH_VECTOR_WITH_NAME(colName, pt);
  PUSH_VECTOR_WITH_NAME(colName, eta);
  PUSH_VECTOR_WITH_NAME(colName, phi);
  PUSH_VECTOR_WITH_NAME(colName, mass);

  // Has no convention correspondence in nanoAOD
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_scale_totalUp);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_scale_totalDn);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_smear_totalUp);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_corr_smear_totalDn);

  PUSH_VECTOR_WITH_NAME(colName, id_MVA_Fall17V2_Val);
  PUSH_VECTOR_WITH_NAME(colName, id_MVA_Fall17V2_Cat);

  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V2_Loose_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V2_Medium_Bits);
  PUSH_VECTOR_WITH_NAME(colName, id_cutBased_Fall17V2_Tight_Bits);

  PUSH_VECTOR_WITH_NAME(colName, pfIso_comb);
  PUSH_VECTOR_WITH_NAME(colName, pfChargedHadronIso_EAcorr);

  return n_objects;
}
size_t CMS3Ntuplizer::fillMuons(const edm::Event& iEvent, std::vector<pat::Muon const*>* filledObjects){
  const char colName[] = "muons";
  edm::Handle< edm::View<pat::Muon> > muonsHandle;
  iEvent.getByToken(muonsToken, muonsHandle);
  if (!muonsHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillMuons: Error getting the muon collection from the event...");
  size_t n_objects = muonsHandle->size();

  if (filledObjects) filledObjects->reserve(n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, mass, n_objects);

  MAKE_VECTOR_WITH_RESERVE(int, charge, n_objects);

  MAKE_VECTOR_WITH_RESERVE(unsigned int, POG_selector_bits, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pfIso03_comb_nofsr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pfIso04_comb_nofsr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, miniIso_comb_nofsr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, miniIso_comb_nofsr_uncorrected, n_objects);

  MAKE_VECTOR_WITH_RESERVE(int, time_comb_ndof, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_comb_IPInOut, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_comb_IPOutIn, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_comb_IPInOutError, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_comb_IPOutInError, n_objects);
  MAKE_VECTOR_WITH_RESERVE(int, time_rpc_ndof, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_rpc_IPInOut, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_rpc_IPOutIn, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_rpc_IPInOutError, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, time_rpc_IPOutInError, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_pt_corr, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_pt_corr_scale_totalUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_pt_corr_scale_totalDn, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_pt_corr_smear_totalUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, scale_smear_pt_corr_smear_totalDn, n_objects);

  for (View<pat::Muon>::const_iterator obj = muonsHandle->begin(); obj != muonsHandle->end(); obj++){
    if (!MuonSelectionHelpers::testSkimMuon(*obj, this->year)) continue;

    // Core particle quantities
    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());

    PUSH_USERINT_INTO_VECTOR(charge);

    PUSH_USERINT_INTO_VECTOR(POG_selector_bits);

    PUSH_USERFLOAT_INTO_VECTOR(pfIso03_comb_nofsr);
    PUSH_USERFLOAT_INTO_VECTOR(pfIso04_comb_nofsr);
    PUSH_USERFLOAT_INTO_VECTOR(miniIso_comb_nofsr);
    PUSH_USERFLOAT_INTO_VECTOR(miniIso_comb_nofsr_uncorrected);

    PUSH_USERINT_INTO_VECTOR(time_comb_ndof);
    PUSH_USERFLOAT_INTO_VECTOR(time_comb_IPInOut);
    PUSH_USERFLOAT_INTO_VECTOR(time_comb_IPOutIn);
    PUSH_USERFLOAT_INTO_VECTOR(time_comb_IPInOutError);
    PUSH_USERFLOAT_INTO_VECTOR(time_comb_IPOutInError);
    PUSH_USERINT_INTO_VECTOR(time_rpc_ndof);
    PUSH_USERFLOAT_INTO_VECTOR(time_rpc_IPInOut);
    PUSH_USERFLOAT_INTO_VECTOR(time_rpc_IPOutIn);
    PUSH_USERFLOAT_INTO_VECTOR(time_rpc_IPInOutError);
    PUSH_USERFLOAT_INTO_VECTOR(time_rpc_IPOutInError);

    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_pt_corr);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_pt_corr_scale_totalUp);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_pt_corr_scale_totalDn);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_pt_corr_smear_totalUp);
    PUSH_USERFLOAT_INTO_VECTOR(scale_smear_pt_corr_smear_totalDn);

    if (filledObjects) filledObjects->push_back(&(*obj));
  }

  // Pass collections to the communicator
  PUSH_VECTOR_WITH_NAME(colName, pt);
  PUSH_VECTOR_WITH_NAME(colName, eta);
  PUSH_VECTOR_WITH_NAME(colName, phi);
  PUSH_VECTOR_WITH_NAME(colName, mass);

  PUSH_VECTOR_WITH_NAME(colName, charge);

  PUSH_VECTOR_WITH_NAME(colName, POG_selector_bits);

  PUSH_VECTOR_WITH_NAME(colName, pfIso03_comb_nofsr);
  PUSH_VECTOR_WITH_NAME(colName, pfIso04_comb_nofsr);
  PUSH_VECTOR_WITH_NAME(colName, miniIso_comb_nofsr);
  PUSH_VECTOR_WITH_NAME(colName, miniIso_comb_nofsr_uncorrected);

  PUSH_VECTOR_WITH_NAME(colName, time_comb_ndof);
  PUSH_VECTOR_WITH_NAME(colName, time_comb_IPInOut);
  PUSH_VECTOR_WITH_NAME(colName, time_comb_IPOutIn);
  PUSH_VECTOR_WITH_NAME(colName, time_comb_IPInOutError);
  PUSH_VECTOR_WITH_NAME(colName, time_comb_IPOutInError);
  PUSH_VECTOR_WITH_NAME(colName, time_rpc_ndof);
  PUSH_VECTOR_WITH_NAME(colName, time_rpc_IPInOut);
  PUSH_VECTOR_WITH_NAME(colName, time_rpc_IPOutIn);
  PUSH_VECTOR_WITH_NAME(colName, time_rpc_IPInOutError);
  PUSH_VECTOR_WITH_NAME(colName, time_rpc_IPOutInError);

  PUSH_VECTOR_WITH_NAME(colName, scale_smear_pt_corr);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_pt_corr_scale_totalUp);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_pt_corr_scale_totalDn);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_pt_corr_smear_totalUp);
  PUSH_VECTOR_WITH_NAME(colName, scale_smear_pt_corr_smear_totalDn);

  return n_objects;
}
size_t CMS3Ntuplizer::fillAK4Jets(const edm::Event& iEvent, std::vector<pat::Jet const*>* filledObjects){
  constexpr AK4JetSelectionHelpers::AK4JetType jetType = AK4JetSelectionHelpers::AK4PFCHS;

  const char colName[] = "ak4jets";
  edm::Handle< edm::View<pat::Jet> > ak4jetsHandle;
  iEvent.getByToken(ak4jetsToken, ak4jetsHandle);
  if (!ak4jetsHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillAK4Jets: Error getting the ak4 jet collection from the event...");
  size_t n_objects = ak4jetsHandle->size();

  if (filledObjects) filledObjects->reserve(n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, mass, n_objects);

  MAKE_VECTOR_WITH_RESERVE(bool, pass_looseId, n_objects);
  MAKE_VECTOR_WITH_RESERVE(bool, pass_tightId, n_objects);
  MAKE_VECTOR_WITH_RESERVE(bool, pass_leptonVetoId, n_objects);
  MAKE_VECTOR_WITH_RESERVE(bool, pass_puId, n_objects);

  MAKE_VECTOR_WITH_RESERVE(size_t, n_pfcands, n_objects);
  MAKE_VECTOR_WITH_RESERVE(size_t, n_mucands, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, area, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pt_resolution, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, deepFlavourprobb, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, deepFlavourprobbb, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, deepFlavourprobc, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, deepFlavourprobg, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, deepFlavourproblepb, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, deepFlavourprobuds, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, ptDistribution, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, totalMultiplicity, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, axis1, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, axis2, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, JECNominal, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JECUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JECDn, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, JERNominal, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JERUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JERDn, n_objects);

  MAKE_VECTOR_WITH_RESERVE(int, partonFlavour, n_objects);
  MAKE_VECTOR_WITH_RESERVE(int, hadronFlavour, n_objects);

  for (View<pat::Jet>::const_iterator obj = ak4jetsHandle->begin(); obj != ak4jetsHandle->end(); obj++){
    if (!AK4JetSelectionHelpers::testSkimAK4Jet(*obj, this->year, jetType)) continue;

    // Core particle quantities
    // These are the uncorrected momentum components!
    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());

    pass_looseId.push_back(AK4JetSelectionHelpers::testLooseAK4Jet(*obj, this->year, jetType));
    pass_tightId.push_back(AK4JetSelectionHelpers::testTightAK4Jet(*obj, this->year, jetType));
    pass_leptonVetoId.push_back(AK4JetSelectionHelpers::testLeptonVetoAK4Jet(*obj, this->year, jetType));
    pass_puId.push_back(AK4JetSelectionHelpers::testPileUpAK4Jet(*obj, this->year, jetType));

    PUSH_USERINT_INTO_VECTOR(n_pfcands);
    PUSH_USERINT_INTO_VECTOR(n_mucands);

    PUSH_USERFLOAT_INTO_VECTOR(area);
    PUSH_USERFLOAT_INTO_VECTOR(pt_resolution);

    PUSH_USERFLOAT_INTO_VECTOR(deepFlavourprobb);
    PUSH_USERFLOAT_INTO_VECTOR(deepFlavourprobbb);
    PUSH_USERFLOAT_INTO_VECTOR(deepFlavourprobc);
    PUSH_USERFLOAT_INTO_VECTOR(deepFlavourprobg);
    PUSH_USERFLOAT_INTO_VECTOR(deepFlavourproblepb);
    PUSH_USERFLOAT_INTO_VECTOR(deepFlavourprobuds);

    PUSH_USERFLOAT_INTO_VECTOR(ptDistribution);
    PUSH_USERFLOAT_INTO_VECTOR(totalMultiplicity);
    PUSH_USERFLOAT_INTO_VECTOR(axis1);
    PUSH_USERFLOAT_INTO_VECTOR(axis2);

    PUSH_USERFLOAT_INTO_VECTOR(JECNominal);
    PUSH_USERFLOAT_INTO_VECTOR(JECUp);
    PUSH_USERFLOAT_INTO_VECTOR(JECDn);

    PUSH_USERFLOAT_INTO_VECTOR(JERNominal);
    PUSH_USERFLOAT_INTO_VECTOR(JERUp);
    PUSH_USERFLOAT_INTO_VECTOR(JERDn);

    PUSH_USERINT_INTO_VECTOR(partonFlavour);
    PUSH_USERINT_INTO_VECTOR(hadronFlavour);

    if (filledObjects) filledObjects->push_back(&(*obj));
  }

  // Pass collections to the communicator
  PUSH_VECTOR_WITH_NAME(colName, pt);
  PUSH_VECTOR_WITH_NAME(colName, eta);
  PUSH_VECTOR_WITH_NAME(colName, phi);
  PUSH_VECTOR_WITH_NAME(colName, mass);

  PUSH_VECTOR_WITH_NAME(colName, pass_looseId);
  PUSH_VECTOR_WITH_NAME(colName, pass_tightId);
  PUSH_VECTOR_WITH_NAME(colName, pass_leptonVetoId);
  PUSH_VECTOR_WITH_NAME(colName, pass_puId);

  PUSH_VECTOR_WITH_NAME(colName, n_pfcands);
  PUSH_VECTOR_WITH_NAME(colName, n_mucands);

  PUSH_VECTOR_WITH_NAME(colName, area);
  PUSH_VECTOR_WITH_NAME(colName, pt_resolution);

  PUSH_VECTOR_WITH_NAME(colName, deepFlavourprobb);
  PUSH_VECTOR_WITH_NAME(colName, deepFlavourprobbb);
  PUSH_VECTOR_WITH_NAME(colName, deepFlavourprobc);
  PUSH_VECTOR_WITH_NAME(colName, deepFlavourprobg);
  PUSH_VECTOR_WITH_NAME(colName, deepFlavourproblepb);
  PUSH_VECTOR_WITH_NAME(colName, deepFlavourprobuds);

  PUSH_VECTOR_WITH_NAME(colName, ptDistribution);
  PUSH_VECTOR_WITH_NAME(colName, totalMultiplicity);
  PUSH_VECTOR_WITH_NAME(colName, axis1);
  PUSH_VECTOR_WITH_NAME(colName, axis2);

  PUSH_VECTOR_WITH_NAME(colName, JECNominal);
  PUSH_VECTOR_WITH_NAME(colName, JECUp);
  PUSH_VECTOR_WITH_NAME(colName, JECDn);

  PUSH_VECTOR_WITH_NAME(colName, JERNominal);
  PUSH_VECTOR_WITH_NAME(colName, JERUp);
  PUSH_VECTOR_WITH_NAME(colName, JERDn);

  PUSH_VECTOR_WITH_NAME(colName, partonFlavour);
  PUSH_VECTOR_WITH_NAME(colName, hadronFlavour);

  return n_objects;
}
size_t CMS3Ntuplizer::fillAK8Jets(const edm::Event& iEvent, std::vector<pat::Jet const*>* filledObjects){
  const char colName[] = "ak8jets";
  edm::Handle< edm::View<pat::Jet> > ak8jetsHandle;
  iEvent.getByToken(ak8jetsToken, ak8jetsHandle);
  if (!ak8jetsHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillAK8Jets: Error getting the ak8 jet collection from the event...");
  size_t n_objects = ak8jetsHandle->size();

  if (filledObjects) filledObjects->reserve(n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, mass, n_objects);

  //MAKE_VECTOR_WITH_RESERVE(bool, pass_looseId, n_objects);
  //MAKE_VECTOR_WITH_RESERVE(bool, pass_tightId, n_objects);

  MAKE_VECTOR_WITH_RESERVE(size_t, n_pfcands, n_objects);
  MAKE_VECTOR_WITH_RESERVE(size_t, n_mucands, n_objects);
  MAKE_VECTOR_WITH_RESERVE(size_t, n_softdrop_subjets, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, area, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pt_resolution, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, softdrop_pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_mass, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet0_pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet0_eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet0_phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet0_mass, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet1_pt, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet1_eta, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet1_phi, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, softdrop_subjet1_mass, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, ptDistribution, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, totalMultiplicity, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, axis1, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, axis2, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, JECNominal, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JECUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JECDn, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, JERNominal, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JERUp, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, JERDn, n_objects);

  MAKE_VECTOR_WITH_RESERVE(int, partonFlavour, n_objects);
  MAKE_VECTOR_WITH_RESERVE(int, hadronFlavour, n_objects);

  for (View<pat::Jet>::const_iterator obj = ak8jetsHandle->begin(); obj != ak8jetsHandle->end(); obj++){
    if (!AK8JetSelectionHelpers::testSkimAK8Jet(*obj, this->year)) continue;

    // Core particle quantities
    // These are the uncorrected momentum components!
    pt.push_back(obj->pt());
    eta.push_back(obj->eta());
    phi.push_back(obj->phi());
    mass.push_back(obj->mass());

    //pass_looseId.push_back(AK8JetSelectionHelpers::testLooseAK8Jet(*obj, this->year));
    //pass_tightId.push_back(AK8JetSelectionHelpers::testTightAK8Jet(*obj, this->year));

    PUSH_USERINT_INTO_VECTOR(n_pfcands);
    PUSH_USERINT_INTO_VECTOR(n_mucands);
    PUSH_USERINT_INTO_VECTOR(n_softdrop_subjets);

    PUSH_USERFLOAT_INTO_VECTOR(area);
    PUSH_USERFLOAT_INTO_VECTOR(pt_resolution);

    PUSH_USERFLOAT_INTO_VECTOR(softdrop_pt);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_eta);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_phi);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_mass);

    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet0_pt);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet0_eta);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet0_phi);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet0_mass);

    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet1_pt);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet1_eta);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet1_phi);
    PUSH_USERFLOAT_INTO_VECTOR(softdrop_subjet1_mass);

    PUSH_USERFLOAT_INTO_VECTOR(ptDistribution);
    PUSH_USERFLOAT_INTO_VECTOR(totalMultiplicity);
    PUSH_USERFLOAT_INTO_VECTOR(axis1);
    PUSH_USERFLOAT_INTO_VECTOR(axis2);

    PUSH_USERFLOAT_INTO_VECTOR(JECNominal);
    PUSH_USERFLOAT_INTO_VECTOR(JECUp);
    PUSH_USERFLOAT_INTO_VECTOR(JECDn);

    PUSH_USERFLOAT_INTO_VECTOR(JERNominal);
    PUSH_USERFLOAT_INTO_VECTOR(JERUp);
    PUSH_USERFLOAT_INTO_VECTOR(JERDn);

    PUSH_USERINT_INTO_VECTOR(partonFlavour);
    PUSH_USERINT_INTO_VECTOR(hadronFlavour);

    if (filledObjects) filledObjects->push_back(&(*obj));
  }

  // Pass collections to the communicator
  PUSH_VECTOR_WITH_NAME(colName, pt);
  PUSH_VECTOR_WITH_NAME(colName, eta);
  PUSH_VECTOR_WITH_NAME(colName, phi);
  PUSH_VECTOR_WITH_NAME(colName, mass);

  //PUSH_VECTOR_WITH_NAME(colName, pass_looseId);
  //PUSH_VECTOR_WITH_NAME(colName, pass_tightId);

  PUSH_VECTOR_WITH_NAME(colName, n_pfcands);
  PUSH_VECTOR_WITH_NAME(colName, n_mucands);
  PUSH_VECTOR_WITH_NAME(colName, n_softdrop_subjets);

  PUSH_VECTOR_WITH_NAME(colName, area);
  PUSH_VECTOR_WITH_NAME(colName, pt_resolution);

  // Disable softdrop for now
  /*
  PUSH_VECTOR_WITH_NAME(colName, softdrop_pt);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_eta);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_phi);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_mass);

  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet0_pt);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet0_eta);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet0_phi);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet0_mass);

  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet1_pt);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet1_eta);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet1_phi);
  PUSH_VECTOR_WITH_NAME(colName, softdrop_subjet1_mass);
  */

  PUSH_VECTOR_WITH_NAME(colName, ptDistribution);
  PUSH_VECTOR_WITH_NAME(colName, totalMultiplicity);
  PUSH_VECTOR_WITH_NAME(colName, axis1);
  PUSH_VECTOR_WITH_NAME(colName, axis2);

  PUSH_VECTOR_WITH_NAME(colName, JECNominal);
  PUSH_VECTOR_WITH_NAME(colName, JECUp);
  PUSH_VECTOR_WITH_NAME(colName, JECDn);

  PUSH_VECTOR_WITH_NAME(colName, JERNominal);
  PUSH_VECTOR_WITH_NAME(colName, JERUp);
  PUSH_VECTOR_WITH_NAME(colName, JERDn);

  PUSH_VECTOR_WITH_NAME(colName, partonFlavour);
  PUSH_VECTOR_WITH_NAME(colName, hadronFlavour);

  return n_objects;
}
size_t CMS3Ntuplizer::fillVertices(const edm::Event& iEvent, std::vector<reco::Vertex const*>* filledObjects){
  const char colName[] = "vtxs";
  edm::Handle< reco::VertexCollection > vtxHandle;
  iEvent.getByToken(vtxToken, vtxHandle);
  if (!vtxHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillVertices: Error getting the vertex collection from the event...");
  size_t n_objects = vtxHandle->size();

  if (filledObjects) filledObjects->reserve(n_objects);

  MAKE_VECTOR_WITH_RESERVE(bool, is_fake, n_objects);
  MAKE_VECTOR_WITH_RESERVE(bool, is_valid, n_objects);
  MAKE_VECTOR_WITH_RESERVE(bool, is_good, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, ndof, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pos_x, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pos_y, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pos_z, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pos_t, n_objects);

  MAKE_VECTOR_WITH_RESERVE(float, pos_dx, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pos_dy, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pos_dz, n_objects);
  MAKE_VECTOR_WITH_RESERVE(float, pos_dt, n_objects);

  bool didFirstVertex = false;
  bool didFirstGoodVertex = false;
  unsigned int nvtxs=0, nvtxs_good=0;
  for (reco::VertexCollection::const_iterator obj = vtxHandle->begin(); obj != vtxHandle->end(); obj++){
    bool isGoodVtx = VertexSelectionHelpers::testGoodVertex(*obj);

    // Avoid recording all vertices
    if (!didFirstVertex || (!didFirstGoodVertex && isGoodVtx)){
      auto const& pos = obj->position();

      is_fake.push_back(obj->isFake());
      is_valid.push_back(obj->isValid());
      is_good.push_back(isGoodVtx);

      ndof.push_back(obj->ndof());

      pos_x.push_back(pos.x());
      pos_y.push_back(pos.y());
      pos_z.push_back(pos.z());
      pos_t.push_back(obj->t());

      pos_dx.push_back(obj->xError());
      pos_dy.push_back(obj->yError());
      pos_dz.push_back(obj->zError());
      pos_dt.push_back(obj->tError());

      if (filledObjects) filledObjects->push_back(&(*obj));

      if (!didFirstVertex) didFirstVertex=true;
      if (isGoodVtx) didFirstGoodVertex=true;
    }

    if (isGoodVtx) nvtxs_good++;
    nvtxs++;
  }

  // Record the counts
  commonEntry.setNamedVal(TString(colName)+"_nvtxs", nvtxs);
  commonEntry.setNamedVal(TString(colName)+"_nvtxs_good", nvtxs_good);

  // Pass collections to the communicator
  PUSH_VECTOR_WITH_NAME(colName, is_fake);
  PUSH_VECTOR_WITH_NAME(colName, is_valid);
  PUSH_VECTOR_WITH_NAME(colName, is_good);

  PUSH_VECTOR_WITH_NAME(colName, ndof);

  PUSH_VECTOR_WITH_NAME(colName, pos_x);
  PUSH_VECTOR_WITH_NAME(colName, pos_y);
  PUSH_VECTOR_WITH_NAME(colName, pos_z);
  PUSH_VECTOR_WITH_NAME(colName, pos_t);

  PUSH_VECTOR_WITH_NAME(colName, pos_dx);
  PUSH_VECTOR_WITH_NAME(colName, pos_dy);
  PUSH_VECTOR_WITH_NAME(colName, pos_dz);
  PUSH_VECTOR_WITH_NAME(colName, pos_dt);

  return n_objects;
}

bool CMS3Ntuplizer::fillEventVariables(const edm::Event& iEvent){
  edm::Handle< double > rhoHandle;
  iEvent.getByToken(rhoToken, rhoHandle);
  if (!rhoHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillEventVariables: Error getting the rho collection from the event...");

  // Simple event-level variables
  commonEntry.setNamedVal("EventNumber", iEvent.id().event());
  commonEntry.setNamedVal("RunNumber", iEvent.id().run());
  commonEntry.setNamedVal("LuminosityBlock", iEvent.luminosityBlock());
  commonEntry.setNamedVal("event_rho", (float) (*rhoHandle));
  if (isMC){
    edm::Handle< std::vector<PileupSummaryInfo> > puInfoHandle;
    iEvent.getByToken(puInfoToken, puInfoHandle);
    if (!puInfoHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillEventVariables: Error getting the PU info from the event...");

    commonEntry.setNamedVal("n_vtxs_PU", (int) ((*puInfoHandle)[0].getPU_NumInteractions()));
    commonEntry.setNamedVal("n_true_int", (float) ((*puInfoHandle)[0].getTrueNumInteractions()));
  }
  else{
    commonEntry.setNamedVal("n_vtxs_PU", (int) -1);
    commonEntry.setNamedVal("n_true_int", (float) -1);
  }

  if (applyPrefiringWeights){
    edm::Handle< double > prefiringweight;
    float prefiringweightval=0;

    iEvent.getByToken(prefiringWeightToken, prefiringweight);
    if (!prefiringweight.isValid()) throw cms::Exception("CMS3Ntuplizer::fillEventVariables: Error getting the nominal prefiring weight from the event...");
    prefiringweightval = (*prefiringweight);
    commonEntry.setNamedVal("prefiringWeight_Nominal", prefiringweightval);

    iEvent.getByToken(prefiringWeightToken_Dn, prefiringweight);
    if (!prefiringweight.isValid()) throw cms::Exception("CMS3Ntuplizer::fillEventVariables: Error getting the prefiring weight down variation from the event...");
    prefiringweightval = (*prefiringweight);
    commonEntry.setNamedVal("prefiringWeight_Dn", prefiringweightval);

    iEvent.getByToken(prefiringWeightToken_Up, prefiringweight);
    if (!prefiringweight.isValid()) throw cms::Exception("CMS3Ntuplizer::fillEventVariables: Error getting the prefiring weight up variation from the event...");
    prefiringweightval = (*prefiringweight);
    commonEntry.setNamedVal("prefiringWeight_Up", prefiringweightval);
  }

  return true;
}
bool CMS3Ntuplizer::fillTriggerInfo(const edm::Event& iEvent){
  const char colName[] = "triggers";
  edm::Handle< edm::View<TriggerInfo> > triggerInfoHandle;
  iEvent.getByToken(triggerInfoToken, triggerInfoHandle);
  if (!triggerInfoHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillTriggerInfo: Error getting the trigger infos. from the event...");
  size_t n_triggers = triggerInfoHandle->size();

  MAKE_VECTOR_WITH_RESERVE(std::string, name, n_triggers);
  MAKE_VECTOR_WITH_RESERVE(bool, passTrigger, n_triggers);
  MAKE_VECTOR_WITH_RESERVE(int, L1prescale, n_triggers);
  MAKE_VECTOR_WITH_RESERVE(int, HLTprescale, n_triggers);

  bool passAtLeastOneTrigger = false;
  for (View<TriggerInfo>::const_iterator obj = triggerInfoHandle->begin(); obj != triggerInfoHandle->end(); obj++){
    name.emplace_back(obj->name);
    passTrigger.emplace_back(obj->passTrigger);
    L1prescale.emplace_back(obj->L1prescale);
    HLTprescale.emplace_back(obj->HLTprescale);

    passAtLeastOneTrigger |= obj->passTrigger;
  }

  PUSH_VECTOR_WITH_NAME(colName, name);
  PUSH_VECTOR_WITH_NAME(colName, passTrigger);
  PUSH_VECTOR_WITH_NAME(colName, L1prescale);
  PUSH_VECTOR_WITH_NAME(colName, HLTprescale);

  // If the (data) event does not pass any triggers, do not record it.
  return passAtLeastOneTrigger;
}
bool CMS3Ntuplizer::fillMETFilterVariables(const edm::Event& iEvent){
  // See https://twiki.cern.ch/twiki/bin/viewauth/CMS/MissingETOptionalFiltersRun2 for recommendations
  // See also PhysicsTools/PatAlgos/python/slimming/metFilterPaths_cff.py for the collection names
  const char metFiltCollName[] = "metfilter";
  edm::Handle<METFilterInfo> metFilterInfoHandle;
  iEvent.getByToken(metFilterInfoToken, metFilterInfoHandle);
  if (!metFilterInfoHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillMETFilterVariables: Error getting the MET filter handle from the event...");

  static const std::vector<std::string> metfilterflags{
    "CSCTightHaloFilter",
    "CSCTightHalo2015Filter",
    "globalTightHalo2016Filter",
    "globalSuperTightHalo2016Filter",
    "HBHENoiseFilter",
    "EcalDeadCellTriggerPrimitiveFilter",
    "hcalLaserEventFilter",
    "trackingFailureFilter",
    "chargedHadronTrackResolutionFilter",
    "eeBadScFilter",
    "ecalLaserCorrFilter",
    "METFilters",
    "goodVertices",
    "trkPOGFilters",
    "trkPOG_logErrorTooManyClusters",
    "trkPOG_manystripclus53X",
    "trkPOG_toomanystripclus53X",
    "HBHENoiseIsoFilter",
    "CSCTightHaloTrkMuUnvetoFilter",
    "HcalStripHaloFilter",
    "EcalDeadCellBoundaryEnergyFilter",
    "muonBadTrackFilter",
    "BadPFMuonFilter",
    "BadChargedCandidateFilter",
    "ecalBadCalibFilter",
    "ecalBadCalibFilterUpdated"
  };
  for (auto const& flagname:metfilterflags){
    bool flag = false;
    auto it_flag = metFilterInfoHandle->flag_accept_map.find(flagname);
    if (it_flag!=metFilterInfoHandle->flag_accept_map.cend()) flag = it_flag->second;
    commonEntry.setNamedVal((std::string(metFiltCollName) + "_" + flagname).data(), flag);
  }

  return true;
}
bool CMS3Ntuplizer::fillMETVariables(const edm::Event& iEvent){
#define SET_MET_VARIABLE(HANDLE, NAME, COLLNAME) commonEntry.setNamedVal((std::string(COLLNAME) + "_" + #NAME).data(), HANDLE->NAME);

  edm::Handle<METInfo> metHandle;

  // PF MET
  const char pfmetCollName[] = "pfmet";
  iEvent.getByToken(pfmetToken, metHandle);
  if (!metHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillMETVariables: Error getting the PF MET handle from the event...");

  SET_MET_VARIABLE(metHandle, met_Nominal, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_Nominal, pfmetCollName);
  SET_MET_VARIABLE(metHandle, sumEt_Nominal, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metSignificance, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_over_sqrtSumEt, pfmetCollName);

  SET_MET_VARIABLE(metHandle, met_Raw, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_Raw, pfmetCollName);
  SET_MET_VARIABLE(metHandle, sumEt_Raw, pfmetCollName);

  SET_MET_VARIABLE(metHandle, met_JERUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JERUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_JERDn, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JERDn, pfmetCollName);

  SET_MET_VARIABLE(metHandle, met_JECUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JECUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_JECDn, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JECDn, pfmetCollName);

  /*
  SET_MET_VARIABLE(metHandle, met_MuonEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_MuonEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_MuonEnDn, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_MuonEnDn, pfmetCollName);

  SET_MET_VARIABLE(metHandle, met_ElectronEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_ElectronEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_ElectronEnDn, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_ElectronEnDn, pfmetCollName);

  SET_MET_VARIABLE(metHandle, met_TauEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_TauEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_TauEnDn, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_TauEnDn, pfmetCollName);

  SET_MET_VARIABLE(metHandle, met_UnclusteredEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_UnclusteredEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_UnclusteredEnDn, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_UnclusteredEnDn, pfmetCollName);

  SET_MET_VARIABLE(metHandle, met_PhotonEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_PhotonEnUp, pfmetCollName);
  SET_MET_VARIABLE(metHandle, met_PhotonEnDn, pfmetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_PhotonEnDn, pfmetCollName);
  */

  SET_MET_VARIABLE(metHandle, calo_met, pfmetCollName);
  SET_MET_VARIABLE(metHandle, calo_metPhi, pfmetCollName);

  /*
  SET_MET_VARIABLE(metHandle, gen_met, pfmetCollName);
  SET_MET_VARIABLE(metHandle, gen_metPhi, pfmetCollName);
  */

  // PUPPI MET
  const char puppimetCollName[] = "puppimet";
  iEvent.getByToken(puppimetToken, metHandle);
  if (!metHandle.isValid()) throw cms::Exception("CMS3Ntuplizer::fillMETVariables: Error getting the PUPPI MET handle from the event...");

  SET_MET_VARIABLE(metHandle, met_Nominal, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_Nominal, puppimetCollName);
  SET_MET_VARIABLE(metHandle, sumEt_Nominal, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metSignificance, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_over_sqrtSumEt, puppimetCollName);

  SET_MET_VARIABLE(metHandle, met_Raw, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_Raw, puppimetCollName);
  SET_MET_VARIABLE(metHandle, sumEt_Raw, puppimetCollName);

  SET_MET_VARIABLE(metHandle, met_JERUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JERUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_JERDn, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JERDn, puppimetCollName);

  SET_MET_VARIABLE(metHandle, met_JECUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JECUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_JECDn, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_JECDn, puppimetCollName);

  /*
  SET_MET_VARIABLE(metHandle, met_MuonEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_MuonEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_MuonEnDn, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_MuonEnDn, puppimetCollName);

  SET_MET_VARIABLE(metHandle, met_ElectronEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_ElectronEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_ElectronEnDn, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_ElectronEnDn, puppimetCollName);

  SET_MET_VARIABLE(metHandle, met_TauEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_TauEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_TauEnDn, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_TauEnDn, puppimetCollName);

  SET_MET_VARIABLE(metHandle, met_UnclusteredEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_UnclusteredEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_UnclusteredEnDn, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_UnclusteredEnDn, puppimetCollName);

  SET_MET_VARIABLE(metHandle, met_PhotonEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_PhotonEnUp, puppimetCollName);
  SET_MET_VARIABLE(metHandle, met_PhotonEnDn, puppimetCollName);
  SET_MET_VARIABLE(metHandle, metPhi_PhotonEnDn, puppimetCollName);
  */

  /*
  SET_MET_VARIABLE(metHandle, calo_met, puppimetCollName);
  SET_MET_VARIABLE(metHandle, calo_metPhi, puppimetCollName);

  SET_MET_VARIABLE(metHandle, gen_met, puppimetCollName);
  SET_MET_VARIABLE(metHandle, gen_metPhi, puppimetCollName);
  */

#undef SET_MET_VARIABLE

  return true;
}

bool CMS3Ntuplizer::fillGenVariables(
  const edm::Event& iEvent,
  std::vector<reco::GenParticle const*>* filledPrunedGenParts,
  std::vector<pat::PackedGenParticle const*>* filledPackedGenParts,
  std::vector<reco::GenJet const*>* filledGenAK4Jets,
  std::vector<reco::GenJet const*>* filledGenAK8Jets
){
  if (!this->isMC) return true;

  // Gen. info.
  recordGenInfo(iEvent);

  // Gen. particles
  recordGenParticles(iEvent, filledPrunedGenParts, filledPackedGenParts);

  // Gen. jets
  recordGenJets(iEvent, false, filledGenAK4Jets); // ak4jets
  recordGenJets(iEvent, true, filledGenAK8Jets); // ak8jets

  return true;
}


// Undefine the convenience macros
#undef PUSH_VECTOR_WITH_NAME
#undef PUSH_USERFLOAT_INTO_VECTOR
#undef PUSH_USERINT_INTO_VECTOR
#undef MAKE_VECTOR_WITH_RESERVE


DEFINE_FWK_MODULE(CMS3Ntuplizer);
