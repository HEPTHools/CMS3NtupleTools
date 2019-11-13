#ifndef METINFO_H
#define METINFO_H

#include <string>
#include <unordered_map>


struct METInfo{
  float met;
  float metPhi;
  float sumEt;
  float metSignificance;
  float met_over_sqrtSumEt;

  float met_raw;
  float metPhi_raw;
  float sumEt_raw;

  float met_JERUp;
  float metPhi_JERUp;
  float met_JERDn;
  float metPhi_JERDn;

  float met_JECUp;
  float metPhi_JECUp;
  float met_JECDn;
  float metPhi_JECDn;

  float met_MuonEnUp;
  float metPhi_MuonEnUp;
  float met_MuonEnDn;
  float metPhi_MuonEnDn;

  float met_ElectronEnUp;
  float metPhi_ElectronEnUp;
  float met_ElectronEnDn;
  float metPhi_ElectronEnDn;

  float met_TauEnUp;
  float metPhi_TauEnUp;
  float met_TauEnDn;
  float metPhi_TauEnDn;

  float met_UnclusteredEnUp;
  float metPhi_UnclusteredEnUp;
  float met_UnclusteredEnDn;
  float metPhi_UnclusteredEnDn;

  float met_PhotonEnUp;
  float metPhi_PhotonEnUp;
  float met_PhotonEnDn;
  float metPhi_PhotonEnDn;

  float calo_met;
  float calo_metPhi;

  float gen_met;
  float gen_metPhi;


  METInfo();
  METInfo(const METInfo&);

};


#endif
