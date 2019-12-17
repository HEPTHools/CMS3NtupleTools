#ifndef GENINFOHANDLER_H
#define GENINFOHANDLER_H

#include <vector>
#include "IvyBase.h"
#include "GenInfoObject.h"
#include "SystematicVariations.h"


class GenInfoHandler : public IvyBase{
protected:
  std::unordered_map< BaseTree*, std::vector<TString> > tree_MElist_map;
  std::unordered_map< BaseTree*, bool > tree_lheparticles_present_map;

  bool acquireCoreGenInfo;
  bool acquireLHEMEWeights;
  bool acquireLHEParticles;

  GenInfoObject* genInfo;

  void clear(){ delete genInfo; genInfo=nullptr; }

public:
  GenInfoHandler();
  ~GenInfoHandler(){ clear(); }

  bool constructGenInfo(SystematicsHelpers::SystematicVariationTypes const& syst);
  GenInfoObject* const& getGenInfo() const{ return genInfo; }

  void setAcquireCoreGenInfo(bool flag){ acquireCoreGenInfo=flag; }
  void setAcquireLHEMEWeights(bool flag){ acquireLHEMEWeights=flag; }
  void setAcquireLHEParticles(bool flag){ acquireLHEParticles=flag; }

  void bookBranches(BaseTree* tree);

};


#endif
