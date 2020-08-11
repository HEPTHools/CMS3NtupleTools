#include <cassert>
#include <cmath>
#include "ElectronSelectionHelpers.h"
#include "HelperFunctions.h"
#include "MELAStreamHelpers.hh"


// These are functions hidden from the user
namespace ElectronSelectionHelpers{
  std::vector<ElectronTriggerCutEnums::ElectronTriggerCutTypes> electron_triggeremulationV1_bits;
  std::vector<ElectronTriggerCutEnums::ElectronTriggerCutTypes> electron_triggeremulationV2_bits;

  bool testPtEtaGen(ElectronObject const& part);

  bool testVetoId(ElectronObject const& part);
  bool testVetoIso(ElectronObject const& part);
  bool testVetoKin(ElectronObject const& part);

  bool testLooseId(ElectronObject const& part);
  bool testLooseIso(ElectronObject const& part);
  bool testLooseKin(ElectronObject const& part);

  bool testMediumId(ElectronObject const& part);
  bool testMediumIso(ElectronObject const& part);
  bool testMediumKin(ElectronObject const& part);

  bool testTightId(ElectronObject const& part);
  bool testTightIso(ElectronObject const& part);
  bool testTightKin(ElectronObject const& part);

  bool testProbeId(ElectronObject const& part);

  bool testFakeableBaseIso(ElectronObject const& part);
  bool testFakeableBase(ElectronObject const& part);
  bool testFakeable(ElectronObject const& part);

  bool testPreselectionVeto(ElectronObject const& part);
  bool testPreselectionLoose(ElectronObject const& part);
  bool testPreselectionTight(ElectronObject const& part);
}


using namespace std;
using namespace MELAStreamHelpers;


void ElectronSelectionHelpers::clearTriggerEmulationBits(){ electron_triggeremulationV1_bits.clear(); electron_triggeremulationV2_bits.clear(); }
void ElectronSelectionHelpers::setTriggerEmulationV1Bits(std::vector<ElectronTriggerCutEnums::ElectronTriggerCutTypes> const& bitlist){ electron_triggeremulationV1_bits = bitlist; }
void ElectronSelectionHelpers::setTriggerEmulationV2Bits(std::vector<ElectronTriggerCutEnums::ElectronTriggerCutTypes> const& bitlist){ electron_triggeremulationV2_bits = bitlist; }

float ElectronSelectionHelpers::getIsolationDRmax(ElectronObject const& part){
  if (isoType_preselection == kPFIsoDR0p3) return 0.3;
  else if (isoType_preselection == kPFIsoDR0p4) return 0.4;
  else if (isoType_preselection == kMiniIso) return (10. / std::min(std::max(part.pt()/part.currentSystScale, 50.), 200.));
  else{
    MELAerr << "ElectronSelectionHelpers::getIsolationDRmax: Isolation type " << isoType_preselection << " is not implemented." << endl;
    assert(0);
    return -1;
  }
}

float ElectronSelectionHelpers::absMiniIso(ElectronObject const& part){ return part.extras.miniIso_comb_nofsr; }
float ElectronSelectionHelpers::relMiniIso(ElectronObject const& part){ float pt = part.pt(); return (pt>0. ? absMiniIso(part)/pt : 0.f); }

float ElectronSelectionHelpers::absPFIso_DR0p3(ElectronObject const& part){ return part.extras.pfIso03_comb_nofsr; }
float ElectronSelectionHelpers::relPFIso_DR0p3(ElectronObject const& part){ float pt = part.pt(); return (pt>0. ? absPFIso_DR0p3(part)/pt : 0.f); }

float ElectronSelectionHelpers::absPFIso_DR0p4(ElectronObject const& part){ return part.extras.pfIso04_comb_nofsr; }
float ElectronSelectionHelpers::relPFIso_DR0p4(ElectronObject const& part){ float pt = part.pt(); return (pt>0. ? absPFIso_DR0p4(part)/pt : 0.f); }

float ElectronSelectionHelpers::computeIso(ElectronObject const& part){
  // If id is an MVA id with iso., return lowest possible iso;
  // otherwise, return the output of the corresponding function.
  if (
    idType_preselection == kMVAId_Fall17V2_Iso
    ||
    idType_preselection == kMVAId_HZZRun2Legacy_Iso
    ) return 0.f;
  else if (isoType_preselection == kPFIsoDR0p3) return relPFIso_DR0p3(part);
  else if (isoType_preselection == kPFIsoDR0p4) return relPFIso_DR0p4(part);
  else if (isoType_preselection == kMiniIso) return relMiniIso(part);
  else MELAerr << "ElectronSelectionHelpers::computeIso: Isolation " << isoType_preselection << " with id " << idType_preselection << " is not implemented." << endl;
  return 999.f;
}

/*
From https://twiki.cern.ch/twiki/bin/view/CMS/CutBasedElectronIdentificationRun2#Applying_Individual_Cuts_of_a_Se
For cut-based selection, the bit map is the following:
0: Min. pT cut
1: SC eta multi. range
2: dEtaIn seed
3: dPhiIn
4: Full 5x5 sigmaIetaIeta
5: H/E
6: 1/E - 1/p
7: Eff. area PF iso.
8: Conversion veto
9: Missing hits
*/
#define TEST_CUTBASED_BIT(ibit) (ibit == 1023 || ibit == 895)
bool ElectronSelectionHelpers::testVetoId(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Veto_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Veto_Bits);
  case kMVAId_Fall17V2_NoIso:
    return part.extras.id_MVA_Fall17V2_NoIso_pass_wpLoose;
  case kMVAId_Fall17V2_Iso:
    return part.extras.id_MVA_Fall17V2_Iso_pass_wpLoose;
  case kMVAId_HZZRun2Legacy_Iso:
    return part.extras.id_MVA_HZZRun2Legacy_Iso_pass_wpHZZ;
  default:
    MELAerr << "ElectronSelectionHelpers::testVetoId: Id " << idType_preselection << " is not implemented!" << endl;
    assert(0);
    return false;
  };
}
bool ElectronSelectionHelpers::testLooseId(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Loose_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Loose_Bits);
  case kMVAId_Fall17V2_NoIso:
    return part.extras.id_MVA_Fall17V2_NoIso_pass_wpLoose;
  case kMVAId_Fall17V2_Iso:
    return part.extras.id_MVA_Fall17V2_Iso_pass_wpLoose;
  case kMVAId_HZZRun2Legacy_Iso:
    return part.extras.id_MVA_HZZRun2Legacy_Iso_pass_wpHZZ;
  default:
    MELAerr << "ElectronSelectionHelpers::testLooseId: Id " << idType_preselection << " is not implemented!" << endl;
    assert(0);
    return false;
  };
}
bool ElectronSelectionHelpers::testMediumId(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Medium_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Medium_Bits);
  case kMVAId_Fall17V2_NoIso:
    return part.extras.id_MVA_Fall17V2_NoIso_pass_wp90;
  case kMVAId_Fall17V2_Iso:
    return part.extras.id_MVA_Fall17V2_Iso_pass_wp90;
  case kMVAId_HZZRun2Legacy_Iso:
    return part.extras.id_MVA_HZZRun2Legacy_Iso_pass_wpHZZ;
  default:
    MELAerr << "ElectronSelectionHelpers::testMediumId: Id " << idType_preselection << " is not implemented!" << endl;
    assert(0);
    return false;
  };
}
bool ElectronSelectionHelpers::testTightId(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Tight_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Tight_Bits);
  case kMVAId_Fall17V2_NoIso:
    return part.extras.id_MVA_Fall17V2_NoIso_pass_wp80;
  case kMVAId_Fall17V2_Iso:
    return part.extras.id_MVA_Fall17V2_Iso_pass_wp80;
  case kMVAId_HZZRun2Legacy_Iso:
    return part.extras.id_MVA_HZZRun2Legacy_Iso_pass_wpHZZ;
  default:
    MELAerr << "ElectronSelectionHelpers::testTightId: Id " << idType_preselection << " is not implemented!" << endl;
    assert(0);
    return false;
  };
}
#undef TEST_CUTBASED_BIT

#define TEST_CUTBASED_BIT(ibit) HelperFunctions::test_bit(ibit, 7)
bool ElectronSelectionHelpers::testVetoIso(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Veto_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Veto_Bits);
  case kMVAId_Fall17V2_Iso:
  case kMVAId_HZZRun2Legacy_Iso:
    return true;
  default:
    return (computeIso(part)<isoThr_veto);
  };
}
bool ElectronSelectionHelpers::testLooseIso(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Loose_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Loose_Bits);
  case kMVAId_Fall17V2_Iso:
  case kMVAId_HZZRun2Legacy_Iso:
    return true;
  default:
    return (computeIso(part)<isoThr_loose);
  };
}
bool ElectronSelectionHelpers::testMediumIso(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Medium_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Medium_Bits);
  case kMVAId_Fall17V2_Iso:
  case kMVAId_HZZRun2Legacy_Iso:
    return true;
  default:
    return (computeIso(part)<isoThr_medium);
  };
}
bool ElectronSelectionHelpers::testTightIso(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Tight_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Tight_Bits);
  case kMVAId_Fall17V2_Iso:
  case kMVAId_HZZRun2Legacy_Iso:
    return true;
  default:
    return (computeIso(part)<isoThr_tight);
  };
}
bool ElectronSelectionHelpers::testFakeableBaseIso(ElectronObject const& part){
  switch (idType_preselection){
#if ELECTRONS_HAVE_FALL17V1_CUTBASED == 1
  case kCutBasedId_Fall17V1:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V1_Loose_Bits);
#endif
  case kCutBasedId_Fall17V2:
    return TEST_CUTBASED_BIT(part.extras.id_cutBased_Fall17V2_Loose_Bits);
  case kMVAId_Fall17V2_Iso:
  case kMVAId_HZZRun2Legacy_Iso:
    // FIXME: Fakeable id/iso combination needs to be reimplemented
    return true;
  default:
    return (computeIso(part)<isoThr_fakeable);
  };
}
#undef TEST_CUTBASED_BIT

bool ElectronSelectionHelpers::testVetoKin(ElectronObject const& part){
  return (part.pt()>=ptThr_skim_veto && std::abs(part.eta())<etaThr_skim_veto);
}
bool ElectronSelectionHelpers::testLooseKin(ElectronObject const& part){
  return (part.pt()>=ptThr_skim_loose && std::abs(part.eta())<etaThr_skim_loose);
}
bool ElectronSelectionHelpers::testMediumKin(ElectronObject const& part){
  return (part.pt()>=ptThr_skim_medium && std::abs(part.eta())<etaThr_skim_medium);
}
bool ElectronSelectionHelpers::testTightKin(ElectronObject const& part){
  return (part.pt()>=ptThr_skim_tight && std::abs(part.eta())<etaThr_skim_tight);
}

bool ElectronSelectionHelpers::testProbeId(ElectronObject const& part){
  return part.extras.ecalEnergy*part.extras.sinTheta_SC_pos>=ptThr_skim_loose;
}

bool ElectronSelectionHelpers::testPtEtaGen(ElectronObject const& part){
  return (part.pt()>=ptThr_gen && std::abs(part.eta())<etaThr_gen);
}
bool ElectronSelectionHelpers::testFakeableBase(ElectronObject const& part){
  return (
    part.testSelectionBit(bit_preselectionTight_id)
    &&
    part.testSelectionBit(kFakeableBaseIso)
    &&
    part.testSelectionBit(bit_preselectionTight_kin)
    );
}
bool ElectronSelectionHelpers::testFakeable(ElectronObject const& part){
  bool res = false;
  if (part.testSelectionBit(kFakeableBase)){
    auto const& bits_V1 = part.extras.id_cutBased_triggerEmulationV1_Bits;
    auto const& bits_V2 = part.extras.id_cutBased_triggerEmulationV2_Bits;
    for (auto const& bit:electron_triggeremulationV1_bits) res |= HelperFunctions::test_bit(bits_V1, bit);
    for (auto const& bit:electron_triggeremulationV2_bits) res |= HelperFunctions::test_bit(bits_V2, bit);
  }
  return res;
}
bool ElectronSelectionHelpers::testPreselectionVeto(ElectronObject const& part){
  return (
    part.testSelectionBit(bit_preselectionVeto_id)
    &&
    part.testSelectionBit(bit_preselectionVeto_iso)
    &&
    part.testSelectionBit(bit_preselectionVeto_kin)
    );
}
bool ElectronSelectionHelpers::testPreselectionLoose(ElectronObject const& part){
  return (
    part.testSelectionBit(bit_preselectionLoose_id)
    &&
    part.testSelectionBit(bit_preselectionLoose_iso)
    &&
    part.testSelectionBit(bit_preselectionLoose_kin)
    );
}
bool ElectronSelectionHelpers::testPreselectionTight(ElectronObject const& part){
  return (
    part.testSelectionBit(bit_preselectionTight_id)
    &&
    part.testSelectionBit(bit_preselectionTight_iso)
    &&
    part.testSelectionBit(bit_preselectionTight_kin)
    );
}
void ElectronSelectionHelpers::setSelectionBits(ElectronObject& part){
  static_assert(std::numeric_limits<unsigned long long>::digits >= nSelectionBits);

  part.setSelectionBit(kGenPtEta, testPtEtaGen(part));

  part.setSelectionBit(kVetoId, testVetoId(part));
  part.setSelectionBit(kVetoIso, testVetoIso(part));
  part.setSelectionBit(kVetoKin, testVetoKin(part));

  part.setSelectionBit(kLooseId, testLooseId(part));
  part.setSelectionBit(kLooseIso, testLooseIso(part));
  part.setSelectionBit(kLooseKin, testLooseKin(part));

  part.setSelectionBit(kMediumId, testMediumId(part));
  part.setSelectionBit(kMediumIso, testMediumIso(part));
  part.setSelectionBit(kMediumKin, testMediumKin(part));

  part.setSelectionBit(kTightId, testTightId(part));
  part.setSelectionBit(kTightIso, testTightIso(part));
  part.setSelectionBit(kTightKin, testTightKin(part));

  part.setSelectionBit(kProbeId, testProbeId(part));

  part.setSelectionBit(kFakeableBaseIso, testFakeableBaseIso(part));

  // The functions below test the bits set in the steps above.
  part.setSelectionBit(kFakeableBase, testFakeableBase(part));
  part.setSelectionBit(kFakeable, testFakeable(part));
  part.setSelectionBit(kPreselectionVeto, testPreselectionVeto(part));
  part.setSelectionBit(kPreselectionLoose, testPreselectionLoose(part));
  part.setSelectionBit(kPreselectionTight, testPreselectionTight(part));
}
